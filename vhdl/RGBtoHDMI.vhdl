----------------------------------------------------------------------------------
-- Engineer:            David Banks
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
    Generic (
        SupportAnalog : boolean := false
    );
    Port (
        -- From RGB Connector
        R0:        in    std_logic;
        G0:        in    std_logic;
        B0:        in    std_logic;
        R1:        in    std_logic;
        G1:        in    std_logic;
        B1:        in    std_logic;
        csync_in:  in    std_logic;
        vsync_in:  in    std_logic;
        analog:    inout std_logic;

        -- From Pi
        clk:       in    std_logic;
        mode7_in:  in    std_logic;
        mux:       in    std_logic;
        sp_clk:    in    std_logic;
        sp_clken:  in    std_logic;
        sp_data:   in    std_logic;

        -- To PI GPIO
        quad:      out   std_logic_vector(11 downto 0);
        psync:     out   std_logic;
        csync:     out   std_logic;

        -- User interface
        version:   in    std_logic;
        SW1:       in    std_logic;
        SW2:       in    std_logic;
        SW3:       in    std_logic;

        LED1:      in    std_logic  -- allow it to be driven from the Pi
    );
end RGBtoHDMI;

architecture Behavorial of RGBtoHDMI is

    subtype counter_type is unsigned(7 downto 0);

    -- Version number: Design_Major_Minor
    -- Design: 0 = BBC CPLD
    --         1 = Alternative CPLD
    --         2 = Atom CPLD
    --         3 = six bit CPLD (if required);
    --         4 = RGB CPLD (TTL)
    --         C = RGB CPLD (Analog)
    constant VERSION_NUM_BBC        : std_logic_vector(11 downto 0) := x"066";
    constant VERSION_NUM_RGB_TTL    : std_logic_vector(11 downto 0) := x"475";
    constant VERSION_NUM_RGB_ANALOG : std_logic_vector(11 downto 0) := x"C75";

    -- Sampling points
    constant INIT_SAMPLING_POINTS : std_logic_vector(23 downto 0) := "000000011011011011011011";

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
    signal counter  : counter_type;

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
    signal sp_reg   : std_logic_vector(23 downto 0) := INIT_SAMPLING_POINTS;

    -- Break out of sp_reg
    signal invert   : std_logic;
    signal rate     : std_logic_vector(1 downto 0);
    signal delay    : unsigned(1 downto 0);
    signal half     : std_logic;
    signal offset_A : std_logic_vector(2 downto 0);
    signal offset_B : std_logic_vector(2 downto 0);
    signal offset_C : std_logic_vector(2 downto 0);
    signal offset_D : std_logic_vector(2 downto 0);
    signal offset_E : std_logic_vector(2 downto 0);
    signal offset_F : std_logic_vector(2 downto 0);

    -- Pipelined offset mux output
    signal offset   : std_logic_vector(2 downto 0);

    -- Index to cycle through offsets A..F
    signal index    : std_logic_vector(2 downto 0);

    -- Sample pixel on next clock; pipelined to reduce the number of product terms
    signal sample   : std_logic;

    -- New sample available, toggle psync on next cycle
    signal toggle   : std_logic;

    -- RGB Input Mux
    signal old_mux  : std_logic;
    signal mode7    : std_logic;
	 signal mux_sync : std_logic;
	 
    signal R        : std_logic;
    signal G        : std_logic;
    signal B        : std_logic;

    signal clamp_int    : std_logic;
    signal clamp_enable : std_logic;

