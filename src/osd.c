#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "osd.h"
#include "rpi-mailbox-interface.h"
#include "saa5050_font.h"

#define NLINES 4
#define LINELEN 20

static char buffer[LINELEN * NLINES];

static uint32_t mapping_table0[0x1000];
static uint32_t mapping_table1[0x1000];
static uint32_t mapping_table2[0x1000];

static int active = 0;

void osd_init() {
   // Precalculate character->screen mapping table
   // char bit 0 -> mapping_table1 bits 31, 27
   // ...
   // char bit 3 -> mapping_table1 bits 7, 3
   // char bit 4 -> mapping_table0 bits 31, 27
   // ...
   // char bit 7 -> mapping_table0 bits 7, 3
   for (int i = 0; i <= 0xFFF; i++) {
      mapping_table0[i] = 0;
      mapping_table1[i] = 0;
      mapping_table2[i] = 0;
      for (int j = 0; j < 12; j++) {
         if (i & (1 << j)) {
            if (j < 4) {
               mapping_table2[i] |= 0x88 << (8 * (3 - j));
            } else if (j < 8) {
               mapping_table1[i] |= 0x88 << (8 * (7 - j));
            } else {               
               mapping_table0[i] |= 0x88 << (8 * (11 - j));
            }
         }
      }
   }
}

void osd_clear() {
   if (active) {
      active = 0;
      RPI_PropertyInit();
      RPI_PropertyAddTag( TAG_SET_PALETTE );
      RPI_PropertyProcess();
      memset(buffer, 0, sizeof(buffer));
   }
}

void osd_set(int line, char *text) {
   if (!active) {
      active = 1;
      RPI_PropertyInit();
      RPI_PropertyAddTag( TAG_SET_PALETTE );
      RPI_PropertyProcess();
   }
   memset(buffer + line * LINELEN, 0, LINELEN);
   int len = strlen(text);
   if (len > LINELEN) {
      len = LINELEN;
   }
   strncpy(buffer + line * LINELEN, text, len);
}


int osd_active() {
   return active;
}

void osd_update(uint32_t *osd_base, int bytes_per_line) {
   // SAA5050 character data is 12x20
   uint32_t *line_ptr = osd_base;
   int words_per_line = bytes_per_line >> 2;
   for (int line = 0; line < NLINES; line++) {
      for (int y = 0; y < 20; y++) {
         uint32_t *word_ptr = line_ptr;
         for (int i = 0; i < LINELEN; i++) {
            int c = buffer[line * LINELEN + i];
            // Deal with unprintable characters
            if (c < 32 || c > 127) {
               c = 32;
            }
            // Row is 8 pixels
            int data = fontdata[32 * c + y] & 0x3ff;
            // Map to the screen pixel format
            uint32_t word0 = mapping_table0[data];
            uint32_t word1 = mapping_table1[data];
            uint32_t word2 = mapping_table2[data];
            *word_ptr &= 0x77777777;
            *word_ptr |= word0;
            *(word_ptr + words_per_line) &= 0x77777777;;
            *(word_ptr + words_per_line) |= word0;
            word_ptr++;
            *word_ptr &= 0x77777777;
            *word_ptr |= word1;
            *(word_ptr + words_per_line) &= 0x77777777;;
            *(word_ptr + words_per_line) |= word1;
            word_ptr++;
            *word_ptr &= 0x77777777;
            *word_ptr |= word2;
            *(word_ptr + words_per_line) &= 0x77777777;;
            *(word_ptr + words_per_line) |= word2;
            word_ptr++;
         }
         line_ptr += 2 * words_per_line;
      }
   }
}
