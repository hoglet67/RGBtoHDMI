----------------------------------------------------------------------------------
-- Engineer:            David Banks
--
-- Create Date:         9/6/2018
-- Module Name:         RGBtoHDMI CPLD
-- Project Name:        RGBtoHDMI
-- Target Devices:      XC9572XL
--
-- Version:             0.1
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
        elk:       in    std_logic;
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
        LED1:      out   std_logic;
        LED2:      out   std_logic
    );
end RGBtoHDMI;

architecture Behavorial of RGBtoHDMI is

    -- Version number: Design_Major_Minor
    -- Design: 0 = Normal CPLD, 1 = Alternative CPLD
    constant VERSION_NUM : std_logic_vector(11 downto 0) := x"101";

    -- For Modes 0..6
    constant default_offset : unsigned(11 downto 0) := to_unsigned(4096 - 64 * 17 + 6, 12);

    -- For Mode 7
    constant mode7_offset : unsigned(11 downto 0) := to_unsigned(4096 - 96 * 12 + 4, 12);

    -- Sampling points
    constant INIT_SAMPLING_POINTS : std_logic_vector(20 downto 0) := "000000000000011011011";

    signal shift_R : std_logic_vector(3 downto 0);
    signal shift_G : std_logic_vector(3 downto 0);
    signal shift_B : std_logic_vector(3 downto 0);

    signal CSYNC1 : std_logic;

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
    signal counter : unsigned(11 downto 0);

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
    signal sp_reg     : std_logic_vector(20 downto 0) := INIT_SAMPLING_POINTS;

    -- Break out of sp_reg
    signal offset_A : std_logic_vector(1 downto 0);
    signal offset_B : std_logic_vector(1 downto 0);
    signal offset_C : std_logic_vector(1 downto 0);
    signal offset_D : std_logic_vector(1 downto 0);
    signal offset_E : std_logic_vector(1 downto 0);
    signal offset_F : std_logic_vector(1 downto 0);
    signal delay_R  : std_logic_vector(2 downto 0);
    signal delay_G  : std_logic_vector(2 downto 0);
    signal delay_B  : std_logic_vector(2 downto 0);

    -- Pipelined offset mux output
    signal offset   : std_logic_vector(1 downto 0);

    -- Pipelined adjusted counter
    signal adjusted_counter : unsigned(2 downto 0);

    -- Index to cycle through offsets A..F
    signal index    : std_logic_vector(2 downto 0);

    -- Sample pixel on next clock; pipelined to reduce the number of product terms
    signal sample_R : std_logic;
    signal sample_G : std_logic;
    signal sample_B : std_logic;

    -- RGB Input Mux
    signal R        : std_logic;
    signal G        : std_logic;
    signal B        : std_logic;

begin

    R <= R1 when elk = '1' else R0;
    G <= G1 when elk = '1' else G0;
    B <= B1 when elk = '1' else B0;

    delay_R <= sp_reg(2 downto 0);
    delay_G <= sp_reg(5 downto 3);
    delay_B <= sp_reg(8 downto 6);
    offset_A <= sp_reg(10 downto 9);
    offset_B <= sp_reg(12 downto 11);
    offset_C <= sp_reg(14 downto 13);
    offset_D <= sp_reg(16 downto 15);
    offset_E <= sp_reg(18 downto 17);
    offset_F <= sp_reg(20 downto 19);

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
            CSYNC1 <= S;

            -- Counter is used to find sampling point for first pixel
            if CSYNC1 = '0' then
                if mode7 = '1' then
                    counter <= mode7_offset;
                else
                    counter <= default_offset;
                end if;
            elsif counter(11) = '1' then
                counter <= counter + 1;
            elsif mode7 = '1' or counter(2 downto 0) /= 5 then
                counter(5 downto 0) <= counter(5 downto 0) + 1;
            else
                counter(5 downto 0) <= counter(5 downto 0) + 3;
            end if;

            -- Sample point offset index
            if CSYNC1 = '0' then
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

            -- Adjusted counter is the counter value with the offset added in
            --
            -- The offset is sign extended offset to 3 bits so values are:
            --   00 -> 000 (0)
            --   01 -> 001 (1)
            --   10 -> 110 (-2)
            --   11 -> 111 (-1)
            adjusted_counter <= counter(2 downto 0) + unsigned(offset(1) & offset(1 downto 0));

            -- R sample/shift control
            if counter(11) = '0' and adjusted_counter(2 downto 0) = unsigned(delay_R) then
                sample_R <= '1';
            else
                sample_R <= '0';
            end if;

            -- G sample/shift control
            if counter(11) = '0' and adjusted_counter(2 downto 0) = unsigned(delay_G) then
                sample_G <= '1';
            else
                sample_G <= '0';
            end if;

            -- B sample/shift control
            if counter(11) = '0' and adjusted_counter(2 downto 0) = unsigned(delay_B) then
                sample_B <= '1';
            else
                sample_B <= '0';
            end if;

            -- R Sample/shift register
            if sample_R = '1' then
                shift_R <= R & shift_R(3 downto 1);
            end if;

            -- G Sample/shift register
            if sample_G = '1' then
                shift_G <= G & shift_G(3 downto 1);
            end if;

            -- B Sample/shift register
            if sample_B = '1' then
                shift_B <= B & shift_B(3 downto 1);
            end if;

            -- Output quad register
            if version = '0' then
				    quad  <= VERSION_NUM;
					 psync <= '0';
            elsif counter(11) = '0' then
                if counter(4 downto 0) = "00001" then
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

    csync  <= S;      -- pass through, as clock might not be running
    --LED1   <= 'Z';    -- allow this to be driven from the Pi
    --LED2   <= not(mode7);

end Behavorial;
