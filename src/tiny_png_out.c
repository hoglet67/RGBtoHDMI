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

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "tiny_png_out.h"


static const uint16_t DEFLATE_MAX_BLOCK_SIZE = 65535;

static bool write  (struct TinyPngOut this[static 1], const uint8_t data[], size_t len);
static void crc32  (struct TinyPngOut this[static 1], const uint8_t data[], size_t len);
static void adler32(struct TinyPngOut this[static 1], const uint8_t data[], size_t len);
static void putBigUint32(uint32_t val, uint8_t array[static 4]);


enum TinyPngOut_Status TinyPngOut_init(struct TinyPngOut this[static 1], uint32_t w, uint32_t h, uint8_t *out) {
	// Check arguments
	if (w == 0 || h == 0 || out == NULL)
		return TINYPNGOUT_INVALID_ARGUMENT;
	this->width = w;
	this->height = h;

	// Compute and check data siezs
	uint64_t lineSz = (uint64_t)this->width * 3 + 1;
	if (lineSz > UINT32_MAX)
		return TINYPNGOUT_IMAGE_TOO_LARGE;
	this->lineSize = (uint32_t)lineSz;

	uint64_t uncompRm = this->lineSize * this->height;
	if (uncompRm > UINT32_MAX)
		return TINYPNGOUT_IMAGE_TOO_LARGE;
	this->uncompRemain = (uint32_t)uncompRm;

	uint32_t numBlocks = this->uncompRemain / DEFLATE_MAX_BLOCK_SIZE;
	if (this->uncompRemain % DEFLATE_MAX_BLOCK_SIZE != 0)
		numBlocks++;  // Round up
	// 5 bytes per DEFLATE uncompressed block header, 2 bytes for zlib header, 4 bytes for zlib Adler-32 footer
	uint64_t idatSize = (uint64_t)numBlocks * 5 + 6;
	idatSize += this->uncompRemain;
	if (idatSize > (uint32_t)INT32_MAX)
		return TINYPNGOUT_IMAGE_TOO_LARGE;

	// Write header (not a pure header, but a couple of things concatenated together)
	uint8_t header[] = {  // 43 bytes long
		// PNG header
		0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
		// IHDR chunk
		0x00, 0x00, 0x00, 0x0D,
		0x49, 0x48, 0x44, 0x52,
		0, 0, 0, 0,  // 'width' placeholder
		0, 0, 0, 0,  // 'height' placeholder
		0x08, 0x02, 0x00, 0x00, 0x00,
		0, 0, 0, 0,  // IHDR CRC-32 placeholder
		// IDAT chunk
		0, 0, 0, 0,  // 'idatSize' placeholder
		0x49, 0x44, 0x41, 0x54,
		// DEFLATE data
		0x08, 0x1D,
	};
	putBigUint32(this->width, &header[16]);
	putBigUint32(this->height, &header[20]);
	putBigUint32(idatSize, &header[33]);
	this->crc = 0;
	crc32(this, &header[12], 17);
	putBigUint32(this->crc, &header[29]);
	this->output = out;
	this->output_len = 0;
	if (!write(this, header, sizeof(header) / sizeof(header[0])))
		return TINYPNGOUT_IO_ERROR;

	this->crc = 0;
	crc32(this, &header[37], 6);  // 0xD7245B6B
	this->adler = 1;

	this->positionX = 0;
	this->positionY = 0;
	this->deflateFilled = 0;
	return TINYPNGOUT_OK;
}


