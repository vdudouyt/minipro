#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>

#include "version.h"
#include "minipro.h"
#include "database.h"
#include "byte_utils.h"
#include "easyconfig.h"
#include "error.h"


static struct {
	void (*action) (const char *, minipro_p handle, device_p device);
	char *filename;
	device_p device;
	enum { UNSPECIFIED = 0, CODE, DATA, CONFIG } page;
	int erase;
	int protect_off;
	int protect_on;
	int size_error;
	int size_nowarn;
	int verify;
	uint8_t icsp;
	int idcheck_continue;
} cmdopts;


void	action_read(const char *filename, minipro_p handle, device_p device);
void	action_write(const char *filename, minipro_p handle, device_p device);


static void
print_help_and_exit(const char *progname) {
	const char *usage =
		"minipro version %s     A free and open TL866XX programmer\n"
		"Usage: %s [options]\n"
		"options:\n"
		"	-l		List all supported devices\n"
		"	-r <filename>	Read memory\n"
		"	-w <filename>	Write memory\n"
		"	-e 		Do NOT erase device\n"
		"	-u 		Do NOT disable write-protect\n"
		"	-P 		Do NOT enable write-protect\n"
		"	-v		Do NOT verify after write\n"
		"	-p <device>	Specify device (use quotes)\n"
		"	-c <type>	Specify memory type (optional)\n"
		"			Possible values: code, data, config\n"
		"	-i		Use ICSP\n"
		"	-I		Use ICSP (without enabling Vcc)\n"
		"	-s		Do NOT error on file size mismatch (only a warning)\n"
		"	-S		No warning message for file size mismatch (can't combine with -s)\n"
		"	-y		Do NOT error on ID mismatch\n";
	fprintf(stderr, usage, VERSION, basename(progname));

#ifdef DEBUG
	dev_db_dump_to_h();
#endif
	exit(-1);
}

static void
print_devices_and_exit() {

	dev_db_dump_flt(NULL);
	exit(-1);
}

static void
parse_cmdline(int argc, char **argv) {
	int c;

	memset(&cmdopts, 0x00, sizeof(cmdopts));
	while ((c = getopt(argc, argv, "leuPvyr:w:p:c:iIsS")) != -1) {
		switch (c) {
		case 'l':
			print_devices_and_exit();
			break;
		case 'e':
			cmdopts.erase = 1; // 1= do not erase
			break;
		case 'u':
			cmdopts.protect_off = 1; // 1= do not disable write protect
			break;
		case 'P':
			cmdopts.protect_on = 1; // 1= do not enable write protect
			break;
		case 'v':
			cmdopts.verify = 1; // 1= do not verify
			break;
		case 'y':
			cmdopts.idcheck_continue = 1; // 1= do not stop on id mismatch
			break;
		case 'p':
			if (!strcmp(optarg, "help"))
				print_devices_and_exit();
			cmdopts.device = dev_db_get_by_name(optarg);
			if (!cmdopts.device)
				ERROR("Unknown device");
			break;
		case 'c':
			if (!strcmp(optarg, "code"))
				cmdopts.page = CODE;
			if (!strcmp(optarg, "data"))
				cmdopts.page = DATA;
			if (!strcmp(optarg, "config"))
				cmdopts.page = CONFIG;
			if (!cmdopts.page)
				ERROR("Unknown memory type");
			break;
		case 'r':
			cmdopts.action = action_read;
			cmdopts.filename = optarg;
			break;
		case 'w':
			cmdopts.action = action_write;
			cmdopts.filename = optarg;
			break;
		case 'i':
			cmdopts.icsp = (MP_ICSP_FLAG_ENABLE | MP_ICSP_FLAG_VCC);
			break;
		case 'I':
			cmdopts.icsp = MP_ICSP_FLAG_ENABLE;
			break;
		case 'S':
			cmdopts.size_nowarn = 1;
			cmdopts.size_error = 1;
			break;
		case 's':
			cmdopts.size_error = 1;
			break;
		default:
			print_help_and_exit(argv[0]);
		}
	}
}

static int
get_file_size(const char *filename) {
	FILE *file = fopen(filename, "r");
	if(!file) {
		PERROR("Couldn't open file");
	}

	fseek(file, 0, SEEK_END);
	int size = ftell(file);
	fseek(file, 0, SEEK_SET);

	fclose(file);
	return (size);
}

static void
update_status(char *status_msg, char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	printf("\r\e[K%s", status_msg);
	vprintf(fmt, args);
	fflush(stdout);
}

