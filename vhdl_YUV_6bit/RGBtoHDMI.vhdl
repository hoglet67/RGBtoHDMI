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
    signal filter_L   : std_logic;
    signal invert     : std_logic;
    signal subsam_C   : std_logic;
    signal alt_R      : std_logic;
    signal edge       : std_logic;
    signal clamp_size : unsigned (1 downto 0);

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
    signal HS4      : std_logic;
    signal HS5      : std_logic;

    signal LL_S      : std_logic;
    signal LH_S      : std_logic;

    signal AL_S      : std_logic;
    signal AH_S      : std_logic;

    signal BL_S      : std_logic;
    signal BH_S      : std_logic;
     
    signal HS_S      : std_logic;

    signal sample_L  : std_logic;
    signal sample_AB : std_logic;
    signal sample_Q  : std_logic;
     
    signal HS_counter : unsigned(1 downto 0);

begin
    -- Offset is inverted as we count upwards to 0
    offset <= unsigned(sp_reg(6 downto 0) xor "1111111");
    four_level <= sp_reg(7);
    filter_L <= sp_reg(8);
    invert <= sp_reg(9);
    subsam_C <= sp_reg(10);
    alt_R <= sp_reg(11);
    edge <= sp_reg(12);
    clamp_size <= unsigned(sp_reg(14 downto 13));
     
    HS_S <= FS_I when mux = '1' else HS_I;
     
    process(LL_I, LH_I, FS_I)
    begin
    if four_level = '1' then
         if LL_I = '0' and LH_I = '0' and FS_I = '0' then
            LL_S <= '0';
            LH_S <= '0';
         elsif LL_I = '1' and LH_I = '0' and FS_I = '0' then
            LL_S <= '1';
            LH_S <= '0';
         elsif LH_I = '1' and FS_I = '0' then
            LL_S <= '0';
            LH_S <= '1';
         elsif FS_I = '1' then
            LL_S <= '1';
            LH_S <= '1';
         end if;
    else
         LL_S <= LL_I;
         LH_S <= LH_I;
    end if;  
    end process;

    process(AL_I, AH_I, X1_I)
    begin
    if four_level = '1' then
         if AL_I = '0' and AH_I = '0' and X1_I = '0' then
            AL_S <= '0';
            AH_S <= '0';
         elsif AL_I = '1' and AH_I = '0' and X1_I = '0' then
            AL_S <= '1';
            AH_S <= '0';
         elsif AH_I = '1' and X1_I = '0' then
            AL_S <= '0';
            AH_S <= '1';
         elsif X1_I = '1' then
            AL_S <= '1';
            AH_S <= '1';
         end if;
    else
         AL_S <= AL_I;
         AH_S <= AH_I;
    end if;  
    end process;
     
    process(BL_I, BH_I, X2_I)
    begin
    if four_level = '1' then
         if BL_I = '0' and BH_I = '0' and X2_I = '0' then
            BL_S <= '0';
            BH_S <= '0';
         elsif BL_I = '1' and BH_I = '0' and X2_I = '0' then
            BL_S <= '1';
            BH_S <= '0';
         elsif BH_I = '1' and X2_I = '0' then
            BL_S <= '0';
            BH_S <= '1';
         elsif X2_I = '1' then
            BL_S <= '1';
            BH_S <= '1';
         end if;
    else
         BL_S <= BL_I;
         BH_S <= BH_I;
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
    process(AL1, AL2, AL_S,
            AH1, AH2, AH_S,
            BL1, BL2, BL_S,
            BH1, BH2, BH_S,
            LL1, LL2, LL_S,
            LH1, LH2, LH_S,
            filter_L,
            inv_R
            )
    begin
        if filter_L = '1' then
            LL_next <= (LL1 AND LL2) OR (LL1 AND LL_S) OR (LL2 AND LL_S);
            LH_next <= (LH1 AND LH2) OR (LH1 AND LH_S) OR (LH2 AND LH_S);
        else
            LL_next <= LL1;
            LH_next <= LH1;
        end if;
        if inv_R = '1' and AH1 = AL1 then
            AL_next <= not AL1;
            AH_next <= not AH1;
        else
            AL_next <= AL1;
            AH_next <= AH1;
        end if;
        BL_next <= BL1;
        BH_next <= BH1;
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

            AL1 <= AL_S;
            AH1 <= AH_S;
            BL1 <= BL_S;
            BH1 <= BH_S;
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
            elsif sampling = '1' then
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
            if alt_R <= '0' then
                inv_R <= '0';
            elsif HS5 = '1' and HS4 = '0' then
                -- leading edge, toggle PAL inversion if enabled
                inv_R <= not inv_R;
            elsif HS5 = '0' and misc_counter = 0 then
                -- if HSYNC remains low for >= 7 * 128 cycles we have VSYNC
                -- synchronise inv_R when VSYNC detected
                inv_R <= '1';
            end if;

            -- generate the csync output from the most delayed version of HS
            -- (csync adds an extra register, could save this in future)
            csync <= HS5;

        end if;
    end process;

end Behavorial;
