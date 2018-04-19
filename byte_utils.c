/*
 * byte_utils.c - Functions for dealing with byte format idiosyncrasies.
 *
 * This file is a part of Minipro.
 *
 * Minipro is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Minipro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "byte_utils.h"

void format_int(unsigned char *out, unsigned int in, unsigned char length, unsigned char endianess) {
	int i, idx;
	for(i = 0; i < length; i++) {
		idx = endianess == MP_LITTLE_ENDIAN ? i : length - 1 - i;
		out[i] = (in & 0xFF << idx*8) >> idx*8;
	}
}

int load_int(unsigned char *buf, unsigned char length, unsigned char endianess) {
	int i, idx, result = 0;
	for(i = 0; i < length; i++) {
		idx = endianess == MP_LITTLE_ENDIAN ? i : length - 1 - i;
		result |= (buf[i] << idx*8);
	}
	return(result);
}
