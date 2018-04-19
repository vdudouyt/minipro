/*
 * fuses.h - Defintions and structures for dealing with microcontroller fuses.
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
