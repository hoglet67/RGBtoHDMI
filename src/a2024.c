#include "a2024.h"

#include <stddef.h>

uint16_t a2024_control_line[A2024_CONTROL_WIDTH]
    __attribute__((aligned(64)));
uint16_t a2024_lut[A2024_RAW_MASK + 1]
    __attribute__((aligned(64)));
unsigned int a2024_panel_width = 512;
unsigned int a2024_hires = 1;
int a2024_h_offset_adjust;
int a2024_capture_result = -1;
uint16_t a2024_line_buffer[A2024_LINE_SAMPLES]
    __attribute__((aligned(64)));

static unsigned int panel_span = 512;
static int control_x = A2024_CONTROL_NOMINAL_X;
static int candidate_x = -1;
static unsigned int candidate_hits;
static unsigned int lut_dpl = 4;

enum {
    A2024_R = 1 << 11,
    A2024_G = 1 << 7,
    A2024_B = 1 << 3,
    A2024_I = 1 << 0,
};

static unsigned int majority_sample(int x)
{
    unsigned int result = 0;
    const unsigned int bits[] = {
        A2024_R, A2024_G, A2024_B, A2024_I,
    };

    for (size_t bit = 0; bit < sizeof(bits) / sizeof(bits[0]); bit++) {
        int count = 0;

        for (int offset = -2; offset <= 2; offset++) {
            unsigned int sample =
                a2024_control_line[x + offset] &
                A2024_RAW_MASK;

            if (sample & bits[bit])
                count++;
        }
        if (count >= 3)
            result |= bits[bit];
    }
    return result;
}

static int stable_sample(int x, unsigned int *value)
{
    unsigned int left = majority_sample(x - 8);
    unsigned int middle = majority_sample(x);
    unsigned int right = majority_sample(x + 8);

    if (left != middle || middle != right)
        return 0;

    *value = middle;
    return 1;
}

static unsigned int apply_dpl(unsigned int value, unsigned int dpl)
{
    if (dpl == 0)
        return 0;
    if (dpl == 1) {
        value &= 1;
        return value | (value << 1);
    }
    if (dpl == 2) {
        value &= 2;
        return value | (value >> 1);
    }
    return value;
}

static void build_lut(unsigned int dpl)
{
    for (unsigned int input = 0;
         input <= A2024_RAW_MASK; input++) {
        unsigned int even = 0;
        unsigned int odd = 0;

        if (input & A2024_R)
            even |= 2;
        if (input & A2024_B)
            even |= 1;
        if (input & A2024_G)
            odd |= 2;
        if (input & A2024_I)
            odd |= 1;

        even = apply_dpl(even, dpl);
        odd = apply_dpl(odd, dpl);
        a2024_lut[input] =
            (uint16_t)(even | (odd << 8));
    }
}

static void clear_control_candidate(void)
{
    candidate_x = -1;
    candidate_hits = 0;
}

