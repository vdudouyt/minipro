#ifndef __FUSES_H
#define __FUSES_H

typedef struct fuse_decl {
	const char *name;
	char minipro_cmd;
	char length;
	int offset;
} fuse_decl_t;

extern fuse_decl_t avr_fuses[];
extern fuse_decl_t avr2_fuses[];
extern fuse_decl_t avr3_fuses[];
extern fuse_decl_t pic_fuses[];
extern fuse_decl_t pic2_fuses[];

#endif
