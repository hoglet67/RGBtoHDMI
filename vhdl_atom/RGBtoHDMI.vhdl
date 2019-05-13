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
        -- From Atom L/PA/PB Comparators
        AL_I:      in    std_logic;
        AH_I:      in    std_logic;
        BL_I:      in    std_logic;
        BH_I:      in    std_logic;
        L_I:       in    std_logic;
        HS_I:      in    std_logic;
        FS_I:      in    std_logic;
        CSS_I:     in    std_logic;
        SYNC_I:    in    std_logic;

        -- To Atom L/PA/PB Comparators
        clamp:     out   std_logic;

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
    -- Design: 0 = Normal CPLD, 1 = Alternative CPLD, 2=Atom CPLD
    constant VERSION_NUM  : std_logic_vector(11 downto 0) := x"223";

    -- Default offset to start sampling at
    constant default_offset   : unsigned(8 downto 0) := to_unsigned(512 - 255 + 8, 9);

    -- Turn on back porch clamp
    constant atom_clamp_start : unsigned(8 downto 0) := to_unsigned(512 - 255 + 48, 9);

    -- Turn off back port clamo
    constant atom_clamp_end   : unsigned(8 downto 0) := to_unsigned(512 - 255 + 248, 9);

    -- Sampling points
    constant INIT_SAMPLING_POINTS : std_logic_vector(5 downto 0) := "110000";

    signal shift_R : std_logic_vector(1 downto 0);
    signal shift_G : std_logic_vector(1 downto 0);
    signal shift_B : std_logic_vector(1 downto 0);
    signal shift_X : std_logic_vector(1 downto 0);

    -- The sampling counter runs at 8x pixel clock of 7.15909MHz = 56.272720MHz
    --
    -- The luminance signal is sampled every  8 counts (bits 2..0)
    -- The chromance signal is sampled every 16 counts (bits 3..0)
    -- The pixel shift register is shifter every 4 counts (bits 1..0)
    --    (i.e. each pixel is replicated twice)
    -- The quad counter is bits 3..2
    -- The psync flag is bit 4
    --
    -- At the moment we don't count pixels with the line, the Pi does that
    signal counter  : unsigned(8 downto 0);

    -- Sample point register;
    signal sp_reg   : std_logic_vector(5 downto 0) := INIT_SAMPLING_POINTS;

    -- Break out of sp_reg
    signal offset   : unsigned (3 downto 0);
    signal filter_C : std_logic;
    signal filter_L : std_logic;

    -- Sample pixel on next clock; pipelined to reduce the number of product terms
    signal sample_C : std_logic;
    signal sample_L : std_logic;

    -- Decoded RGB signals
    signal R        : std_logic;
    signal G        : std_logic;
    signal B        : std_logic;
    signal X        : std_logic; -- indicates either dark orange or bright orange

    -- R/PA/PB processing pipeline
    signal AL1      : std_logic;
    signal AH1      : std_logic;
    signal BL1      : std_logic;
    signal BH1      : std_logic;
    signal LL1      : std_logic;
    signal LH1      : std_logic;

    signal AL2      : std_logic;
    signal AH2      : std_logic;
    signal BL2      : std_logic;
    signal BH2      : std_logic;
    signal LL2      : std_logic;
    signal LH2      : std_logic;

    signal AL3      : std_logic;
    signal AH3      : std_logic;
    signal BL3      : std_logic;
    signal BH3      : std_logic;
    signal LL3      : std_logic;
    signal LH3      : std_logic;

    signal AL       : std_logic;
    signal AH       : std_logic;
    signal BL       : std_logic;
    signal BH       : std_logic;
    signal LL       : std_logic;
    signal LH       : std_logic;

    signal HS1      : std_logic;
    signal HS2      : std_logic;
    signal FS1      : std_logic;

    function map_to_sixbit_pixel (i : std_logic_vector(3 downto 0))
        return std_logic_vector is
        variable temp : std_logic_vector(5 downto 0);
    begin
        case i is
            when "0000" => temp := "000000";
            when "0001" => temp := "000001";
            when "0010" => temp := "000010";
            when "0011" => temp := "000011";
            when "0100" => temp := "000100";
            when "0101" => temp := "000101";
            when "0110" => temp := "000110";
            when "0111" => temp := "000111";
            when "1000" => temp := "001011";
            when "1001" => temp := "010011";
            when "1010" => temp := "001000";
            when "1011" => temp := "010000";
            when others => temp := "000000";
        end case;
        return temp;
    end function;