static int decode_control_at(int x, int allow_lut_build)
{
    const unsigned int rgbi_mask =
        A2024_R | A2024_G | A2024_B | A2024_I;
    unsigned int word0;
    unsigned int word1;
    unsigned int word2;
    unsigned int word3;
    unsigned int panel_x = 0;
    unsigned int panel_y = 0;
    unsigned int dpl;
    unsigned int f64;

    if (x < 10 ||
        x + 3 * A2024_CONTROL_WORD_SPACING + 10 >=
            A2024_CONTROL_WIDTH)
        return -1;

    if (!stable_sample(x, &word0) ||
        !stable_sample(x + A2024_CONTROL_WORD_SPACING, &word1) ||
        !stable_sample(x + 2 * A2024_CONTROL_WORD_SPACING, &word2) ||
        !stable_sample(x + 3 * A2024_CONTROL_WORD_SPACING, &word3))
        return -1;

    word0 &= rgbi_mask;
    word1 &= rgbi_mask;
    word2 &= rgbi_mask;
    word3 &= rgbi_mask;

    /*
     * The 15 Hz driver writes $001 and the 10 Hz driver writes
     * $801/$f01.  Both 10 Hz values produce R | I on RGBI.
     */
    if (word0 != A2024_I && word0 != (A2024_R | A2024_I))
        return -1;
    if (!(word1 & A2024_I))
        return -1;
    if (word2 != (A2024_R | A2024_G))
        return -1;

    /*
     * BuildColours and the later VBlank update use opposite DPL bits
     * for one-bit mode.  Accept all four DPL values.  The low colour
     * bit supplies I and B is the optional LESS16 flag.
     */
    if (!(word3 & A2024_I))
        return -1;

    f64 = (word0 & A2024_R) != 0;

    if (word1 & A2024_B)
        panel_x |= 2;
    if (word1 & A2024_G)
        panel_x |= 1;
    if (word1 & A2024_R)
        panel_y |= 1;
    if (panel_x > (f64 ? 2U : 1U))
        return -1;

    dpl = 0;
    if (word3 & A2024_R)
        dpl |= 1;
    if (word3 & A2024_G)
        dpl |= 2;
    if (dpl != lut_dpl) {
        if (!allow_lut_build)
            return -1;
        build_lut(dpl);
        lut_dpl = dpl;
    }
    a2024_hires = !f64;
    /*
     * Four-field capture otherwise includes eight border pixels at
     * the start of every panel.  Six-field transfer uses its existing
     * doubled-pixel alignment.
     */
    a2024_h_offset_adjust = f64 ? -16 : 8;
    if (f64) {
        panel_span = panel_x == 2 ? 352 : 336;
        a2024_panel_width = panel_span;
        if ((word3 & A2024_B) && panel_x == 2)
            a2024_panel_width -= 16;
    } else {
        panel_span = 512;
        a2024_panel_width = 512;
        if ((word3 & A2024_B) && panel_x == 1)
            a2024_panel_width -= 16;
    }

    return (int)(panel_x | (panel_y << 2) |
                 ((word3 & A2024_B) ? (1U << 3) : 0) |
                 (f64 ? (1U << 4) : 0));
}

int a2024_decode_control_at(int x)
{
    return decode_control_at(x, 1);
}

int a2024_decode_control_fast(void)
{
    int result = decode_control_at(control_x, 0);

    if (result >= 0)
        clear_control_candidate();
    return result;
}

int a2024_decode_control_nominal(void)
{
    int result =
        a2024_decode_control_at(A2024_CONTROL_NOMINAL_X);

    if (result >= 0) {
        control_x = A2024_CONTROL_NOMINAL_X;
        clear_control_candidate();
    }
    return result;
}

int a2024_decode_control(void)
{
    const int first_x = 10;
    const int last_x =
        A2024_CONTROL_WIDTH -
        3 * A2024_CONTROL_WORD_SPACING - 11;
    int result;

    result = a2024_decode_control_at(control_x);
    if (result >= 0) {
        clear_control_candidate();
        return result;
    }

    if (control_x != A2024_CONTROL_NOMINAL_X) {
        result = a2024_decode_control_nominal();
        if (result >= 0)
            return result;
    }

    for (int x = first_x; x <= last_x; x++) {
        if (x != control_x &&
            x != A2024_CONTROL_NOMINAL_X) {
            result = a2024_decode_control_at(x);
        } else {
            continue;
        }
        if (result >= 0) {
            /*
             * Normal PAL pixels can occasionally resemble a command
             * header.  Require the same shifted position in two
             * consecutive rejected fields before trusting it.
             */
            if (candidate_x == x) {
                candidate_hits++;
            } else {
                candidate_x = x;
                candidate_hits = 1;
            }
            if (candidate_hits >= 2) {
                control_x = x;
                clear_control_candidate();
            }
            return result;
        }
    }

    clear_control_candidate();
    return -1;
}

void a2024_convert_line(uint8_t *destination, unsigned int pitch,
                        unsigned int limit)
{
    uint8_t *second_line = destination + pitch;
    const uint16_t *source = a2024_line_buffer;
    unsigned int width = a2024_panel_width;
    unsigned int span = panel_span;
    unsigned int source_step = a2024_hires ? 1 : 2;

    /*
     * The capture preloader only collects eight samples.  Run the real
     * conversion loop over those samples to warm it without delaying the
     * first active source line by a full panel conversion.
     */
    if (limit && width > limit) {
        width = limit;
        span = limit;
    }

    for (unsigned int x = 0; x < width; x++) {
        uint16_t output =
            a2024_lut[*source & A2024_RAW_MASK];

        destination[x] = (uint8_t)output;
        second_line[x] = (uint8_t)(output >> 8);
        source += source_step;
    }

    for (unsigned int x = width; x < span; x++) {
        destination[x] = 0;
        second_line[x] = 0;
    }
}
