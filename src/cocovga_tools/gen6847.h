#ifndef GEN6847_H
#define GEN6847_H
#include <stdint.h>
#include "../cocovga.h"

typedef struct {
   int fb_w, fb_h, pitch;   // frame buffer geometry
   int ox, oy;              // top left of the 256x192 active picture
   int hs, vs;              // fb pixels per 6847 dot
} gen_geom_t;

typedef enum { GEN_TEXT, GEN_CG1, GEN_CG2, GEN_CG3, GEN_CG6, GEN_RG6 } gen_mode_t;

void gen_frame(uint8_t *fb, const gen_geom_t *g, gen_mode_t mode, const uint8_t *vram, int css, unsigned int noise_seed);
// a text frame with the rows in ext_rows (bit n = row n) drawn with INT/EXT set
void gen_text_ext(uint8_t *fb, const gen_geom_t *g, const uint8_t *vram, int css, unsigned int ext_rows);

#endif
