#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "minipro.h"
#include "byte_utils.h"
#include "database.h"
#include "error.h"


static void
print_help_and_exit(const char *progname) {

	fprintf(stderr, "Usage: %s [-s search] [<device>]\n", progname);
	exit(-1);
}

static void
print_device_info(device_p dev) {
	uint8_t package_details[4];

	printf("Name: %s\n", dev->name);

	/* Memory shape */
	printf("Memory: %d", (dev->code_memory_size / WORD_SIZE(dev)));
	switch (dev->opts4 & 0xFF000000) {
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
		ERROR2("Unknown memory shape: 0x%x\n", (dev->opts4 & 0xFF000000));
	}
	if (dev->data_memory_size) {
		printf(" + %d Bytes", dev->data_memory_size);
	}
	if (dev->data_memory2_size) {
		printf(" + %d Bytes", dev->data_memory2_size);
	}
	printf("\n");

	/* Package info */
	format_int(package_details, dev->package_details, 4, MP_LITTLE_ENDIAN);
	printf("Package: ");
	if (package_details[0]) {
		printf("Adapter%03d.JPG\n", package_details[0]);
	} else if(package_details[3]) {
		printf("DIP%d\n", (package_details[3] & 0x7F));
	} else {
		printf("ISP only\n");
	}

	/* ISP connection info */
	printf("ISP: ");
	if (package_details[1]) {
		printf("ICP%03d.JPG\n", package_details[1]);
	} else {
		printf("-\n");
	}

	printf("Protocol: 0x%02x\n", dev->protocol_id);
	printf("Read buffer size: %d Bytes\n", dev->read_buffer_size);
	printf("Write buffer size: %d Bytes\n", dev->write_buffer_size);
}

int
main(int argc, char **argv) {
	device_p dev;

	if (argc < 2) {
		print_help_and_exit(argv[0]);
	}

	if (!strcmp(argv[1], "-s")) {
		if (argc < 3) {
			print_help_and_exit(argv[0]);
		}
		// Listing all devices that starts with argv[2]
		dev_db_dump_flt(argv[2]);
		return (0);
	}

	dev = dev_db_get_by_name(argv[1]);
	if (!dev) {
		ERROR("Unknown device");
	}
	print_device_info(dev);

	return (0);
}
