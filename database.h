#ifndef __DATABASE_H
#define __DATABASE_H

#include <sys/types.h>
#include <inttypes.h>


typedef struct fuse_decl_s {
	const char	*name;
	uint8_t		minipro_cmd;
	uint8_t		length;
	uint32_t	offset;
} fuse_decl_t, *fuse_decl_p;


typedef struct device_s {
	const char	*name;
	uint8_t		protocol_id;
	uint8_t		variant;
	uint32_t	addressing_mode;
	uint32_t	read_buffer_size;
	uint32_t	write_buffer_size;

	uint32_t	code_memory_size; /* Presenting for every device. */
	uint32_t	data_memory_size;
	uint32_t	data_memory2_size;
	uint32_t	chip_id;	/* A vendor-specific chip ID (i.e. 0x1E9502 for ATMEGA48). */
	uint8_t		chip_id_size;	/* chip_id_bytes_count */
	uint16_t	opts1;
	uint16_t	opts2;
	uint16_t	opts3; // XXX
	uint32_t	opts4;
	uint32_t	package_details; /* Pins count or image ID for some devices. */
	uint8_t		write_unlock; // XXX

	fuse_decl_p	fuses;		/* Configuration bytes that's presenting in some architectures. */
} __attribute__((__packed__)) device_t, *device_p;

#define WORD_SIZE(__dev)	((0x01000000 == (0xff000000 & (__dev)->opts4)) ? 2 : 1)
#define DEVICE_NAME_MAX		64 /* Max device name len. */

device_p	dev_db_get_by_name(const char *name);
void		dev_db_dump_flt(const char *dname);

#ifdef DEBUG
void		dev_db_dump_to_h(void);
#endif

#endif
