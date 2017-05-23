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
        R:         in    std_logic;
        G:         in    std_logic;
        B:         in    std_logic;
        S:         in    std_logic;

        -- From Pi
        clk:       in    std_logic;
        mode7:     in    std_logic;

        -- To PI GPIO
        quad:      out   std_logic_vector(11 downto 0);
        psync:     out   std_logic;
        csync:     out   std_logic;

        -- Test
        SW:        in    std_logic;
        LED1:      out   std_logic;
        LED2:      out   std_logic
    );
end RGBtoHDMI;

architecture Behavorial of RGBtoHDMI is

    -- For Modes 0..6
    constant default_sample_point : unsigned(11 downto 0) := to_unsigned(4096 - 64 * 23 + 6, 12);

    -- For Mode 7
    constant mode7_sample_point : unsigned(11 downto 0) := to_unsigned(4096 - 96 * 12 + 6, 12);

    signal shift : std_logic_vector(11 downto 0);

    signal CSYNC1 : std_logic;

    -- The counter runs at 4x pixel clock
    --
    -- It serves several purposes:
    -- 1. Counts the 12us between the rising edge of nCSYNC and the first pixel
    -- 2. Counts within each pixel (bits 0 and 1)
    -- 3. Counts counts pixels within a quad pixel (bits 2 and 3)
    -- 4. Handles double buffering of alternative quad pixels (bit 4)
    --
    -- At the moment we don't count pixels with the line, the Pi does that
    --
    -- The sequence is basically:
    -- 1280, 1280, ..., 1280, 1281, 1282, ... , 2047
    -- 0, 1, 2, ..., 14, 15  - first quad pixel
    -- 16, 17, 18, ..., 30, 31, -- second quad pixel
    -- ...
    -- 1280, 1280, ..., 1280, 1281, 1282, ... , 2047

    signal counter : unsigned(11 downto 0);

    -- Hsync is every 64us
    signal led_counter : unsigned(12 downto 0);

begin

    process(S)
    begin
        if rising_edge(S) then
            led_counter <= led_counter + 1;
        end if;
    end process;

    process(clk)
    begin
        if rising_edge(clk) then
            -- synchronize CSYNC to the sampling clock
            CSYNC1 <= S;

            if CSYNC1 = '0' then
                -- in the line sync
                psync <= '0';
                -- counter is used to find sampling point for first pixel
                if mode7 = '1' then
                    counter <= mode7_sample_point;
                else
                    counter <= default_sample_point;
                end if;
            else
                -- within the line
                if mode7 = '1' then
                    if counter = 63 then
                        counter <= to_unsigned(0, counter'length);
                    else
                        counter <= counter + 1;
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
                    if counter(2 downto 0) = "100" then
                        shift <= B & G & R & shift(11 downto 3);
                        if counter(4 downto 3) = "00" then
                            quad <= shift;
                            psync <= counter(5);
                        end if;
                    end if;
                else
                    quad <= (others => '0');
                    psync <= '0';
                end if;
            end if;
        end if;
    end process;

    csync <= S;

    LED1 <= mode7;

    LED2 <= led_counter(led_counter'left);

end Behavorial;
