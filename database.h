#ifndef __DATABASE_H
#define __DATABASE_H

#include <sys/types.h>
#include <inttypes.h>
#include "fuses.h"


typedef struct device_s {
	const char	*name;
	uint8_t		protocol_id;
	uint8_t		variant;
	uint32_t	addressing_mode;
	uint32_t	read_buffer_size;
	uint32_t	write_buffer_size;
	enum { BYTE, WORD, BIT } word_size;

	uint32_t	code_memory_size; /* Presenting for every device. */
	uint32_t	data_memory_size;
	uint32_t	data_memory2_size;
	uint32_t	chip_id;	/* A vendor-specific chip ID (i.e. 0x1E9502 for ATMEGA48). */
	uint32_t	chip_id_bytes_count:3;
	uint16_t	opts1;
	uint16_t	opts2;
	uint16_t	opts3;
	uint32_t	opts4;
	uint32_t	package_details; /* Pins count or image ID for some devices. */
	uint8_t		write_unlock;

	fuse_decl_t	*fuses;		/* Configuration bytes that's presenting in some architectures. */
} device_t, *device_p;

#define WORD_SIZE(__dev)	(((__dev)->opts4 & 0xFF000000) == 0x01000000 ? 2 : 1)
#define DEVICE_NAME_MAX		64 /* Max device name len. */

device_p	dev_db_get_by_name(const char *name);
void		dev_db_dump_flt(const char *dname);

#endif
