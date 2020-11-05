----------------------------------------------------------------------------------
-- Engineer:            David Banks & Ian Bradbury
--
-- Create Date:         15/7/2018
-- Module Name:         RGBtoHDMI CPLD
-- Project Name:        RGBtoHDMI
-- Target Devices:      XC9572XL
--
-- Version:             1.0
--
----------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity RGBtoHDMI is
    Port (
        -- From RGB Connector
        R3_I:      in    std_logic;
        G3_I:      in    std_logic;
        B3_I:      in    std_logic;
        R2_I:      in    std_logic;
        G2_I:      in    std_logic;
        B2_I:      in    std_logic;
        R1_I:      in    std_logic;
        G1_I:      in    std_logic;
        B1_I:      in    std_logic;
        R0_I:      in    std_logic;
        G0_I:      in    std_logic;
        B0_I:      in    std_logic;

        csync_I:   in    std_logic;
        vsync_I:   in    std_logic;

        analog:    inout std_logic;

        -- From Pi
        clk:       in    std_logic;
        sp_clk:    in    std_logic;
        sp_clken:  in    std_logic;
        sp_data:   in    std_logic;

        -- To PI GPIO
        quad:      out   std_logic_vector(11 downto 0);
        psync:     out   std_logic;
        csync:     out   std_logic;

        -- User interface
        version:   in    std_logic
    );
end RGBtoHDMI;

architecture Behavorial of RGBtoHDMI is

    -- Version number: Design_Major_Minor
    -- Design: 0 = BBC CPLD
    --         1 = Alternative CPLD
    --         2 = Atom CPLD
    --         3 = six bit CPLD (if required);
    --         4 = RGB CPLD (TTL)
    --         C = RGB CPLD (Analog)
    constant VERSION_NUM_RGB_TTL    : std_logic_vector(11 downto 0) := x"492";
    constant VERSION_NUM_RGB_ANALOG : std_logic_vector(11 downto 0) := x"C92";

    signal shift_R  : std_logic_vector(3 downto 0);
    signal shift_G  : std_logic_vector(3 downto 0);
    signal shift_B  : std_logic_vector(3 downto 0);

    signal csync1   : std_logic;
    signal csync2   : std_logic;
    signal last     : std_logic;

    signal csync_counter : unsigned(1 downto 0);

    -- The sampling counter runs at 96MHz
    -- - In modes 0..6 it is 6x  the pixel clock
    -- - In mode 7 it is 8x the pixel clock
    --
    -- It serves several purposes:
    -- 1. Counts the 12us between the rising edge of nCSYNC and the first pixel
    -- 2. Counts within each pixel (bits 0, 1, 2)
    -- 3. Counts counts pixels within a quad pixel (bits 3 and 4)
    -- 4. Handles double buffering of alternative quad pixels (bit 5)
    --
    -- At the moment we don't count pixels with the line, the Pi does that

    subtype counter_type is unsigned(8 downto 0);

    signal counter  : counter_type;

    -- Sample pixel on next clock; pipelined to reduce the number of product terms
    signal sample   : std_logic;

    -- New sample available, toggle psync on next cycle
    signal toggle          : std_logic;
    signal sample_toggle   : std_logic;

    -- RGB Input Mux
    signal R3       :std_logic;
    signal G3       :std_logic;
    signal B3       :std_logic;
    signal R2       :std_logic;
    signal G2       :std_logic;
    signal B2       :std_logic;

    signal mux_sync     : std_logic;
    signal clamp_pulse  : std_logic;
    signal clamp_enable : std_logic;

    -- Sample point register;
    --
    -- In Mode 7 each pixel lasts 8 clocks (96MHz / 12MHz). The original
    -- pixel clock is a regenerated 6Mhz clock, and both edges are used.
    -- Due to the way it is generated, there are three distinct phases,
    -- each with different rising/falling edge speeds, hence six sampling
    -- points are used.
    --
    -- In Modes 0..6 each pixel lasts 6 clocks (96MHz / 16MHz). The original
    -- pixel clock is a clean 16Mhz clock, so only one sample point is needed.
    -- To achieve this, all six values are set to be the same. This minimises
    -- the logic in the CPLD.

     -- Sampling points
    constant INIT_SAMPLING_POINTS : std_logic_vector(14 downto 0) := "000000000000000";
    signal sp_reg   : std_logic_vector(14 downto 0) := INIT_SAMPLING_POINTS;

    -- Break out of sp_reg
    signal offset     : std_logic_vector(3 downto 0);
    signal delay      : unsigned(2 downto 0);
    signal rate       : std_logic_vector(1 downto 0);
    signal rateswitch : std_logic;
    signal invert     : std_logic;
    signal divider    : unsigned(2 downto 0);
    signal mux        : std_logic;

