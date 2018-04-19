/*
 * fuses.c - Data for dealing with microcontroller fuses.
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

#include <stdio.h>
#include "fuses.h"
#include "minipro.h"

fuse_decl_t avr_fuses[] = {
	{ .name = "fuses", .minipro_cmd = MP_READ_CFG, .length = 1, .offset = 0 },
	{ .name = "lock_byte", .minipro_cmd = MP_READ_LOCK, .length = 1, .offset = 0 },
	{ .name = NULL },
};

fuse_decl_t avr2_fuses[] = {
	{ .name = "fuses_lo", .minipro_cmd = MP_READ_CFG, .length = 1, .offset = 0 },
	{ .name = "fuses_hi", .minipro_cmd = MP_READ_CFG, .length = 1, .offset = 1 },
	{ .name = "lock_byte", .minipro_cmd = MP_READ_LOCK, .length = 1, .offset = 0 },
	{ .name = NULL },
};

fuse_decl_t avr3_fuses[] = {
	{ .name = "fuses_lo", .minipro_cmd = MP_READ_CFG, .length = 1, .offset = 0 },
	{ .name = "fuses_hi", .minipro_cmd = MP_READ_CFG, .length = 1, .offset = 1 },
	{ .name = "fuses_ext", .minipro_cmd = MP_READ_CFG, .length = 1, .offset = 2 },
	{ .name = "lock_byte", .minipro_cmd = MP_READ_LOCK, .length = 1, .offset = 0 },
	{ .name = NULL },
};

fuse_decl_t pic_fuses[] = {
	{ .name = "user_id0",  .minipro_cmd = MP_READ_USER, .length = 2, .offset = 0 },
	{ .name = "user_id1",  .minipro_cmd = MP_READ_USER, .length = 2, .offset = 2 },
	{ .name = "user_id2",  .minipro_cmd = MP_READ_USER, .length = 2, .offset = 4 },
	{ .name = "user_id3",  .minipro_cmd = MP_READ_USER, .length = 2, .offset = 6 },
	{ .name = "conf_word", .minipro_cmd = MP_READ_CFG, .length = 2, .offset = 0 },
	{ .name = NULL },
};

fuse_decl_t pic2_fuses[] = {
	{ .name = "user_id0",  .minipro_cmd = MP_READ_USER, .length = 2, .offset = 0 },
	{ .name = "user_id1",  .minipro_cmd = MP_READ_USER, .length = 2, .offset = 2 },
	{ .name = "user_id2",  .minipro_cmd = MP_READ_USER, .length = 2, .offset = 4 },
	{ .name = "user_id3",  .minipro_cmd = MP_READ_USER, .length = 2, .offset = 6 },
	{ .name = "conf_word", .minipro_cmd = MP_READ_CFG, .length = 2, .offset = 0 },
	{ .name = "conf_word1", .minipro_cmd = MP_READ_CFG, .length = 2, .offset = 2 },
	{ .name = NULL },
};
