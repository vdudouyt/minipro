/*
 * database.c - Definitions and declarations for dealing with the
 *		device database.
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

#ifndef __DATABASE_H
#define __DATABASE_H

#include <stddef.h>
#include "fuses.h"

typedef struct device {
	const char *name;
	unsigned int protocol_id;
	unsigned int variant;
	size_t read_buffer_size;
	size_t write_buffer_size;
	size_t code_memory_size; // Presenting for every device
	size_t data_memory_size;
	size_t data_memory2_size;
	unsigned int chip_id; // A vendor-specific chip ID (i.e. 0x1E9502 for ATMEGA48)
	unsigned int chip_id_bytes_count : 3;
	unsigned int opts1;
	unsigned int opts2;
	unsigned int opts3;
	unsigned int opts4;
	unsigned int package_details; // pins count or image ID for some devices
	unsigned int write_unlock;

	fuse_decl_t *fuses; // Configuration bytes that's presenting in some architectures
} device_t;

#define WORD_SIZE(device) (((device)->opts4 & 0xFF000000) == 0x01000000 ? 2 : 1)

extern device_t devices[];

device_t *get_device_by_name(const char *name);

#endif