begin
    offset     <= sp_reg(3 downto 0);
    delay      <= unsigned(sp_reg(6 downto 4));
    rate       <= sp_reg(8 downto 7);
    rateswitch <= sp_reg(9);                -- have to do it this way as 3 bit rate won't fit
    invert     <= sp_reg(10);
    divider    <= unsigned(sp_reg(13 downto 11));
    mux        <= sp_reg(14);

    mux_sync <= vsync_I when mux = '1' and version = '1' else csync_I;

    -- sp_data is overloaded as clamp off/on or 4lvl/6x2 or multiplex off/on depending on rate
    -- rate = 00 and rateswitch = '1' is 1 bit capture with sp_data 0/1 = clamp off/on
    -- rate = 00 and rateswitch = '0' is 3 bit capture with sp_data 0/1 = clamp off/on
    -- rate = 01 is 6 bit capture with sp_data 0/1 = clamp off/on
    -- rate = 10 and sp_data = 0 is 6 bit capture with 3 bit to 4 level encoding (clamp not usable in 4 level mode)
    -- rate = 10 and sp_data = 1 is 6 bit capture with multiplex enabled for 12 bit capture
    -- rate = 11 and sp_data = 0 is 9 bit capture using vsync_I as replacement G1 bit (will work on 8 bit board)
    -- rate = 11 and sp_data = 1 is 12 bit capture

    G3 <= G2_I when vsync_I = '1' else G3_I;
    G2 <= G3_I when vsync_I = '1' else G2_I;

    B3 <= B2_I when B1_I = '1' else B3_I;
    B2 <= B3_I when B1_I = '1' else B2_I;

    R3 <= R2_I when R1_I = '1' else R3_I;
    R2 <= R3_I when R1_I = '1' else R2_I;

    -- Shift the bits in LSB first
    process(sp_clk)
    begin
        if rising_edge(sp_clk) then
            if sp_clken = '1' then
                sp_reg <= sp_data & sp_reg(sp_reg'left downto sp_reg'right + 1);
            end if;
        end if;
    end process;

    process(clk)
    begin
        if rising_edge(clk) then

            -- synchronize CSYNC to the sampling clock
            -- if link fitted sync is inverted. If +ve vsync connected to link & +ve hsync to S then generate -ve composite sync
            csync1 <= mux_sync xor invert;

            -- De-glitch CSYNC
            --    csync1 is the possibly glitchy input
            --    csync2 is the filtered output
            if csync1 = csync2 then
                -- output same as input, reset the counter
                csync_counter <= to_unsigned(0, csync_counter'length);
            else
                -- output different to input
                csync_counter <= csync_counter + 1;
                -- if the difference lasts for N-1 cycles, update the output
                if csync_counter = 3 then
                    csync2 <= csync1;
                end if;
            end if;

            -- Counter is used to find sampling point for first pixel
            last <= csync2;
            -- reset counter on the rising edge of csync
            if last = '0' and csync2 = '1' then
                    if rateswitch = '1' then
                        counter(8 downto 4) <= "10" & delay; -- 3 low bits of delay with 1bpp so 10xxx
                    else
                        counter(8 downto 4) <= "11" & delay; -- only 2 low bits of delay used unless 1bpp so 110xx
                    end if;
                counter(3 downto 0) <= "0000";
            elsif divider = "000" then
                if counter(3 downto 0) /= 2 then
                    if counter(counter'left) = '1' then
                        counter <= counter + 1;
                    else
                        counter(counter'left - 1 downto 0) <= counter(counter'left - 1 downto 0) + 1;
                    end if;
                else
                    if counter(counter'left) = '1' then
                        counter <= counter + 14;
                    else
                        counter(counter'left - 1 downto 0) <= counter(counter'left - 1 downto 0) + 14;
                    end if;
                end if;
            elsif counter(3 downto 0) /= (divider & '1') then
                if counter(counter'left) = '1' then
                    counter <= counter + 1;
                else
                    counter(counter'left - 1 downto 0) <= counter(counter'left - 1 downto 0) + 1;
                end if;
            elsif counter(counter'left) = '1' then
                    counter <= counter + (not(divider) & '1');
                else
                    counter(counter'left - 1 downto 0) <= counter(counter'left - 1 downto 0) + (not(divider) & '1');
                end if;

                -- sample/shift control
            if counter(counter'left) = '0' and counter(3 downto 0) = unsigned(offset) then
                sample <= '1';
            else
                sample <= '0';
            end if;

            if counter(counter'left) = '1' then
                sample_toggle <= '0';
            elsif sample = '1' then
                sample_toggle <= not (sample_toggle);
            end if;

            -- R Sample/shift register
            if sample = '1' then
                if rate = "00" and rateswitch = '1' then
                    if sample_toggle = '0' then
                        shift_R <= G3_I & shift_R(3 downto 1);        -- 1 bpp
                    else
                        shift_R <= shift_R;
                    end if;
                elsif rate = "11" and sp_data = '1' then
                    shift_R <= R1_I & G2_I & B3_I & B0_I;             -- 12 bpp
                elsif rate = "11" and sp_data = '0'  then
                    shift_R <= R1_I & G2_I & B3_I & B3_I;             -- 9 bpp
                elsif rate = "10" and rateswitch = '1' then
                    shift_R <= shift_B(3) & G2_I & B3_I & shift_B(0); -- 6x2 multiplex 12 bpp
                elsif rate = "10" and rateswitch = '0' then
                    shift_R <= R2 & R3 & shift_R(3 downto 2);         -- 6 bpp 4 level
                elsif rate = "01" then
                    shift_R <= R2_I & R3_I & shift_R(3 downto 2);     -- 6 bpp
                else
                    shift_R <= R3_I & shift_R(3 downto 1);            -- 3 bpp
                end if;
            end if;


            -- G Sample/shift register
            if sample = '1' then
                if rate = "11" and sp_data = '1' then
                    shift_G <= R2_I & G3_I & G0_I & B1_I;             -- 12 bpp
                elsif rate = "11" and sp_data = '0' then
                    shift_G <= R2_I & G3_I & G3_I & B1_I;             -- 9 bpp
                elsif rate = "10" and rateswitch = '1' then
                    shift_G <= R2_I & G3_I & shift_R(2) & shift_R(1); -- 6x2 multiplex 12 bpp
                elsif rate = "10" and rateswitch = '0' then
                    shift_G <= G2 & G3 & shift_G(3 downto 2);         -- 6 bpp 4 level
                elsif rate = "01" then
                    shift_G <= G2_I & G3_I & shift_G(3 downto 2);     -- 6 bpp
                else
                    shift_G <= G3_I & shift_G(3 downto 1);            -- 3 bpp and 1bpp  (bit not used for 1bpp)
                end if;
            end if;

            -- B Sample/shift register
             if sample = '1' then
                if rate = "00" and rateswitch = '1'  then
                    if sample_toggle = '1' then
                       shift_B <= G3_I & shift_B(3 downto 1);         -- 1 bpp
                    else
                       shift_B <= shift_B;
                    end if;
                elsif rate = "11"  and sp_data = '1' then
                    shift_B <= R3_I & R0_I & G1_I & B2_I;             -- 12 bpp
                elsif rate = "11"  and sp_data = '0' then
                    shift_B <= R3_I & R3_I & vsync_I & B2_I;          -- 9 bpp with G1 on vsync_I
                elsif rate = "10" and rateswitch = '1' then
                    shift_B <= R3_I & shift_G(3) & shift_G(2) & B2_I; -- 6x2 multiplex 12 bpp
                elsif rate = "10" and rateswitch = '0' then
                    shift_B <= B2 & B3 & shift_B(3 downto 2);         -- 6 bpp 4 level
                elsif rate = "01" then
                    shift_B <= B2_I & B3_I & shift_B(3 downto 2);     -- 6 bpp
                else
                    shift_B <= B3_I & shift_B(3 downto 1);            -- 3 bpp
                end if;
            end if;

            -- Pipeline when to update the quad
            if counter(counter'left) = '0' and (
                (rate = "00" and rateswitch = '1' and counter(6 downto 0) = 0) or   --  1 bpp
                (rate = "00" and rateswitch = '0' and counter(5 downto 0) = 0) or   --  3 bpp
                (rate = "01" and counter(4 downto 0) = 0) or                        --  6 bpp
                (rate = "10" and counter(4 downto 0) = 0) or                        --  6 bpp  4 level or 6x2 12 bpp
                (rate = "11" and counter(3 downto 0) = 0) ) then                    --  9 or 12 bpp
                toggle <= '1';  -- toggle is asserted in cycle 1
            else
                toggle <= '0';
            end if;

            -- Output quad register
            if version = '0' then
                if analog = '1' then
                    quad <= VERSION_NUM_RGB_ANALOG;
                else
                    quad <= VERSION_NUM_RGB_TTL;
                end if;
            elsif counter(counter'left) = '1' then
                quad <= (others => '0');
            elsif toggle = '1' then
                -- quad changes at the start of cycle 2
                quad(11) <= shift_B(3);
                quad(10) <= shift_G(3);
                quad(9)  <= shift_R(3);
                quad(8)  <= shift_B(2);
                quad(7)  <= shift_G(2);
                quad(6)  <= shift_R(2);
                quad(5)  <= shift_B(1);
                quad(4)  <= shift_G(1);
                quad(3)  <= shift_R(1);
                quad(2)  <= shift_B(0);
                quad(1)  <= shift_G(0);
                quad(0)  <= shift_R(0);
            end if;

            -- Output a skewed version of psync
            if version = '0' then
                psync <= vsync_I;
            elsif counter(counter'left) = '1' then
                psync <= '0';
            elsif counter(3 downto 0) = 2 then -- comparing with N gives N-1 cycles of skew
                if rate = "00" and rateswitch = '1' then
                    psync <= counter(7);       --  1 bpp: one edge for every 8 pixels
                elsif rate = "00" and rateswitch = '0' then
                    psync <= counter(6);       --  3 bpp: one edge for every 4 pixels
                elsif rate = "01" then
                    psync <= counter(5);       --  6 bpp: one edge for every 2 pixels
                elsif rate = "10" then
                    psync <= counter(5);       --  6 bpp 4 level or 6x2: one edge for every 2 pixels
                elsif rate = "11" then
                    psync <= counter(4);       --  9/12 bpp: one edge for every pixel
                end if;
            end if;
        end if;
    end process;

    csync <= csync2; -- output the registered version to save a macro-cell

    -- csync2 is cleaned but delayed so OR with csync1 to remove delay on trailing edge of sync pulse
    -- clamp not usable in 4 LEVEL mode (rate = 10) or 8/12 bit mode (rate = 11) so use as multiplex signal instead
    clamp_pulse <= sample_toggle when rate(1) = '1' else not(csync1 or csync2);
    clamp_enable <= '1' when mux = '1' else version;
    -- spdata is overloaded as clamp on/off
    analog <= 'Z' when clamp_enable = '0' else clamp_pulse and sp_data;

end Behavorial;
