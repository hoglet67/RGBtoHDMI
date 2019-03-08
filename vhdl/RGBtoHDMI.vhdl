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
    Port (
        -- From Beeb RGB Connector
        R0:        in    std_logic;
        G0:        in    std_logic;
        B0:        in    std_logic;
        R1:        in    std_logic;
        G1:        in    std_logic;
        B1:        in    std_logic;
        S:         in    std_logic;

        -- From Pi
        clk:       in    std_logic;
        mode7:     in    std_logic;
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
        SW1:       in    std_logic; -- currently unused
        SW2:       in    std_logic; -- currently unused
        SW3:       in    std_logic; -- currently unused
        link:      in    std_logic; -- currently unused
        spare:     in    std_logic; -- currently unused
        LED1:      in    std_logic  -- allow it to be driven from the Pi
    );
end RGBtoHDMI;

architecture Behavorial of RGBtoHDMI is

    subtype counter_type is unsigned(7 downto 0);

    -- Version number: Design_Major_Minor
    -- Design: 0 = Normal CPLD, 1 = Alternative CPLD
    constant VERSION_NUM : std_logic_vector(11 downto 0) := x"024";

    -- For Modes 0..6
    constant default_offset_A : counter_type := "10000000";
    -- Offset B adds half a 16MHz pixel
    constant default_offset_B : counter_type := "10000011";

    -- For Mode 7
    constant mode7_offset_A   : counter_type := "10000000";
    -- Offset B adds half a 12MHz pixel
    constant mode7_offset_B   : counter_type := "10000100";

    -- Sampling points
    constant INIT_SAMPLING_POINTS : std_logic_vector(22 downto 0) := "01000011011011011011011";

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
    signal sp_reg   : std_logic_vector(22 downto 0) := INIT_SAMPLING_POINTS;

    -- Break out of sp_reg
    signal delay    : unsigned(3 downto 0);
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

    -- RGB Input Mux
    signal R        : std_logic;
    signal G        : std_logic;
    signal B        : std_logic;

begin

    R <= R1 when mux = '1' else R0;
    G <= G1 when mux = '1' else G0;
    B <= B1 when mux = '1' else B0;

    offset_A <= sp_reg(2 downto 0);
    offset_B <= sp_reg(5 downto 3);
    offset_C <= sp_reg(8 downto 6);
    offset_D <= sp_reg(11 downto 9);
    offset_E <= sp_reg(14 downto 12);
    offset_F <= sp_reg(17 downto 15);
    half     <= sp_reg(18);
    delay    <= unsigned(sp_reg(22 downto 19));

    -- Shift the bits in LSB first
    process(sp_clk, SW1)
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
            csync1 <= S xnor link;

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

            -- track the previous value of csync2 for falling edge detection
            last <= csync2;

            -- Counter is used to find sampling point for first pixel
            if last = '0' and csync2 = '1' then
                if mode7 = '1' then
                    if half = '1' then
                        counter <= mode7_offset_A + (delay & "000");
                    else
                        counter <= mode7_offset_B + (delay & "000");
                    end if;
                else
                    if half = '1' then
                        counter <= default_offset_A + (delay & "000");
                    else
                        counter <= default_offset_B + (delay & "000");
                    end if;
                end if;
            elsif mode7 = '1' or counter(2 downto 0) /= 5 then
                if counter(counter'left) = '1' then
                    counter <= counter + 1;
                else
                    counter(5 downto 0) <= counter(5 downto 0) + 1;
                end if;
            else
                if counter(counter'left) = '1' then
                    counter <= counter + 3;
                else
                    counter(5 downto 0) <= counter(5 downto 0) + 3;
                end if;
            end if;

            -- Sample point offset index
            if counter(counter'left) = '1' then
                index <= "000";
            else
                -- so index offset changes at the same time counter wraps 7->0
                if counter(2 downto 0) = 6 then
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
            if counter(counter'left) = '0' and counter(2 downto 0) = unsigned(offset) then
                sample <= '1';
            else
                sample <= '0';
            end if;

            -- R Sample/shift register
            if sample = '1' then
                shift_R <= R & shift_R(3 downto 1);
            end if;

            -- G Sample/shift register
            if sample = '1' then
                shift_G <= G & shift_G(3 downto 1);
            end if;

            -- B Sample/shift register
            if sample = '1' then
                shift_B <= B & shift_B(3 downto 1);
            end if;

            -- Output quad register
            if version = '0' then
                quad  <= VERSION_NUM;
                psync <= '0';
            elsif counter(counter'left) = '0' then
                if counter(4 downto 0) = "00000" then
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
                    psync    <= counter(5);
                end if;
            else
                quad  <= (others => '0');
                psync <= '0';
            end if;

        end if;
    end process;

    csync  <= csync1; -- output the registered version to save a macro-cell

end Behavorial;
