#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "minipro.h"
#include "database.h"


static fuse_decl_t avr_fuses[] = {
	{ .name = "fuses",	.minipro_cmd = MP_CMD_READ_CFG,	.length = 1, .offset = 0 },
	{ .name = "lock_byte",	.minipro_cmd = 0x41,		.length = 1, .offset = 0 },
	{ .name = NULL },
};

static fuse_decl_t avr2_fuses[] = {
	{ .name = "fuses_lo",	.minipro_cmd = MP_CMD_READ_CFG,	.length = 1, .offset = 0 },
	{ .name = "fuses_hi",	.minipro_cmd = MP_CMD_READ_CFG,	.length = 1, .offset = 1 },
	{ .name = "lock_byte",	.minipro_cmd = 0x41,		.length = 1, .offset = 0 },
	{ .name = NULL },
};

static fuse_decl_t avr3_fuses[] = {
	{ .name = "fuses_lo",	.minipro_cmd = MP_CMD_READ_CFG,	.length = 1, .offset = 0 },
	{ .name = "fuses_hi",	.minipro_cmd = MP_CMD_READ_CFG,	.length = 1, .offset = 1 },
	{ .name = "fuses_ext",	.minipro_cmd = MP_CMD_READ_CFG,	.length = 1, .offset = 2 },
	{ .name = "lock_byte",	.minipro_cmd = 0x41,		.length = 1, .offset = 0 },
	{ .name = NULL },
};

static fuse_decl_t pic_fuses[] = {
	{ .name = "user_id0",	.minipro_cmd = 0x10,		.length = 2, .offset = 0 },
	{ .name = "user_id1",	.minipro_cmd = 0x10,		.length = 2, .offset = 2 },
	{ .name = "user_id2",	.minipro_cmd = 0x10,		.length = 2, .offset = 4 },
	{ .name = "user_id3",	.minipro_cmd = 0x10,		.length = 2, .offset = 6 },
	{ .name = "conf_word",	.minipro_cmd = MP_CMD_READ_CFG,	.length = 2, .offset = 0 },
	{ .name = NULL },
};

static fuse_decl_t pic2_fuses[] = {
	{ .name = "user_id0",	.minipro_cmd = 0x10,		.length = 2, .offset = 0 },
	{ .name = "user_id1",	.minipro_cmd = 0x10,		.length = 2, .offset = 2 },
	{ .name = "user_id2",	.minipro_cmd = 0x10,		.length = 2, .offset = 4 },
	{ .name = "user_id3",	.minipro_cmd = 0x10,		.length = 2, .offset = 6 },
	{ .name = "conf_word",	.minipro_cmd = MP_CMD_READ_CFG,	.length = 2, .offset = 0 },
	{ .name = "conf_word1",	.minipro_cmd = MP_CMD_READ_CFG,	.length = 2, .offset = 2 },
	{ .name = NULL },
};


static device_t devices[] = {
	#include "devices.h"
	{ .name = NULL },
};


device_p
dev_db_get_by_name(const char *name) {
	device_p dev;

	for (dev = devices; dev->name; dev ++) {
		if (!strcasecmp(name, dev->name))
			return (dev);
	}

	return (NULL);
}

void
dev_db_dump_flt(const char *dname) {
	size_t dname_size = 0;
	device_p dev;

	if (NULL != dname) {
		dname_size = strnlen(dname, DEVICE_NAME_MAX);
	}
	for (dev = devices; dev->name; dev ++) {
		if (0 != dname_size &&
		    strncasecmp(dev->name, dname, dname_size))
			continue;
		printf("0x%02x: %s\n", dev->chip_id, dev->name);
	}
}

#ifdef DEBUG
void
dev_db_dump_to_h(void) {
	device_p dev;

	for (dev = devices; dev->name; dev ++) {
		printf("{\n");
		printf("	.name = \"%s\",\n", dev->name);
		printf("	.protocol_id = 0x%02x,\n", (0xffff & dev->protocol_id));
		printf("	.variant = 0x%02x,\n", dev->variant);
		printf("	.read_buffer_size = 0x%02x,\n", dev->read_buffer_size);
		printf("	.write_buffer_size = 0x%02x,\n", dev->write_buffer_size);
		printf("	.code_memory_size = 0x%02x,\n", dev->code_memory_size);
		printf("	.data_memory_size = 0x%02x,\n", dev->data_memory_size);
		printf("	.data_memory2_size = 0x%02x,\n", dev->data_memory2_size);
		printf("	.chip_id = 0x%02x,\n", dev->chip_id);
		printf("	.chip_id_size = 0x%02x,\n", dev->chip_id_size);
		printf("	.opts1 = 0x%02x,\n", dev->opts1);
		printf("	.opts2 = 0x%02x,\n", dev->opts2);
		printf("	.opts3 = 0x%02x,\n", dev->opts3);
		printf("	.opts4 = 0x%02x,\n", dev->opts4);
		printf("	.package_details = 0x%02x,\n", dev->package_details);
		printf("	.write_unlock = 0x%02x,\n", dev->write_unlock);
		printf("	.fuses = ");
		switch (dev->protocol_id) {
		case 0x71:
			switch (dev->variant) {
			case 0x01:
			case 0x21:
			case 0x44:
			case 0x61:
				printf("&avr_fuses,\n");
				break;
			case 0x00:
			case 0x20:
			case 0x22:
			case 0x43:
			case 0x85:
				printf("&avr2_fuses,\n");
				break;
			case 0x0a:
			case 0x2a:
			case 0x48:
			case 0x49:
			case 0x6b:
				printf("&avr3_fuses,\n");
				break;
			default:
				printf("NULL,\n");
			}
			break;
		case 0x10063: // select 2 fuses
			printf("&pic2_fuses,\n");
			//dev->protocol_id &= 0xFFFF;
			break;
		case 0x63:
		case 0x65:
			printf("&pic_fuses,\n");
			break;
		default:
			printf("NULL,\n");
			break;
		}
		printf("},\n");
	}
}
#endif
