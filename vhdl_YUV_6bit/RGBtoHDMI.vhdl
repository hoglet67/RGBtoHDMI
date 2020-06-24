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
        -- From YUV Comparators
        VL_I:      in    std_logic;
        VH_I:      in    std_logic;
        UL_I:      in    std_logic;
        UH_I:      in    std_logic;
        YL_I:      in    std_logic;
        YH_I:      in    std_logic;
        HS_I:      in    std_logic;
        FS_I:      in    std_logic;
        X1_I:      in    std_logic;
        X2_I:      in    std_logic;

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
    constant VERSION_NUM  : std_logic_vector(11 downto 0) := x"382";

    -- NOTE: the difference between the leading and trailing offsets is
    -- 256 clks = 32 pixel clocks.

    -- Sampling points
    constant INIT_SAMPLING_POINTS : std_logic_vector(14 downto 0) := "000000110000000";

    -- The counter runs at 8x pixel clock and controls all aspects of sampling
    signal counter  : unsigned(6 downto 0);

    -- This used to be the top bit of counter, but it's really logically seperate
    -- It controls when psync is active
    signal sampling : std_logic;

    -- The misc counter (used for VSYNC detection and Clamp)
    signal misc_tick  : std_logic;
    signal misc_counter : unsigned(2 downto 0);

    -- Sample point register;
    signal sp_reg   : std_logic_vector(14 downto 0) := INIT_SAMPLING_POINTS;

    -- Break out of sp_reg
    signal offset     : unsigned (6 downto 0);
    signal four_level : std_logic;
    signal filter_Y   : std_logic;
    signal invert     : std_logic;
    signal subsam_UV  : std_logic;
    signal alt_V      : std_logic;
    signal edge       : std_logic;
    signal clamp_size : unsigned (1 downto 0);

    -- State to determine whether to invert A
    signal inv_V     : std_logic;

    -- R/PA/PB processing pipeline
    signal VL1      : std_logic;
    signal VH1      : std_logic;
    signal UL1      : std_logic;
    signal UH1      : std_logic;
    signal YL1      : std_logic;
    signal YH1      : std_logic;

    signal VL2      : std_logic;
    signal VH2      : std_logic;
    signal UL2      : std_logic;
    signal UH2      : std_logic;
    signal YL2      : std_logic;
    signal YH2      : std_logic;

    signal VL_next  : std_logic;
    signal VH_next  : std_logic;
    signal UL_next  : std_logic;
    signal UH_next  : std_logic;
    signal YL_next  : std_logic;
    signal YH_next  : std_logic;

    signal VL       : std_logic;
    signal VH       : std_logic;
    signal UL       : std_logic;
    signal UH       : std_logic;
    signal YL       : std_logic;
    signal YH       : std_logic;

    signal HS1      : std_logic;
    signal HS2      : std_logic;
    signal HS3      : std_logic;
    signal HS4      : std_logic;
    signal HS5      : std_logic;

    signal YL_S      : std_logic;
    signal YH_S      : std_logic;

    signal VL_S      : std_logic;
    signal VH_S      : std_logic;

    signal UL_S      : std_logic;
    signal UH_S      : std_logic;

    signal HS_S      : std_logic;

    signal sample_Y  : std_logic;
    signal sample_UV : std_logic;
    signal sample_Q  : std_logic;

    signal HS_counter : unsigned(1 downto 0);

