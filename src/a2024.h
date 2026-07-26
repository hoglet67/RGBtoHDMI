#ifndef A2024_H
#define A2024_H

#include <stdint.h>

#define A2024_CONTROL_WIDTH 448
#define A2024_CONTROL_NOMINAL_X 39
#define A2024_CONTROL_WORD_SPACING 128
#define A2024_CONTROL_SKIP 160
#define A2024_LINE_SAMPLES 704
#define A2024_RAW_MASK 0x0fff

extern uint16_t a2024_control_line[A2024_CONTROL_WIDTH];
extern uint16_t a2024_line_buffer[A2024_LINE_SAMPLES];
extern uint16_t a2024_lut[A2024_RAW_MASK + 1];
extern unsigned int a2024_panel_width;
extern unsigned int a2024_hires;
extern int a2024_h_offset_adjust;
extern int a2024_capture_result;

int a2024_decode_control_at(int x);
int a2024_decode_control_fast(void);
int a2024_decode_control_nominal(void);
int a2024_decode_control(void);
void a2024_convert_line(uint8_t *destination, unsigned int pitch,
                        unsigned int limit);

#endif
