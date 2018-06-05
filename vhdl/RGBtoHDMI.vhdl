----------------------------------------------------------------------------------
-- Engineer:            David Banks
--
-- Create Date:         14/4/2017
-- Module Name:         RGBtoHDMI CPLD
-- Project Name:        RGBtoHDMI
-- Target Devices:      XC9572XL
--
-- Version:             0.50
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
        sp_data:   in    std_logic;

        -- To PI GPIO
        quad:      out   std_logic_vector(11 downto 0);
        psync:     out   std_logic;
        csync:     out   std_logic;
        SW1out:    out   std_logic;

        -- User interface
        link:      in    std_logic; -- currently unused		  
        SW1:       in    std_logic;
        SW2:       in    std_logic; -- currently unused
        SW3:       in    std_logic; -- currently unused
        LED1:      out   std_logic;
        LED2:      out   std_logic
    );
end RGBtoHDMI;

architecture Behavorial of RGBtoHDMI is

    -- For Modes 0..6
    constant default_offset : unsigned(11 downto 0) := to_unsigned(4096 - 64 * 23 + 6, 12);

    -- For Mode 7
    constant mode7_offset : unsigned(11 downto 0) := to_unsigned(4096 - 96 * 12 + 4, 12);

    -- Sampling points
    constant INIT_SAMPLING_POINTS : std_logic_vector(20 downto 0) := "011011011011011011011";

    signal shift : std_logic_vector(11 downto 0);

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

    signal counter2 : unsigned(5 downto 3);

    -- Sample point register;
    --
    -- In Modes 0..6 each pixel lasts 6 clocks (96MHz / 16MHz). The original
    -- pixel clock is a clean 16Mhz clock, so only one sample point is needed.
    --
    -- In Mode 7 each pixel lasts 8 clocks (96MHz / 12MHz). The original
    -- pixel clock is a regenerated 6Mhz clock, and both edges are used.
    -- Due to the way it is generated, there are three distinct phases,
    -- hence three sampling points are used.
    signal sp_reg     : std_logic_vector(20 downto 0) := INIT_SAMPLING_POINTS;

    -- Break out of sp
    signal sp         : std_logic_vector(2 downto 0);
    signal mode7_sp_A : std_logic_vector(2 downto 0);
    signal mode7_sp_B : std_logic_vector(2 downto 0);
    signal mode7_sp_C : std_logic_vector(2 downto 0);
    signal mode7_sp_D : std_logic_vector(2 downto 0);
    signal mode7_sp_E : std_logic_vector(2 downto 0);
    signal mode7_sp_F : std_logic_vector(2 downto 0);
    signal default_sp : std_logic_vector(2 downto 0);

    -- Index to allow cycling between A, B and C in Mode 7
    signal sp_index   : std_logic_vector(2 downto 0);

    -- Sample pixel on next clock; pipelined to reducethe number of product terms
    signal sample     : std_logic;

    signal R          : std_logic;
    signal G          : std_logic;
    signal B          : std_logic;

begin

    R <= R1 when elk = '1' else R0;
    G <= G1 when elk = '1' else G0;
    B <= B1 when elk = '1' else B0;
    
    mode7_sp_A <= sp_reg(2 downto 0);
    mode7_sp_B <= sp_reg(5 downto 3);
    mode7_sp_C <= sp_reg(8 downto 6);
    mode7_sp_D <= sp_reg(11 downto 9);
    mode7_sp_E <= sp_reg(14 downto 12);
    mode7_sp_F <= sp_reg(17 downto 15);
    default_sp <= sp_reg(20 downto 18);

    -- Shift the bits in LSB first
    process(sp_clk, SW1)
    begin
        if SW1 = '0' then
            sp_reg <= INIT_SAMPLING_POINTS;
        elsif rising_edge(sp_clk) then
            sp_reg <= sp_data & sp_reg(sp_reg'left downto sp_reg'right + 1);
        end if;
    end process;

    process(clk)
    begin
        if rising_edge(clk) then
            -- synchronize CSYNC to the sampling clock
            CSYNC1 <= S;

            -- pipeline sample
            if counter(2 downto 0) = unsigned(sp) then
                sample <= '1';
            else
                sample <= '0';
            end if;
            -- pipeline counter
            counter2(5 downto 3) <= counter(5 downto 3);

            if CSYNC1 = '0' then
                -- in the line sync
                psync <= '0';
                -- counter is used to find sampling point for first pixel
                if mode7 = '1' then
                    counter <= mode7_offset;
                    sp_index <= "000";
                    sp <= mode7_sp_A;
                else
                    counter <= default_offset;
                    sp_index <= "111";
                    sp <= default_sp;
                end if;
            else
                -- within the line
                if mode7 = '1' then
                    if counter = 63 then
                        counter <= to_unsigned(0, counter'length);
                    else
                        counter <= counter + 1;
                    end if;
                    if counter(2 downto 0) = 7 then
                        case sp_index is
                            when "000" =>
                                sp_index <= "001";
                                sp <= mode7_sp_B;
                            when "001" =>
                                sp_index <= "010";
                                sp <= mode7_sp_C;
                            when "010" =>
                                sp_index <= "011";
                                sp <= mode7_sp_D;
                            when "011" =>
                                sp_index <= "100";
                                sp <= mode7_sp_E;
                            when "100" =>
                                sp_index <= "101";
                                sp <= mode7_sp_F;
                            when others =>
                                sp_index <= "000";
                                sp <= mode7_sp_A;
                        end case;
                    end if;
                else
                    if counter = 61 then
                        counter <= to_unsigned(0, counter'length);
                    elsif counter(2 downto 0) = 5 then
                        counter <= counter + 3;
                    else
                        counter <= counter + 1;
                    end if;
                end if;

                if counter(11) = '0' then
                    if sample = '1' then
                        shift <= B & G & R & shift(11 downto 3);
                        if counter2(4 downto 3) = "00" then
                            quad <= shift;
                            psync <= counter2(5);
                        end if;
                    end if;
                else
                    quad <= (others => '0');
                    psync <= '0';
                end if;
            end if;
        end if;
    end process;

    csync  <= CSYNC1;
    LED1   <= 'Z';    -- allow this to be driven from the Pi
    LED2   <= mode7;     
    SW1Out <= SW1;

end Behavorial;