static int
compare_memory(uint8_t *buf1, uint8_t *buf2, int size, uint8_t *c1, uint8_t *c2) {
	int i;

	for (i = 0; i < size; i ++) {
		if (buf1[i] != buf2[i]) {
			*c1 = buf1[i];
			*c2 = buf2[i];
			return (i);
		}
	}

	return (-1);
}

/* RAM-centric IO operations */
static void
read_page_ram(minipro_p handle, uint8_t *buf, uint8_t cmd, const char *name, int size) {
	int i, blocks_count, addr, len;
	char status_msg[24];

	sprintf(status_msg, "Reading %s... ", name);

	device_p device = minipro_device_get(handle);
	blocks_count = size / device->read_buffer_size;
	if ((size % device->read_buffer_size) != 0) {
		blocks_count ++;
	}

	for (i = 0; i < blocks_count; i ++) {
		update_status(status_msg, "%2d%%", ((i * 100) / blocks_count));
		// Translating address to protocol-specific
		addr = (i * device->read_buffer_size);
		if (device->opts4 & 0x2000) {
			addr = (addr >> 1);
		}
		len = device->read_buffer_size;
		// Last block
		if (((i + 1) * len) > size) {
			len = (size % len);
		}
		minipro_read_block(handle, cmd, addr, (buf + (i * device->read_buffer_size)), len);
	}

	update_status(status_msg, "OK\n");
}

static void
write_page_ram(minipro_p handle, const uint8_t *buf, uint8_t cmd, const char *name, int size) {
	int i, blocks_count, addr, len;
	char status_msg[24];

	sprintf(status_msg, "Writing %s... ", name);

	device_p device = minipro_device_get(handle);

	blocks_count = (size / device->write_buffer_size);
	if ((size % device->write_buffer_size) != 0) {
		blocks_count ++;
	}

	for (i = 0; i < blocks_count; i ++) {
		update_status(status_msg, "%2d%%", ((i * 100) / blocks_count));
		// Translating address to protocol-specific
		addr = (i * device->write_buffer_size);
		if (device->opts4 & 0x2000) {
			addr = (addr >> 1);
		}
		len = device->write_buffer_size;
		// Last block
		if (((i + 1) * len) > size) {
			len = (size % len);
		}
		minipro_write_block(handle, cmd, addr, (buf + (i * device->write_buffer_size)), len);
	}
	update_status(status_msg, "OK\n");
}

/* Wrappers for operating with files */
static void
read_page_file(minipro_p handle, const char *filename, uint8_t cmd, const char *name, int size) {
	FILE *file = fopen(filename, "w");

	if (file == NULL) {
		PERROR("Couldn't open file for writing");
	}

	uint8_t *buf = malloc(size);
	if (!buf) {
		ERROR("Can't malloc");
	}

	read_page_ram(handle, buf, cmd, name, size);
	fwrite(buf, 1, size, file);

	fclose(file);
	free(buf);
}

static void
write_page_file(minipro_p handle, const char *filename, uint8_t cmd, const char *name, int size) {
	FILE *file = fopen(filename, "r");

	if (file == NULL) {
		PERROR("Couldn't open file for reading");
	}

	uint8_t *buf = malloc(size);
	if (!buf) {
		ERROR("Can't malloc");
	}

	if (fread(buf, 1, size, file) != size && !cmdopts.size_error) {
		ERROR("Short read");
	}
	write_page_ram(handle, buf, cmd, name, size);

	fclose(file);
	free(buf);
}

static void
read_fuses(minipro_p handle, const char *filename, fuse_decl_p fuses) {
	int i, d;
	uint8_t data_length = 0, opcode = fuses[0].minipro_cmd;
	uint8_t buf[11];

	printf("Reading fuses... ");
	fflush(stdout);

	if (Config_init(filename)) {
		PERROR("Couldn't create config");
	}

	minipro_begin_transaction(handle);
	for (i = 0; fuses[i].name; i ++) {
		data_length += fuses[i].length;
		if (fuses[i].minipro_cmd < opcode) {
			ERROR("fuse_decls are not sorted");
		}
		if (fuses[i + 1].name == NULL || fuses[i + 1].minipro_cmd > opcode) {
			minipro_read_fuses(handle, opcode, data_length, buf);
			// Unpacking received buf[] accordingly to fuse_decls with same minipro_cmd
			for (d = 0; fuses[d].name; d ++) {
				if (fuses[d].minipro_cmd != opcode) {
					continue;
				}
				int value = load_int(&(buf[fuses[d].offset]), fuses[d].length, MP_LITTLE_ENDIAN);
				if (Config_set_int(fuses[d].name, value) == -1)
					ERROR("Couldn't set configuration");
			}

			opcode = fuses[i+1].minipro_cmd;
			data_length = 0;
		}
	}
	minipro_end_transaction(handle);

	Config_close();
	printf("OK\n");
}

