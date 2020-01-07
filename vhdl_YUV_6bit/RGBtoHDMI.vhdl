----------------------------------------------------------------------------------
-- Engineer:            David Banks
--
-- Create Date:         15/7/2018
-- Module Name:         RGBtoHDMI CPLD
-- Project Name:        RGBtoHDMI
-- Target Devices:      XC9572XL
--
-- Version:             6.0
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
        LL_I:      in    std_logic;
        LH_I:      in    std_logic;
        HS_I:      in    std_logic;
        FS_I:      in    std_logic;

        -- To Atom L/PA/PB Comparators
        clamp:     out   std_logic;

        -- From Pi
        clk:       in    std_logic;
        sp_clk:    in    std_logic;
        sp_clken:  in    std_logic;
        sp_data:   in    std_logic;
        mux:       in    std_logic;

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
    -- Design: 0 = Normal CPLD, 1 = Alternative CPLD, 2=Atom CPLD, 3=YUV6847 CPLD
    constant VERSION_NUM  : std_logic_vector(11 downto 0) := x"380";

    -- NOTE: the difference between the leading and trailing offsets is
    -- 256 clks = 32 pixel clocks.

    -- Default offset to start sampling at when using the leading edge of sync
    constant leading_offset   : unsigned(9 downto 0) := to_unsigned(1024 - 512, 10);

    -- Default offset to start sampling at when using the trailing edge of sync
    constant trailing_offset   : unsigned(9 downto 0) := to_unsigned(1024 - 256, 10);

    -- Sampling points
    constant INIT_SAMPLING_POINTS : std_logic_vector(13 downto 0) := "00000001100000";

    -- The sampling counter runs at 8x pixel clock
    signal counter  : unsigned(9 downto 0);

    signal clamp_counter  : unsigned(2 downto 0);
    signal start_clamp : std_logic;

    -- Sample point register;
    signal sp_reg   : std_logic_vector(13 downto 0) := INIT_SAMPLING_POINTS;

    -- Break out of sp_reg
    signal offset     : unsigned (4 downto 0);
    signal filter_C   : std_logic;
    signal filter_L   : std_logic;
    signal invert     : std_logic;
    signal subsam_C   : std_logic;
    signal alt_R      : std_logic;
    signal edge       : std_logic;
    signal clamp_size : unsigned (2 downto 0);

    -- State to determine whether to invert A
    signal inv_R     : std_logic;


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

    signal AL_next  : std_logic;
    signal AH_next  : std_logic;
    signal BL_next  : std_logic;
    signal BH_next  : std_logic;
    signal LL_next  : std_logic;
    signal LH_next  : std_logic;

    signal AL       : std_logic;
    signal AH       : std_logic;
    signal BL       : std_logic;
    signal BH       : std_logic;
    signal LL       : std_logic;
    signal LH       : std_logic;

    signal HS1      : std_logic;
    signal HS2      : std_logic;
    signal HS3      : std_logic;

    signal LL_S      : std_logic;
    signal LH_S      : std_logic;
    signal swap_bits : std_logic;

    signal sample_L  : std_logic;
    signal sample_AB : std_logic;
    signal sample_Q : std_logic;

    signal HS_counter : unsigned(1 downto 0);

