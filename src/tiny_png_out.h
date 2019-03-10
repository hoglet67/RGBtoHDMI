/*
 * Tiny PNG Output (C)
 *
 * Copyright (c) 2018 Project Nayuki
 * https://www.nayuki.io/page/tiny-png-output
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program (see COPYING.txt and COPYING.LESSER.txt).
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>


// Treat this data structure as private and opaque. Do not read or write any fields directly.
// This structure can be safely discarded at any time, because the functions of this library don't
// allocate memory or resources. The caller is responsible for initializing objects and cleaning up.
struct TinyPngOut {

	// Immutable configuration
	uint32_t width;   // Measured in pixels
	uint32_t height;  // Measured in pixels
	uint32_t lineSize;  // Measured in bytes, equal to (width * 3 + 1)

	// Running state
	uint8_t *output;
   uint32_t output_len;
	uint32_t positionX;      // Next byte index in current line
	uint32_t positionY;      // Line index of next byte
	uint32_t uncompRemain;   // Number of uncompressed bytes remaining
	uint16_t deflateFilled;  // Bytes filled in the current block (0 <= n < DEFLATE_MAX_BLOCK_SIZE)
	uint32_t crc;    // Primarily for IDAT chunk
	uint32_t adler;  // For DEFLATE data within IDAT

};


enum TinyPngOut_Status {
	TINYPNGOUT_OK,
	TINYPNGOUT_INVALID_ARGUMENT,
	TINYPNGOUT_IMAGE_TOO_LARGE,
	TINYPNGOUT_IO_ERROR,
};


/*
 * Creates a PNG writer with the given width and height (both non-zero) and byte output stream.
 * TinyPngOut will leave the output stream still open once it finishes writing the PNG file data.
 * Returns an error if the dimensions exceed certain limits (e.g. w * h > 700 million).
 */
enum TinyPngOut_Status TinyPngOut_init(struct TinyPngOut this[static 1], uint32_t w, uint32_t h, uint8_t *output_buffer);


/*
 * Writes 'count' pixels from the given array to the output stream. This reads count*3
 * bytes from the array. Pixels are presented from top to bottom, left to right, and with
 * subpixels in RGB order. This object keeps track of how many pixels were written and
 * various position variables. It is an error to write more pixels in total than width*height.
 * Once exactly width*height pixels have been written with this TinyPngOut object,
 * there are no more valid operations on the object and it should be discarded.
 */
enum TinyPngOut_Status TinyPngOut_write(struct TinyPngOut this[static 1], const uint8_t pixels[], size_t count);