static void
write_fuses(minipro_p handle, const char *filename, fuse_decl_p fuses) {

	printf("Writing fuses... ");
	fflush(stdout);

	if (Config_open(filename)) {
		PERROR("Couldn't parse config");
	}

	minipro_begin_transaction(handle);
	int i, d;
	uint8_t data_length = 0, opcode = fuses[0].minipro_cmd;
	uint8_t buf[11];
	for (i = 0; fuses[i].name; i ++) {
		data_length += fuses[i].length;
		if(fuses[i].minipro_cmd < opcode) {
			ERROR("fuse_decls are not sorted");
		}
		if (fuses[i + 1].name == NULL || fuses[i + 1].minipro_cmd > opcode) {
			for(d = 0; fuses[d].name; d ++) {
				if(fuses[d].minipro_cmd != opcode) {
					continue;
				}
				int value = Config_get_int(fuses[d].name);
				if (value == -1)
					ERROR("Could not read configuration");
				format_int(&(buf[fuses[d].offset]), value, fuses[d].length, MP_LITTLE_ENDIAN);
			}
			if (0 != minipro_write_fuses(handle, opcode, data_length, buf))
				ERROR("Failed while writing config bytes");

			opcode = fuses[i + 1].minipro_cmd;
			data_length = 0;
		}
	}
	minipro_end_transaction(handle);

	Config_close();
	printf("OK\n");
}

static void
verify_page_file(minipro_p handle, const char *filename, uint8_t cmd, const char *name, int size) {
	FILE *file = fopen(filename, "r");

	if(file == NULL) {
		PERROR("Couldn't open file for reading");
	}

	/* Loading file */
	int file_size = get_file_size(filename);
	uint8_t *file_data = malloc(file_size);
	if (fread(file_data, 1, file_size, file) != file_size) {
		ERROR("Short read");
	}
	fclose(file);

	minipro_begin_transaction(handle);

	/* Downloading data from chip*/
	uint8_t *chip_data = malloc(size);
	if (cmdopts.size_error)
		read_page_ram(handle, chip_data, cmd, name, file_size);
	else
		read_page_ram(handle, chip_data, cmd, name, size);
	minipro_end_transaction(handle);

	uint8_t c1, c2;
	int idx = compare_memory(file_data, chip_data, file_size, &c1, &c2);

	/* No memory leaks */
	free(file_data);
	free(chip_data);

	if (idx != -1) {
		ERROR_FMT("Verification failed at 0x%02x: 0x%02x != 0x%02x\n", idx, c1, c2);
	} else {
		printf("Verification OK\n");
	}
}

/* Higher-level logic */
void
action_read(const char *filename, minipro_p handle, device_p device) {
	char *code_filename = (char*)filename;
	char *data_filename = (char*)filename;
	char *config_filename = (char*)filename;
	char default_data_filename[] = "eeprom.bin";
	char default_config_filename[] = "fuses.conf";

	minipro_begin_transaction(handle); // Prevent device from hanging
	switch (cmdopts.page) {
	case UNSPECIFIED:
		data_filename = default_data_filename;
		config_filename = default_config_filename;
	case CODE:
		read_page_file(handle, code_filename, MP_CMD_READ_CODE, "Code", device->code_memory_size);
		if (cmdopts.page)
			break;
		/* PASSTROUTH. */
	case DATA:
		if (device->data_memory_size) {
			read_page_file(handle, data_filename, MP_CMD_READ_DATA, "Data", device->data_memory_size);
		}
		if (cmdopts.page)
			break;
		/* PASSTROUTH. */
	case CONFIG:
		if (device->fuses) {
			read_fuses(handle, config_filename, device->fuses);
		}
		break;
	}
	minipro_end_transaction(handle); 
}