begin
    -- Offset is inverted as we count upwards to 0
    offset <= unsigned(sp_reg(6 downto 0) xor "1111111");
    four_level <= sp_reg(7);
    filter_Y <= sp_reg(8);
    invert <= sp_reg(9);
    subsam_UV <= sp_reg(10);
    alt_V <= sp_reg(11);
    edge <= sp_reg(12);
    clamp_size <= unsigned(sp_reg(14 downto 13));

    HS_S <= FS_I when mux = '1' else HS_I;

    process(YL_I, YH_I, FS_I)
    begin
    if four_level = '1' then
         if YL_I = '0' and YH_I = '0' and FS_I = '0' then
            YL_S <= '0';
            YH_S <= '0';
         elsif YL_I = '1' and YH_I = '0' and FS_I = '0' then
            YL_S <= '1';
            YH_S <= '0';
         elsif YH_I = '1' and FS_I = '0' then
            YL_S <= '0';
            YH_S <= '1';
         elsif FS_I = '1' then
            YL_S <= '1';
            YH_S <= '1';
         end if;
    else
         YL_S <= YL_I;
         YH_S <= YH_I;
    end if;
    end process;

    process(VL_I, VH_I, X1_I)
    begin
    if four_level = '1' then
         if VL_I = '0' and VH_I = '0' and X1_I = '0' then
            VL_S <= '0';
            VH_S <= '0';
         elsif VL_I = '1' and VH_I = '0' and X1_I = '0' then
            VL_S <= '1';
            VH_S <= '0';
         elsif VH_I = '1' and X1_I = '0' then
            VL_S <= '0';
            VH_S <= '1';
         elsif X1_I = '1' then
            VL_S <= '1';
            VH_S <= '1';
         end if;
    else
         VL_S <= VL_I;
         VH_S <= VH_I;
    end if;
    end process;

    process(UL_I, UH_I, X2_I)
    begin
    if four_level = '1' then
         if UL_I = '0' and UH_I = '0' and X2_I = '0' then
            UL_S <= '0';
            UH_S <= '0';
         elsif UL_I = '1' and UH_I = '0' and X2_I = '0' then
            UL_S <= '1';
            UH_S <= '0';
         elsif UH_I = '1' and X2_I = '0' then
            UL_S <= '0';
            UH_S <= '1';
         elsif X2_I = '1' then
            UL_S <= '1';
            UH_S <= '1';
         end if;
    else
         UL_S <= UL_I;
         UH_S <= UH_I;
    end if;
    end process;


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
    process(VL1, VL2, VL_S,
            VH1, VH2, VH_S,
            UL1, UL2, UL_S,
            UH1, UH2, UH_S,
            YL1, YL2, YL_S,
            YH1, YH2, YH_S,
            filter_Y,
            inv_V
            )
    begin
        if filter_Y = '1' then
            YL_next <= (YL1 AND YL2) OR (YL1 AND YL_S) OR (YL2 AND YL_S);
            YH_next <= (YH1 AND YH2) OR (YH1 AND YH_S) OR (YH2 AND YH_S);
        else
            YL_next <= YL1;
            YH_next <= YH1;
        end if;
        if inv_V = '1' and VH1 = VL1 then
            VL_next <= not VL1;
            VH_next <= not VH1;
        else
            VL_next <= VL1;
            VH_next <= VH1;
        end if;
        UL_next <= UL1;
        UH_next <= UH1;
     end process;

    process(clk)
    begin
        if rising_edge(clk) then

            -- synchronize CSYNC to the sampling clock
            HS1 <= HS_S xor invert;

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
            HS4 <= HS3;
            HS5 <= HS4;

            -- Counter is used to find sampling point for first pixel
            if (HS3 = '1' and HS2 = '0' and edge = '1') or (HS3 = '0' and HS2 = '1' and edge = '0') then
                counter <= offset;
            else
                counter <= counter + 1;
            end if;

            if HS3 = '0' and HS2 = '1' then
                -- Stop sampling on the trailing edge of HSYNC
                sampling <= '0';
            elsif counter = "1111111" then
                -- Start sampling when the counter expires
                sampling <= '1';
            end if;

            -- Registers for Chroma / Luma Filtering

            -- TODO: If there are 6 spare registers, it might be worth adding
            -- one more tier of registers, so nothing internal depends directly
            -- on potentially asynchronous inputs.

            VL1 <= VL_S;
            VH1 <= VH_S;
            UL1 <= UL_S;
            UH1 <= UH_S;
            YL1 <= YL_S;
            YH1 <= YH_S;

            VL2 <= VL1;
            VH2 <= VH1;
            UL2 <= UL1;
            UH2 <= UH1;
            YL2 <= YL1;
            YH2 <= YH1;

            -- Pipeline the sample signals to reduce the product terms
            if (subsam_UV = '0' and counter(2 downto 0) = "111") or
               (subsam_UV = '1' and counter(3 downto 0) = "0011") then
                sample_UV <= '1';
            else
                sample_UV <= '0';
            end if;

            if counter(2 downto 0) = "111" then
                sample_Y <= '1';
            else
                sample_Y <= '0';
            end if;

            if counter(3 downto 0) = "1111" then
                sample_Q <= '1';
            else
                sample_Q <= '0';
            end if;

            -- sample colour signal
            if sample_UV = '1' then
                VL <= VL_next;
                VH <= VH_next;
                UL <= UL_next;
                UH <= UH_next;
            end if;

            -- sample luminance signal
            if sample_Y = '1' then
                YL <= YL_next;
                YH <= YH_next;
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
            elsif sampling = '1' then
                if sample_Q = '1' then
                    quad(11 downto 6) <= UL_next & YL_next & VL_next & UH_next & YH_next & VH_next;
                    quad( 5 downto 0) <= UL & YL & VL & UH & YH & VH;
                end if;
                if counter(3 downto 0) = "0010" then
                    psync    <= counter(4);
                end if;
            else
                quad  <= (others => '0');
                psync <= '0';
            end if;

            -- misc_counter has two uses:
            --
            -- 1. During HSYNC it measures the duration of HSYNC as a crude
            --    VSYNC detector, which is used for PAL inversion
            -- 2. After HSYNC it determines the duration of the clamp counter
            --
            -- Misc counter deccrements every 128 cycles
            --
            -- HSYNC is nominally 4.7us long (~32 pixels == 256 cycles)
            -- VSYNC is nominally 256 us long (1792 pixels == 14336 cycles)

            if counter = offset then
                misc_tick <= '1';
            else
                misc_tick <= '0';
            end if;

            if HS5 = '1' and HS4 = '0' then
                -- leading edge, load misc_counter to measure HSYNC duration
                misc_counter <= (others => '1');
            elsif HS5 = '0' and HS4 = '1' then
                -- trailing edge, load misc_counter to generate clamp
                misc_counter <= '0' & clamp_size;
            elsif misc_tick = '1' and misc_counter /= 0 then
                misc_counter <= misc_counter - 1;
            end if;

            -- Generate the clamp output (sp_data overloaded as clamp on/off)
            if clamp_size = 0 then
                clamp <= not(HS1 or HS2) and sp_data;
            elsif misc_counter = 0 then
                clamp <= '0';
            elsif HS4 = '1' then
                clamp <= sp_data;
            end if;

            -- PAL Inversion
            if alt_V <= '0' then
                inv_V <= '0';
            elsif HS5 = '1' and HS4 = '0' then
                -- leading edge, toggle PAL inversion if enabled
                inv_V <= not inv_V;
            elsif HS5 = '0' and misc_counter = 0 then
                -- if HSYNC remains low for >= 7 * 128 cycles we have VSYNC
                -- synchronise inv_V when VSYNC detected
                inv_V <= '1';
            end if;

            -- generate the csync output from the most delayed version of HS
            -- (csync adds an extra register, could save this in future)
            csync <= HS5;

        end if;
    end process;

end Behavorial;