enum TinyPngOut_Status TinyPngOut_write(struct TinyPngOut this[static 1], const uint8_t pixels[], size_t count) {
	if (count > SIZE_MAX / 3)
		return TINYPNGOUT_INVALID_ARGUMENT;
	count *= 3;  // Convert pixel count to byte count
	while (count > 0) {
		if (pixels == NULL)
			return TINYPNGOUT_INVALID_ARGUMENT;
		if (this->positionY >= this->height)
			return TINYPNGOUT_INVALID_ARGUMENT;  // All image pixels already written

		if (this->deflateFilled == 0) {  // Start DEFLATE block
			uint16_t size = DEFLATE_MAX_BLOCK_SIZE;
			if (this->uncompRemain < size)
				size = (uint16_t)this->uncompRemain;
			const uint8_t header[] = {  // 5 bytes long
				(uint8_t)(this->uncompRemain <= DEFLATE_MAX_BLOCK_SIZE ? 1 : 0),
				(uint8_t)(size >> 0),
				(uint8_t)(size >> 8),
				(uint8_t)((size >> 0) ^ 0xFF),
				(uint8_t)((size >> 8) ^ 0xFF),
			};
			if (!write(this, header, sizeof(header) / sizeof(header[0])))
				return TINYPNGOUT_IO_ERROR;
			crc32(this, header, sizeof(header) / sizeof(header[0]));
		}
		assert(this->positionX < this->lineSize && this->deflateFilled < DEFLATE_MAX_BLOCK_SIZE);

		if (this->positionX == 0) {  // Beginning of line - write filter method byte
			uint8_t b[] = {0};
			if (!write(this, b, sizeof(b) / sizeof(b[0])))
				return TINYPNGOUT_IO_ERROR;
			crc32(this, b, 1);
			adler32(this, b, 1);
			this->positionX++;
			this->uncompRemain--;
			this->deflateFilled++;

		} else {  // Write some pixel bytes for current line
			uint16_t n = DEFLATE_MAX_BLOCK_SIZE - this->deflateFilled;
			if (this->lineSize - this->positionX < n)
				n = (uint16_t)(this->lineSize - this->positionX);
			if (count < n)
				n = (uint16_t)count;
			assert(n > 0);
			if (!write(this, pixels, n))
				return TINYPNGOUT_IO_ERROR;

			// Update checksums
			crc32(this, pixels, n);
			adler32(this, pixels, n);

			// Increment positions
			count -= n;
			pixels += n;
			this->positionX += n;
			this->uncompRemain -= n;
			this->deflateFilled += n;
		}

		if (this->deflateFilled >= DEFLATE_MAX_BLOCK_SIZE)
			this->deflateFilled = 0;  // End current block

		if (this->positionX == this->lineSize) {  // Increment line
			this->positionX = 0;
			this->positionY++;
			if (this->positionY == this->height) {  // Reached end of pixels
				uint8_t footer[] = {  // 20 bytes long
					0, 0, 0, 0,  // DEFLATE Adler-32 placeholder
					0, 0, 0, 0,  // IDAT CRC-32 placeholder
					// IEND chunk
					0x00, 0x00, 0x00, 0x00,
					0x49, 0x45, 0x4E, 0x44,
					0xAE, 0x42, 0x60, 0x82,
				};
				putBigUint32(this->adler, &footer[0]);
				crc32(this, &footer[0], 4);
				putBigUint32(this->crc, &footer[4]);
				if (!write(this, footer, sizeof(footer) / sizeof(footer[0])))
					return TINYPNGOUT_IO_ERROR;
			}
		}
	}
	return TINYPNGOUT_OK;
}



/*---- Private utility functions ----*/

// Returns whether the write was successful.
static bool write(struct TinyPngOut this[static 1], const uint8_t data[], size_t len) {
   memcpy(this->output + this->output_len, data, len);
   this->output_len += len;
	return 1;
}


// Reads the 'crc' field and updates its value based on the given array of new data.
static void crc32(struct TinyPngOut this[static 1], const uint8_t data[], size_t len) {
	this->crc = ~this->crc;
	for (size_t i = 0; i < len; i++) {
		for (int j = 0; j < 8; j++) {  // Inefficient bitwise implementation, instead of table-based
			uint32_t bit = (this->crc ^ (data[i] >> j)) & 1;
			this->crc = (this->crc >> 1) ^ ((-bit) & UINT32_C(0xEDB88320));
		}
	}
	this->crc = ~this->crc;
}


// Reads the 'adler' field and updates its value based on the given array of new data.
static void adler32(struct TinyPngOut this[static 1], const uint8_t data[], size_t len) {
	uint32_t s1 = this->adler & 0xFFFF;
	uint32_t s2 = this->adler >> 16;
	for (size_t i = 0; i < len; i++) {
		s1 = (s1 + data[i]) % 65521;
		s2 = (s2 + s1) % 65521;
	}
	this->adler = s2 << 16 | s1;
}


static void putBigUint32(uint32_t val, uint8_t array[static 4]) {
	for (int i = 0; i < 4; i++)
		array[i] = (uint8_t)(val >> ((3 - i) * 8));
}
