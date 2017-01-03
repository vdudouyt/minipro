#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "minipro.h"
#include "database.h"


static fuse_decl_t avr_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 3, .offset = 0 }, /* Items count, include this. */
	{ .name = "fuses",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 0 },
	{ .name = "lock_byte",	.cmd = 0x41,		.size = 1, .offset = 0 },
};

static fuse_decl_t avr2_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 4, .offset = 0 }, /* Items count, include this. */
	{ .name = "fuses_lo",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 0 },
	{ .name = "fuses_hi",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 1 },
	{ .name = "lock_byte",	.cmd = 0x41,		.size = 1, .offset = 0 },
};

static fuse_decl_t avr3_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 5, .offset = 0 }, /* Items count, include this. */
	{ .name = "fuses_lo",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 0 },
	{ .name = "fuses_hi",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 1 },
	{ .name = "fuses_ext",	.cmd = MP_CMD_READ_CFG,	.size = 1, .offset = 2 },
	{ .name = "lock_byte",	.cmd = 0x41,		.size = 1, .offset = 0 },
};

static fuse_decl_t pic_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 6, .offset = 0 }, /* Items count, include this. */
	{ .name = "user_id0",	.cmd = 0x10,		.size = 2, .offset = 0 },
	{ .name = "user_id1",	.cmd = 0x10,		.size = 2, .offset = 2 },
	{ .name = "user_id2",	.cmd = 0x10,		.size = 2, .offset = 4 },
	{ .name = "user_id3",	.cmd = 0x10,		.size = 2, .offset = 6 },
	{ .name = "conf_word",	.cmd = MP_CMD_READ_CFG,	.size = 2, .offset = 0 },
};

static fuse_decl_t pic2_fuses[] = {
	{ .name = NULL,		.cmd = 0xff,		.size = 7, .offset = 0 }, /* Items count, include this. */
	{ .name = "user_id0",	.cmd = 0x10,		.size = 2, .offset = 0 },
	{ .name = "user_id1",	.cmd = 0x10,		.size = 2, .offset = 2 },
	{ .name = "user_id2",	.cmd = 0x10,		.size = 2, .offset = 4 },
	{ .name = "user_id3",	.cmd = 0x10,		.size = 2, .offset = 6 },
	{ .name = "conf_word",	.cmd = MP_CMD_READ_CFG,	.size = 2, .offset = 0 },
	{ .name = "conf_word1",	.cmd = MP_CMD_READ_CFG,	.size = 2, .offset = 2 },
};


static chip_t chips_array[] = {
	#include "devices.h"
	{ .name = NULL },
};



int
is_chip_id_prob_eq(const chip_p chip, const uint32_t id, const uint8_t id_size) {

	if (0 == chip->chip_id_size || 0 == id_size)
		return ((chip->chip_id_size == id_size));
	if (chip->chip_id_size == id_size)
		return ((chip->chip_id == id));
	if (chip->chip_id_size > id_size)
		return (0);
	/* chip->chip_id_size < id_size */
	return ((chip->chip_id == (id >> (8 * (id_size - chip->chip_id_size)))));
}

int
is_chip_id_eq(const chip_p chip, const uint32_t id, const uint8_t id_size) {

	if (chip->chip_id_size != id_size ||
	    0 == chip->chip_id_size)
		return (0);
	return ((chip->chip_id == id));
}

chip_p
chip_db_get_by_id(const uint32_t chip_id, const uint8_t chip_id_size) {
	chip_p chip;

	if (0 == chip_id_size)
		return (NULL);
	for (chip = chips_array; NULL != chip->name; chip ++) {
		if (is_chip_id_eq(chip, chip_id, chip_id_size))
			return (chip);
	}

	return (NULL);
}

chip_p
chip_db_get_by_name(const char *name) {
	chip_p chip;

	for (chip = chips_array; NULL != chip->name; chip ++) {
		if (!strcasecmp(name, chip->name))
			return (chip);
	}

	return (NULL);
}

