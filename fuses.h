#ifndef __FUSES_H
#define __FUSES_H

#include <sys/types.h>
#include <inttypes.h>


typedef struct fuse_decl {
	const char	*name;
	uint8_t		minipro_cmd;
	uint8_t		length;
	uint32_t	offset;
} fuse_decl_t;

extern fuse_decl_t avr_fuses[];
extern fuse_decl_t avr2_fuses[];
extern fuse_decl_t avr3_fuses[];
extern fuse_decl_t pic_fuses[];
extern fuse_decl_t pic2_fuses[];

#endif
