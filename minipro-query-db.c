#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "minipro.h"
#include "database.h"
#include "error.h"

void print_help_and_exit(const char *progname) {
	fprintf(stderr, "Usage: %s <device>\n", progname);
	exit(-1);
}

void print_device_info(device_t *device) {
	printf("Name: %s\n", device->name);

	/* Memory shape */
	printf("Memory: %d", device->code_memory_size);
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
		printf(" + %d Bytes", device->data_memory_size);
	}
	if(device->data_memory2_size) {
		printf(" + %d Bytes", device->data_memory2_size);
	}
	printf("\n");

	/* Package info */
	printf("Package: ");
	if(device->package_details & 0xFF) {
		printf("Adapter%d.JPG\n", device->package_details & 0xFF);
	} else {
		printf("DIP%d\n", (device->package_details & 0xFF000000) >> 8*3);
	}

	printf("Protocol: 0x%02x\n", device->protocol_id);
	printf("Read buffer size: %d Bytes\n", device->read_buffer_size);
	printf("Write buffer size: %d Bytes\n", device->write_buffer_size);
}

int main(int argc, char **argv) {
	if(argc != 2) {
		print_help_and_exit(argv[0]);
	}

	device_t *device = get_device_by_name(argv[1]);
	if(!device) {
		ERROR("Unknown device");
	}

	print_device_info(device);
	return(0);
}