void
chip_db_dump_flt(const char *name) {
	chip_p chip;
	size_t name_size = 0;

	if (NULL != name) {
		name_size = strnlen(name, CHIP_NAME_MAX);
	}
	for (chip = chips_array; NULL != chip->name; chip ++) {
		if (0 != name_size &&
		    strncasecmp(chip->name, name, name_size))
			continue;
		printf("0x%04x:	%s\n", chip->chip_id, chip->name);
	}
}

void
chip_db_print_info(const chip_p chip) {

	if (NULL == chip)
		return;

	printf("Name: %s\n", chip->name);

	/* Memory shape */
	printf("Memory: ");
	switch ((0xff000000 & chip->opts4)) {
	case 0x00000000:
		printf("%d Bytes", chip->code_memory_size);
		break;
	case 0x01000000:
		printf("%d Words", (chip->code_memory_size / 2));
		break;
	case 0x02000000:
		printf("%d Bits", chip->code_memory_size);
		break;
	default:
		printf(" unknown memory shape: 0x%x\n", (0xff000000 & chip->opts4));
	}
	if (chip->data_memory_size) {
		printf(" + %d Bytes", chip->data_memory_size);
	}
	if (chip->data_memory2_size) {
		printf(" + %d Bytes", chip->data_memory2_size);
	}
	printf("\n");

	/* Package info */
	printf("Package: ");
	if (0 != (0x000000ff & chip->package_details)) {
		printf("Adapter%03d.JPG\n",
		    (0x000000ff & chip->package_details));
	} else if (0 != (0xff000000 & chip->package_details)) {
		printf("DIP%d\n",
		    ((chip->package_details >> 24) & 0x7f));
	} else {
		printf("ISP only\n");
	}

	/* ISP connection info */
	printf("ISP: ");
	if (0 != (0x0000ff00 & chip->package_details)) {
		printf("ICP%03d.JPG\n",
		    ((chip->package_details >> 8) & 0x000000ff));
	} else {
		printf("-\n");
	}

	printf("Protocol: 0x%02x\n", chip->protocol_id);
	printf("Read buffer size: %d Bytes\n", chip->read_block_size);
	printf("Write buffer size: %d Bytes\n", chip->write_block_size);
}


#ifdef DEBUG
void
chip_db_dump_to_h(void) {
	chip_p chip;

	for (chip = chips_array; chip->name; chip ++) {
		printf("{\n");
		printf("	.name = \"%s\",\n", chip->name);
		printf("	.protocol_id = 0x%02x,\n", (0xffff & chip->protocol_id));
		printf("	.variant = 0x%02x,\n", chip->variant);
		printf("	.read_block_size = 0x%02x,\n", chip->read_block_size);
		printf("	.write_block_size = 0x%02x,\n", chip->write_block_size);
		printf("	.code_memory_size = 0x%02x,\n", chip->code_memory_size);
		printf("	.data_memory_size = 0x%02x,\n", chip->data_memory_size);
		printf("	.data_memory2_size = 0x%02x,\n", chip->data_memory2_size);
		printf("	.chip_id = 0x%02x,\n", chip->chip_id);
		printf("	.chip_id_size = 0x%02x,\n", chip->chip_id_size);
		printf("	.opts1 = 0x%02x,\n", chip->opts1);
		printf("	.opts2 = 0x%02x,\n", chip->opts2);
		printf("	.opts3 = 0x%02x,\n", chip->opts3);
		printf("	.opts4 = 0x%02x,\n", chip->opts4);
		printf("	.package_details = 0x%02x,\n", chip->package_details);
		printf("	.write_unlock = 0x%02x,\n", chip->write_unlock);
		printf("	.fuses = ");
		if (NULL == chip->fuses) {
			printf("NULL,\n");
		} else if (avr_fuses == chip->fuses) {
			printf("avr_fuses,\n");
		} else if (avr2_fuses == chip->fuses) {
			printf("avr2_fuses,\n");
		} else if (avr3_fuses == chip->fuses) {
			printf("avr3_fuses,\n");
		} else if (pic_fuses == chip->fuses) {
			printf("pic_fuses,\n");
		} else if (pic2_fuses == chip->fuses) {
			printf("pic2_fuses,\n");
		} else {
			printf("???,\n"); /* Must not reach here. */
		}
		printf("},\n");
	}
}
#endif