begin

    offset <= unsigned(sp_reg(3 downto 0));
    filter_C <= sp_reg(4);
    filter_L <= sp_reg(5);

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
            HS1 <= HS_I;
            HS2 <= HS1;

            -- Counter is used to find sampling point for first pixel
            if HS2 = '0' and HS1 = '1' then
                counter <= default_offset;
            elsif counter(counter'left) = '1' then
                counter <= counter + 1;
            else
                counter(5 downto 0) <= counter(5 downto 0) + 1;
            end if;

            -- sample luminance signal
            if counter(2 downto 0) = (not offset(2)) & offset(1 downto 0) then
                sample_L <= '1';
            else
                sample_L <= '0';
            end if;

            -- sample colour signal
            if counter(3 downto 0) = offset then
                sample_C <= '1';
            else
                sample_C <= '0';
            end if;

            -- Atom pixel processing
            AL1 <= AL_I;
            AH1 <= AH_I;
            BL1 <= BL_I;
            BH1 <= BH_I;

            AL2 <= AL1;
            AH2 <= AH1;
            BL2 <= BL1;
            BH2 <= BH1;

            AL3 <= AL2;
            AH3 <= AH2;
            BL3 <= BL2;
            BH3 <= BH2;

            LL1 <= L_I;
            LL2 <= LL1;
            LL3 <= LL2;

            LH1 <= SYNC_I;
            LH2 <= LH1;
            LH3 <= LH2;

            if sample_C = '1' then
                if filter_C = '1' then
                    AL <= (AL1 AND AL2) OR (AL1 AND AL3) OR (AL2 AND AL3);
                    AH <= (AH1 AND AH2) OR (AH1 AND AH3) OR (AH2 AND AH3);
                    BL <= (BL1 AND BL2) OR (BL1 AND BL3) OR (BL2 AND BL3);
                    BH <= (BH1 AND BH2) OR (BH1 AND BH3) OR (BH2 AND BH3);
                else
                    AL <= AL2;
                    AH <= AH2;
                    BL <= BL2;
                    BH <= BH2;
                end if;
            end if;

            if sample_L = '1' then
                if filter_L = '1' then
                    LL <= (LL1 AND LL2) OR (LL1 AND LL3) OR (LL2 AND LL3);
                    LH <= (LH1 AND LH2) OR (LH1 AND LH3) OR (LH2 AND LH3);
                else
                    LL <= LL2;
                    LH <= LH2;
                end if;
            end if;

            -- YUV to RGB
            if sample_L = '1' then
                --                         AL AH BL BH LL LH  X  B  G  R
                --YELLOW        WH 1.5 1.0  0  0  1  0  X  X  0  0  1  1
                --RED           WL 2.0 1.5  0  1  0  0  X  X  0  0  0  1
                --MAGENTA       WM 2.0 2.0  0  1  0  1  X  X  0  1  0  1
                --BUFF          WH 1.5 1.5  0  0  0  0  1  X  0  1  1  1
                --BRIGHT ORANGE WH 2.0 1.0  0  1  1  0  X  1  1  0  0  1
                --DARK ORANGE   BL 2.0 1.0  0  1  1  0  0  X  1  0  1  1

                R <= (NOT AL AND NOT AH AND BL AND NOT BH) OR (NOT AL AND AH AND NOT BL AND NOT BH) OR (NOT AL AND AH AND NOT BL AND BH) OR (NOT AL AND NOT AH AND NOT BL AND NOT BH AND LL) OR (NOT AL AND AH AND BL AND NOT BH AND LH) OR (NOT AL AND AH AND BL AND NOT BH AND LH)  OR (NOT AL AND AH AND BL AND NOT BH AND NOT LL);

                --                         AL AH BL BH LL LH  X  B  G  R
                --YELLOW        WM 1.5 1.0  0  0  1  0  X  X  0  0  1  1
                --CYAN          WM 1.0 1.5  1  0  0  0  X  X  0  1  1  0
                --GREEN         WM 1.0 1.0  1  0  1  0  1  X  0  0  1  0
                --DARK GREEN    BL 1.0 1.0  1  0  1  0  0  X  1  0  1  0
                --BUFF          WM 1.5 1.5  0  0  0  0  1  X  0  1  1  1
                --DARK ORANGE   BL 2.0 1.0  0  1  1  0  0  X  1  0  1  1

                G <= (NOT AL AND NOT AH AND BL AND NOT BH) OR (AL AND NOT AH AND NOT BL AND NOT BH) OR (AL AND NOT AH AND BL AND NOT BH) OR (NOT AL AND NOT AH AND NOT BL AND NOT BH AND LL) OR (NOT AL AND AH AND BL AND NOT BH AND NOT LL);

                --                         AL AH BL BH LL LH  X  B  G  R
                --BLUE          WL 1.5 2.0  0  0  0  1  X  X  0  1  0  0
                --CYAN          WM 1.0 1.5  1  0  0  0  X  X  0  1  1  0
                --MAGENTA       WM 2.0 2.0  0  1  0  1  X  X  0  1  0  1
                --BUFF          WM 1.5 1.5  0  0  0  0  1  X  0  1  1  1

                B <= (NOT AL AND NOT AH AND NOT BL AND BH) OR (AL AND NOT AH AND NOT BL AND NOT BH) OR (NOT AL AND AH AND NOT BL AND BH) OR (NOT AL AND NOT AH AND NOT BL AND NOT BH AND LL);

                --                         AL AH BL BH LL LH  X  B  G  R
                --NORMAL ORANGE WM 2.0 1.0  0  1  1  0  1  0  1  0  0  0
                --BRIGHT ORANGE WH 2.0 1.0  0  1  1  0  1  1  1  0  0  1
                --DARK GREEN    BL 1.0 1.0  1  0  1  0  0  X  1  0  1  0
                --DARK ORANGE   BL 2.0 1.0  0  1  1  0  0  X  1  0  1  1

                X <= (NOT AL AND AH AND BL AND NOT BH) OR (AL AND NOT AH AND BL AND NOT BH AND NOT LL);

            end if;

            if sample_L = '1' and counter(counter'left) = '0' then
                -- R Sample/shift register
                shift_R <= R & shift_R(1);
                -- G Sample/shift register
                shift_G <= G & shift_G(1);
                -- B Sample/shift register
                shift_B <= B & shift_B(1);
                -- X Sample/shift register
                shift_X <= X & shift_X(1);
            end if;

            -- Output quad register
            if version = '0' then
                quad  <= VERSION_NUM;
                psync <= '0';
            elsif counter(counter'left) = '0' then
                if counter(3 downto 0) = "0000" then
                    quad(11 downto 6) <= map_to_sixbit_pixel(shift_X(1) & shift_B(1) & shift_G(1) & shift_R(1));
                    quad( 5 downto 0) <= map_to_sixbit_pixel(shift_X(0) & shift_B(0) & shift_G(0) & shift_R(0));
                end if;
                if counter(3 downto 0) = "0010" then
                    psync    <= counter(4);
                end if;
            else
                quad  <= (others => '0');
                psync <= '0';
            end if;

            -- generate the clamp output
            if counter >= atom_clamp_start AND counter < atom_clamp_end then
                clamp <= '1';
            else
                clamp <= '0';
            end if;

            -- generate the csync output
            if HS2 = '0' and HS1 = '1' then
                FS1 <= FS_I;
            end if;

            if HS2 = '1' and HS1 = '0' then
                csync <= '0';
            elsif HS2 = '0' and HS1 = '1' and not (FS1 = '0' and FS_I = '1') then
                csync <= '1';
            end if;

        end if;
    end process;

end Behavorial;