begin
    -- Offset is inverted as we count upwards to 0
    offset <= unsigned(sp_reg(4 downto 0) xor "11111");
    filter_C <= sp_reg(5);
    filter_L <= sp_reg(6);
    invert <= sp_reg(7);
    subsam_C <= sp_reg(8);
    alt_R <= sp_reg(9);
    edge <= sp_reg(10);
    clamp_size <= unsigned(sp_reg(13 downto 11));

    swap_bits <= FS_I when mux = '1' else '0';

    LL_S <= LH_I when swap_bits = '1' else LL_I;
    LH_S <= LL_I when swap_bits = '1' else LH_I;

    -- Shift the bits in LSB first
    process(sp_clk)
    begin
        if rising_edge(sp_clk) then
            if sp_clken = '1' then
                sp_reg <= sp_data & sp_reg(sp_reg'left downto sp_reg'right + 1);
            end if;
        end if;
    end process;

    -- Combine the YUV bits into a 6-bit colour value (combinatorial logic)
    -- including the 3-bit majority voting
    process(AL1, AL2, AL_I,
            AH1, AH2, AH_I,
            BL1, BL2, BL_I,
            BH1, BH2, BH_I,
            LL1, LL2, LL_S,
            LH1, LH2, LH_S,
            filter_C,
            filter_L,
            inv_R
            )
        variable tmp_AL : std_logic;
        variable tmp_AH : std_logic;
    begin
        if filter_C = '1' then
            tmp_AL := (AL1 AND AL2) OR (AL1 AND AL_I) OR (AL2 AND AL_I);
            tmp_AH := (AH1 AND AH2) OR (AH1 AND AH_I) OR (AH2 AND AH_I);
        else
            tmp_AL := AL1;
            tmp_AH := AH1;
        end if;
        if filter_C = '1' then
            BL_next <= (BL1 AND BL2) OR (BL1 AND BL_I) OR (BL2 AND BL_I);
            BH_next <= (BH1 AND BH2) OR (BH1 AND BH_I) OR (BH2 AND BH_I);
        else
            BL_next <= BL1;
            BH_next <= BH1;
        end if;
        if filter_L = '1' then
            LL_next <= (LL1 AND LL2) OR (LL1 AND LL_S) OR (LL2 AND LL_S);
            LH_next <= (LH1 AND LH2) OR (LH1 AND LH_S) OR (LH2 AND LH_S);
        else
            LL_next <= LL1;
            LH_next <= LH1;
        end if;
        if inv_R = '1' and tmp_AH = tmp_AL then
            AL_next <= not tmp_AL;
            AH_next <= not tmp_AH;
        else
            AL_next <= tmp_AL;
            AH_next <= tmp_AH;
        end if;
    end process;

    process(clk)
    begin
        if rising_edge(clk) then

            -- synchronize CSYNC to the sampling clock
            HS1 <= HS_I xor invert;

            -- De-glitch HS
            --    HS1 is the possibly glitchy input
            --    HS2 is the filtered output
            if HS1 = HS2 then
                -- output same as input, reset the counter
                HS_counter <= to_unsigned(0, HS_counter'length);
            else
                -- output different to input
                HS_counter <= HS_counter + 1;
                -- if the difference lasts for N-1 cycles, update the output
                if HS_counter = 3 then
                    HS2 <= HS1;
                end if;
            end if;

            HS3 <= HS2;

            -- Counter is used to find sampling point for first pixel
            if HS3 = '1' and HS2 = '0' then
                -- leading edge, reset the counter to measure the sync pulse
                counter <= leading_offset or offset;
                if alt_R = '1' then
                    inv_R <= not inv_R;
                else
                    inv_R <= '0';
                end if;
            elsif HS3 = '0' and HS2 = '1' and edge = '0' then
                -- trailing edge, if edge=0 (trailing) then reset the counter
                counter <= trailing_offset or offset;
            elsif counter(counter'left) = '1' then
                -- if HS remains low of 1FF cycles, we have the Spectrum FS
                -- (1FF cycles is 9.125 us)
                if HS2 = '0' and "000" & counter(8 downto 0) = x"1FF" then
                    -- synchronise inv_R to frame sync pulse
                    if alt_R = '1' then
                        inv_R <= '1';
                    end if;
                end if;
                counter <= counter + 1;
            else
                counter(5 downto 0) <= counter(5 downto 0) + 1;
            end if;

            -- Registers for Chroma / Luma Filtering

            -- TODO: If there are 6 spare registers, it might be worth adding
            -- one more tier of registers, so nothing internal depends directly
            -- on potentially asynchronous inputs.

            AL1 <= AL_I;
            AH1 <= AH_I;
            BL1 <= BL_I;
            BH1 <= BH_I;
            LL1 <= LL_S;
            LH1 <= LH_S;

            AL2 <= AL1;
            AH2 <= AH1;
            BL2 <= BL1;
            BH2 <= BH1;
            LL2 <= LL1;
            LH2 <= LH1;

            -- Pipeline the sample signals to reduce the product terms
            if (subsam_C = '0' and counter(2 downto 0) = "111") or
               (subsam_C = '1' and counter(3 downto 0) = "0011") then
                sample_AB <= '1';
            else
                sample_AB <= '0';
            end if;

            if counter(2 downto 0) = "111" then
                sample_L <= '1';
            else
                sample_L <= '0';
            end if;

            if counter(3 downto 0) = "1111" then
                sample_Q <= '1';
            else
                sample_Q <= '0';
            end if;

            -- sample colour signal
            if sample_AB = '1' then
                AL <= AL_next;
                AH <= AH_next;
                BL <= BL_next;
                BH <= BH_next;
            end if;

            -- sample luminance signal
            if sample_L = '1' then
                LL <= LL_next;
                LH <= LH_next;
            end if;

            -- Overall timing
            --
            -- Chroma Sub Sampling Off
            --
            --            0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0
            -- L Sample  L0              L1              L2              L3
            -- C Sample  C0              C1              C2              C3
            -- Quad             L0/C0/L1/C1                     L2/C2/L3/C3
            --                            ^                               ^
            --
            -- Chroma Sub Sampling On
            --
            --            0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0
            -- L Sample  L0              L1              L2              L3
            -- C Sample          C0                              C2
            -- Quad             L0/C0/L1/C0                     L2/C2/L3/C2
            --                            ^                               ^
            --
            -- In both cases, we only need to "buffer" CN/LN

            if version = '0' then
                quad  <= VERSION_NUM;
                psync <= FS_I;
            elsif counter(counter'left) = '0' then
                if sample_Q = '1' then
                    quad(11 downto 6) <= BL_next & LL_next & AL_next & BH_next & LH_next & AH_next;
                    quad( 5 downto 0) <= BL & LL & AL & BH & LH & AH;
                end if;
                if counter(3 downto 0) = "0010" then
                    psync    <= counter(4);
                end if;
            else
                quad  <= (others => '0');
                psync <= '0';
            end if;

            if HS3 = '0' and HS2 = '1' then
                start_clamp <= '1';
            else
                start_clamp <= '0';
            end if;

            if start_clamp = '1' then
                -- start at the trailing edge of HSYNC
                clamp_counter <= clamp_size;
            elsif clamp_counter /= "000" and counter(5 downto 0) = ("0" & offset) then
                clamp_counter <= clamp_counter - 1;
            end if;

            -- generate the clamp output
            if clamp_size = "00" then
                clamp <= not(HS1 or HS2);
            elsif clamp_counter = "000" then
                clamp <= '0';
            else
                clamp <= '1';
            end if;

            -- generate the csync output
            csync <= HS2;

        end if;
    end process;

end Behavorial;