begin
    old_mux <= mux when not(SupportAnalog) else '0';
	 
	 mode7 <= mode7_in when not(SupportAnalog) else rate(1) or not(rate(0));
	 
    R <= R1 when old_mux = '1' else R0;
    G <= G1 when old_mux = '1' else G0;
    B <= B1 when old_mux = '1' else B0;

    offset_A <= sp_reg(2 downto 0);
    offset_B <= sp_reg(5 downto 3);
    offset_C <= sp_reg(8 downto 6);
    offset_D <= sp_reg(11 downto 9);
    offset_E <= sp_reg(14 downto 12);
    offset_F <= sp_reg(17 downto 15);
    half     <= sp_reg(18);
    delay    <= unsigned(sp_reg(20 downto 19));
    rate     <= sp_reg(22 downto 21);
    invert   <= sp_reg(23);

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
            csync1 <= csync_in xor invert;

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
                if rate(1) = '1' then
                    counter(7 downto 3) <= "10" & delay & "0";
                    if half = '1' then
                        counter(2 downto 0) <= "000";
                    elsif mode7 = '1' then
                        counter(2 downto 0) <= "100";
                    else
                        counter(2 downto 0) <= "011";
                    end if;
                else
                    counter(7 downto 3) <= "110" & delay;
                    if half = '1' then
                        counter(2 downto 0) <= "000";
                    elsif mode7 = '1' then
                        counter(2 downto 0) <= "100";
                    else
                        counter(2 downto 0) <= "011";
                    end if;
                end if;
            elsif mode7 = '1' or counter(2 downto 0) /= 5 then
                if counter(counter'left) = '1' then
                    counter <= counter + 1;
                else
                    counter(counter'left - 1 downto 0) <= counter(counter'left - 1 downto 0) + 1;
                end if;
            else
                if counter(counter'left) = '1' then
                    counter <= counter + 3;
                else
                    counter(counter'left - 1 downto 0) <= counter(counter'left - 1 downto 0) + 3;
                end if;
            end if;

            -- Sample point offset index
            if counter(counter'left) = '1' then
                index <= "000";
            else
                -- so index offset changes at the same time counter wraps 7->0
                -- so index offset changes at the same time counter wraps ->0
                if (mode7 = '0' and counter(2 downto 0) = 4) or (mode7 = '1' and counter(2 downto 0) = 6) then
                    case index is
                        when "000" =>
                            index <= "001";
                        when "001" =>
                            index <= "010";
                        when "010" =>
                            index <= "011";
                        when "011" =>
                            index <= "100";
                        when "100" =>
                            index <= "101";
                        when others =>
                            index <= "000";
                    end case;
                end if;
            end if;

            -- Sample point offset
            case index is
                when "000" =>
                    offset <= offset_B;
                when "001" =>
                    offset <= offset_C;
                when "010" =>
                    offset <= offset_D;
                when "011" =>
                    offset <= offset_E;
                when "100" =>
                    offset <= offset_F;
                when others =>
                    offset <= offset_A;
            end case;

            -- sample/shift control
            if counter(counter'left) = '0' and counter(2 downto 0) = unsigned(offset) and (rate(1) = '0' or rate(0) = counter(3)) then
                sample <= '1';
            else
                sample <= '0';
            end if;

            -- R Sample/shift register
            if sample = '1' then
                if rate = "01" then
                    shift_R <= R1 & R0 & shift_R(3 downto 2); -- double
                else
                    shift_R <= R & shift_R(3 downto 1);
                end if;
            end if;

            -- G Sample/shift register
            if sample = '1' then
                if rate = "01" then
                    shift_G <= G1 & G0 & shift_G(3 downto 2); -- double
                else
                    shift_G <= G & shift_G(3 downto 1);
                end if;
            end if;

            -- B Sample/shift register
            if sample = '1' then
                if rate = "01" then
                    shift_B <= B1 & B0 & shift_B(3 downto 2); -- double
                else
                    shift_B <= B & shift_B(3 downto 1);
                end if;
            end if;

            -- Pipeline when to update the quad
            if counter(counter'left) = '0' and (
                (rate = "00" and counter(4 downto 0) = 0)  or    -- normal
                (rate = "01" and counter(3 downto 0) = 0)  or    -- double
                (rate = "10" and counter(5 downto 0) = 0)  or    -- subsample even
                (rate = "11" and counter(5 downto 0) = 32)) then -- subsample odd
                -- toggle is asserted in cycle 1
                toggle <= '1';
            else
                toggle <= '0';
            end if;

            -- Output quad register
            if version = '0' then
                if SupportAnalog then
                    if analog = '1' then
                        quad <= VERSION_NUM_RGB_ANALOG;
                    else
                        quad <= VERSION_NUM_RGB_TTL;
                    end if;
                else
                    quad <= VERSION_NUM_BBC;
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
                psync <= vsync_in;
            elsif counter(counter'left) = '1' then
                psync <= '0';
            elsif counter(3 downto 0) = 3 then -- comparing with N gives N-1 cycles of skew
                if rate = "00" then
                    psync <= counter(5); -- normal
                elsif rate = "01" then
                    psync <= counter(4); -- double
                elsif counter(5) = rate(0) then
                    psync <= counter(6); -- subsample
                end if;
            end if;

        end if;
    end process;

    csync <= csync2; -- output the registered version to save a macro-cell

    analog_additions: if SupportAnalog generate
	     -- csync2 is cleaned but delayed so OR with csync1 to remove delay on trailing edge of sync pulse
	     -- spdata is overloaded as clamp on/off  
        clamp_int <= not(csync1 or csync2) and sp_data;

        clamp_enable <= '1' when mux = '1' else version;

        analog <= 'Z' when clamp_enable = '0' else clamp_int;
    end generate;

end Behavorial;
