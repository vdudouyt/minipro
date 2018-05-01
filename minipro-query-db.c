/*
 * minipro-query-db.c - Standalone program to search the device database.
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
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "minipro.h"
#include "byte_utils.h"
#include "database.h"
#include "error.h"

void print_help_and_exit(const char *progname) {
	fprintf(stderr, "Usage: %s [-s search] [<device>]\n", progname);
	exit(-1);
}

void print_device_info(device_t *device) {
	printf("Name: %s\n", device->name);

	/* Memory shape */
	printf("Memory: %zu", device->code_memory_size / WORD_SIZE(device));
	switch(device->opts4 & 0xFF000000) {
		case 0x00000000:
			printf(" Bytes");
			break;
		case 0x01000000:
			printf(" Words");
			break;
		case 0x02000000:
			printf(" Bits");
			break;
		default:
			ERROR2("Unknown memory shape: 0x%x\n", device->opts4 & 0xFF000000);
	}
	if(device->data_memory_size) {
		printf(" + %zu Bytes", device->data_memory_size);
	}
	if(device->data_memory2_size) {
		printf(" + %zu Bytes", device->data_memory2_size);
	}
	printf("\n");

	unsigned char package_details[4];
	format_int(package_details, device->package_details, 4, MP_LITTLE_ENDIAN);
	/* Package info */
	printf("Package: ");
	if(package_details[0]) {
		printf("Adapter%03d.JPG\n", package_details[0]);
	} else if(package_details[3]) {
		printf("DIP%d\n", package_details[3] & 0x7F);
	} else {
		printf("ISP only\n");
	}

	/* ISP connection info */
	printf("ISP: ");
	if(package_details[1]) {
		printf("ICP%03d.JPG\n", package_details[1]);
	} else {
		printf("-\n");
	}

	printf("Protocol: 0x%02x\n", device->protocol_id);
	printf("Read buffer size: %zu Bytes\n", device->read_buffer_size);
	printf("Write buffer size: %zu Bytes\n", device->write_buffer_size);
}

int main(int argc, char **argv) {
	if(argc < 2) {
		print_help_and_exit(argv[0]);
	}

	if(!strcmp(argv[1], "-s")) {
		if(argc < 3) {
			print_help_and_exit(argv[0]);
		}
		// Listing all devices that starts with argv[2]
		device_t *device;
		for(device = &(devices[0]); device[0].name; device = &(device[1])) {
			if(!strncasecmp(device[0].name, argv[2], strlen(argv[2]))) {
				printf("%s\n", device[0].name);
			}
		}
		return(0);
	}

	device_t *device = get_device_by_name(argv[1]);
	if(!device) {
		ERROR("Unknown device");
	}

	print_device_info(device);
	return(0);
}