void
action_write(const char *filename, minipro_p handle, device_p device) {
	int fsize;
	uint16_t status;

	switch (cmdopts.page) {
	case UNSPECIFIED:
	case CODE:
		fsize = get_file_size(filename);
		if (fsize != device->code_memory_size) {
			if (!cmdopts.size_error)
				ERROR_FMT("Incorrect file size: %d (needed %d)\n", fsize, device->code_memory_size);
			else if (cmdopts.size_nowarn == 0)
				printf("Warning: Incorrect file size: %d (needed %d)\n", fsize, device->code_memory_size);
		}
		break;
	case DATA:
		fsize = get_file_size(filename);
		if (fsize != device->data_memory_size) {
			if (!cmdopts.size_error)
				ERROR_FMT("Incorrect file size: %d (needed %d)\n", fsize, device->data_memory_size);
			else if (cmdopts.size_nowarn == 0) 
				printf("Warning: Incorrect file size: %d (needed %d)\n", fsize, device->data_memory_size);
		}
		break;
	case CONFIG:
		break;
	}
	minipro_begin_transaction(handle);
	if (cmdopts.erase == 0) {
		minipro_prepare_writing(handle);
		minipro_end_transaction(handle); // Let prepare_writing() to take an effect
	}

	minipro_begin_transaction(handle);
	if (-1 == minipro_get_status(handle, &status))
		ERROR("Overcurrency protection");
	if (cmdopts.protect_off == 0 && (device->opts4 & 0xc000)) {
		minipro_protect_off(handle);
	}

	switch (cmdopts.page) {
	case UNSPECIFIED:
	case CODE:
		write_page_file(handle, filename, MP_CMD_WRITE_CODE, "Code", device->code_memory_size);
		if (cmdopts.verify == 0)
			verify_page_file(handle, filename, MP_CMD_READ_CODE, "Code", device->code_memory_size);
		break;
	case DATA:
		write_page_file(handle, filename, MP_CMD_WRITE_DATA, "Data", device->data_memory_size);
		if (cmdopts.verify == 0)
			verify_page_file(handle, filename, MP_CMD_READ_DATA, "Data", device->data_memory_size);
		break;
	case CONFIG:
		if (device->fuses) {
			write_fuses(handle, filename, device->fuses);
		}
		break;
	}
	minipro_end_transaction(handle); // Let prepare_writing() to make an effect

	if (cmdopts.protect_on == 0 && (device->opts4 & 0xc000)) {
		minipro_begin_transaction(handle);
		minipro_protect_on(handle);
		minipro_end_transaction(handle);
	}
}

int
main(int argc, char **argv) {
	minipro_p handle;
	device_p device;
	uint32_t chip_id;
	uint8_t chip_id_size;
	minipro_ver_t ver;
	const char *dev_ver_str;
	char str_buf[64];

	parse_cmdline(argc, argv);
	if (!cmdopts.filename) {
		print_help_and_exit(argv[0]);
	}
	if (cmdopts.action && !cmdopts.device) {
		USAGE_ERROR("Device required");
	}
	device = cmdopts.device;

	/* Open MiniPro device. */
	if (0 != minipro_open(MP_TL866_VID, MP_TL866_PID, device,
	    cmdopts.icsp, 1, &handle)) {
		ERROR("minipro_open()");
	}

	/* Printing system info. */
	if (0 != minipro_get_version_info(handle, &ver)) {
		ERROR("minipro_get_version_info()");
	}
	/* Model/dev ver preprocess. */
	switch (ver.device_version) {
	case MP_DEV_VER_TL866A:
	case MP_DEV_VER_TL866CS:
		dev_ver_str = minipro_dev_ver_str[ver.device_version];
		break;
	default:
		snprintf(str_buf, sizeof(str_buf), "%s (ver = %"PRIu8")",
		    minipro_dev_ver_str[MP_DEV_VER_TL866_UNKNOWN],
		    ver.device_version);
		dev_ver_str = (const char*)str_buf;
	}
	printf("Found Minipro: %s, fw: v%02d.%"PRIu8".%"PRIu8"%s, code: %.*s, serial: %.*s, proto: %"PRIu8"\n",
	    dev_ver_str,
	    ver.hardware_version, ver.firmware_version_major,
	    ver.firmware_version_minor,
	    ((MP_FW_VER_MIN > ((ver.firmware_version_major << 8) | ver.firmware_version_minor)) ? " (too old)" : ""),
	    (int)sizeof(ver.device_code), ver.device_code,
	    (int)sizeof(ver.serial_num), ver.serial_num,
	    ver.device_status);

	/* Protocol version check. */
	switch (ver.device_status) {
	case 1:
	case 2:
		break;
	default:
		ERROR("Protocol version error");
	}

	// Verifying Chip ID (if applicable)
	if (device->chip_id_size &&
	    device->chip_id) {
		minipro_begin_transaction(handle);
		minipro_get_chip_id(handle, &chip_id, &chip_id_size);
		minipro_end_transaction(handle);
		if (chip_id == device->chip_id) {
			printf("Chip ID OK: 0x%02x\n", chip_id);
		} else {
			if (cmdopts.idcheck_continue)
				printf("WARNING: Chip ID mismatch: expected 0x%02x, got 0x%02x\n", device->chip_id, chip_id);
			else
				ERROR_FMT("Invalid Chip ID: expected 0x%02x, got 0x%02x\n(use '-y' to continue anyway at your own risk)\n", device->chip_id, chip_id);
		}
	}

	cmdopts.action(cmdopts.filename, handle, device);

	minipro_close(handle);

	return (0);
}
