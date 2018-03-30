#ifndef __DATABASE_H
#define __DATABASE_H

#include "fuses.h"

typedef struct device {
	const char *name;
	unsigned int protocol_id;
	unsigned int variant;
	unsigned int read_buffer_size;
	unsigned int write_buffer_size;
	unsigned int code_memory_size; // Presenting for every device
	unsigned int data_memory_size;
	unsigned int data_memory2_size;
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
