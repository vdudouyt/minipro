#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "main.h"
#include "minipro.h"
#include "database.h"
#include "error.h"

struct {
	void (*action) (const char *, minipro_handle_t *handle, device_t *device);
	char *filename;
	device_t *device;
	enum { UNSPECIFIED = 0, CODE, DATA, CONFIG } page;
} cmdopts;

void print_help_and_exit(const char *progname) {
	char usage[] = 
		"Usage: %s [options]\n"
		"options:\n"
		"	-r <filename>	Read memory\n"
		"	-w <filename>	Write memory\n"
		"	-p <device>	Specify device\n"
		"	-c <type>	Specify memory type (optional)\n"
		"			Possible values: code, data, config\n"
		"	-i		Use ISP\n";
	fprintf(stderr, usage, progname);
	exit(-1);
}

void print_devices_and_exit() {
	if(isatty(STDOUT_FILENO)) {
		// stdout is a terminal, opening pager
		char *pager_program = getenv("PAGER");
		if(!pager_program) pager_program = "less";
		FILE *pager = popen(pager_program, "w");
		dup2(fileno(pager), STDOUT_FILENO);
	}

	device_t *device;
	for(device = &(devices[0]); device[0].name; device = &(device[1])) {
		printf("%s\n", device->name);
	}
	exit(-1);
}

void parse_cmdline(int argc, char **argv) {
	char c;
	char *cvalue;
	memset(&cmdopts, 0, sizeof(cmdopts));

	if(argc < 2) {
		print_help_and_exit(argv[0]);
	}

	while((c = getopt(argc, argv, "r:w:p:c:")) != -1) {
		switch(c) {
			case 'p':
				if(!strcmp(optarg, "help"))
					print_devices_and_exit();
				cmdopts.device = get_device_by_name(optarg);
				if(!cmdopts.device)
					ERROR("Unknown device");
				break;
			case 'c':
				if(!strcmp(optarg, "code"))
					cmdopts.page = CODE;
				if(!strcmp(optarg, "data"))
					cmdopts.page = DATA;
				if(!strcmp(optarg, "config"))
					cmdopts.page = CONFIG;
				if(!cmdopts.page)
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
		}
	}
}

int get_file_size(const char *filename) {
	FILE *file = fopen(filename, "r");
	if(!file) {
		PERROR("Couldn't open file");
	}

	fseek(file, 0, SEEK_END);
	int size = ftell(file);
	fseek(file, 0, SEEK_SET);

	fclose(file);
	return(size);
}

int update_status(char *status_msg, char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("\r\e[K%s", status_msg);
	vprintf(fmt, args);
	fflush(stdout);
}

int compare_memory(char *buf1, char *buf2, int size, char *c1, char *c2) {
	int i;
	for(i = 0; i < size; i++) {
		if(buf1[i] != buf2[i]) {
			*c1 = buf1[i];
			*c2 = buf2[i];
			return(i);
		}
	}
	return(-1);
}

/* RAM-centric IO operations */
void read_page_ram(minipro_handle_t *handle, char *buf, unsigned int type, const char *name, int size) {
	char status_msg[24];
	sprintf(status_msg, "Reading %s... ", name);

	device_t *device = handle->device;
	if(size % device->read_buffer_size != 0) {
		ERROR2("Wrong page size: %d bytes", size);
	}

	int i;
	int blocks_count = size / device->read_buffer_size;
	for(i = 0; i < blocks_count; i++) {
		// Updating status line
		if(i % 10) {
			update_status(status_msg, "%2d%%", i * 100 / blocks_count);
		}
		// Translating address to protocol-specific
		int addr = i * device->read_buffer_size;
		if(device->opts4 & 0x2000) {
			addr = addr >> 1;
		}
		minipro_read_block(handle, type, addr, buf + i * device->read_buffer_size);
	}

	update_status(status_msg, "OK\n");
}

void write_page_ram(minipro_handle_t *handle, char *buf, unsigned int type, const char *name, int size) {
	char status_msg[24];
	sprintf(status_msg, "Writing %s... ", name);

	device_t *device = handle->device;
	if(size % device->write_buffer_size != 0) {
		ERROR2("Wrong page size: %d bytes", size);
	}
	
	int i;
	int blocks_count = size / device->write_buffer_size;
	for(i = 0; i < blocks_count; i++) {
		// Updating status line
		if(i % 10) {
			update_status(status_msg, "%2d%%", i * 100 / blocks_count);
		}
		// Translating address to protocol-specific
		int addr = i * device->write_buffer_size;
		if(device->opts4 & 0x2000) {
			addr = addr >> 1;
		}
		minipro_write_block(handle, type, addr, buf + i * device->write_buffer_size);
	}
	update_status(status_msg, "OK\n");
}

/* Wrappers for operating with files */
void read_page_file(minipro_handle_t *handle, const char *filename, unsigned int type, const char *name, int size) {
	FILE *file = fopen(filename, "w");
	if(file == NULL) {
		PERROR("Couldn't open file for writing");
	}

	char *buf = malloc(size);
	if(!buf) {
		ERROR("Can't malloc");
	}

	read_page_ram(handle, buf, type, name, size);
	fwrite(buf, 1, size, file);

	fclose(file);
	free(buf);
}

void write_page_file(minipro_handle_t *handle, const char *filename, unsigned int type, const char *name, int size) {
	FILE *file = fopen(filename, "r");
	if(file == NULL) {
		PERROR("Couldn't open file for reading");
	}

	char *buf = malloc(size);
	if(!buf) {
		ERROR("Can't malloc");
	}

	fread(buf, 1, size, file);
	write_page_ram(handle, buf, type, name, size);

	fclose(file);
	free(buf);
}

void verify_page_file(minipro_handle_t *handle, const char *filename, unsigned int type, const char *name, int size) {
	FILE *file = fopen(filename, "r");
	if(file == NULL) {
		PERROR("Couldn't open file for reading");
	}

	/* Loading file */
	int file_size = get_file_size(filename);
	char *file_data = malloc(file_size);
	fread(file_data, 1, file_size, file);
	fclose(file);

	/* Let the things to be proceeded */
	minipro_end_transaction(handle);
	minipro_begin_transaction(handle);

	/* Downloading data from chip*/
	char *chip_data = malloc(size);
	read_page_ram(handle, chip_data, type, name, size);
	unsigned char c1, c2;
	int idx = compare_memory(file_data, chip_data, file_size, &c1, &c2);

	/* No memory leaks */
	free(file_data);
	free(chip_data);

	if(idx != -1) {
		ERROR2("Verification failed at 0x%02x: 0x%02x != 0x%02x\n", idx, c1, c2);
	} else {
		printf("Verification OK\n");
	}
}

/* Higher-level logic */
void action_read(const char *filename, minipro_handle_t *handle, device_t *device) {
	char *code_filename = (char*) filename;
	char *data_filename = (char*) filename;
	char default_data_filename[] = "eeprom.bin";

	switch(cmdopts.page) {
		case UNSPECIFIED:
			data_filename = default_data_filename;
		case CODE:
			read_page_file(handle, code_filename, MP_READ_CODE, "Code", device->code_memory_size);
			if(cmdopts.page) break;
		case DATA:
			if(device->data_memory_size) {
				read_page_file(handle, data_filename, MP_READ_DATA, "Data", device->data_memory_size);
			}
			break;
	}
}

void action_write(const char *filename, minipro_handle_t *handle, device_t *device) {
	if(get_file_size(filename) > device->code_memory_size) {
		ERROR("File is too large");
	}
	minipro_prepare_writing(handle);
	minipro_end_transaction(handle); // Let prepare_writing() to make an effect
	minipro_begin_transaction(handle); // Prevent device from hanging
	minipro_get_status(handle);

	switch(cmdopts.page) {
		case UNSPECIFIED:
		case CODE:
			write_page_file(handle, filename, MP_WRITE_CODE, "Code", device->code_memory_size);
			verify_page_file(handle, filename, MP_READ_CODE, "Code", device->code_memory_size);
			break;
		case DATA:
			write_page_file(handle, filename, MP_WRITE_DATA, "Data", device->data_memory_size);
			verify_page_file(handle, filename, MP_READ_DATA, "Data", device->data_memory_size);
			break;
	}
}

int main(int argc, char **argv) {
	parse_cmdline(argc, argv);
	if(!cmdopts.filename) {
		print_help_and_exit(argv[0]);
	}
	if(cmdopts.action && !cmdopts.device) {
		USAGE_ERROR("Device required");
	}

	device_t *device = cmdopts.device;
	minipro_handle_t *handle = minipro_open(device);

	// Printing system info
	minipro_system_info_t info;
	minipro_get_system_info(handle, &info);
	printf("Found Minipro %s v%s\n", info.model_str, info.firmware_str);

	minipro_begin_transaction(handle);
	minipro_get_status(handle);

	// Verifying Chip ID (if applicable)
	if(device->chip_id_bytes_count && device->chip_id) {
		unsigned int chip_id = minipro_get_chip_id(handle);
		if (chip_id == device->chip_id) {
			printf("Chip ID OK: 0x%02x\n", chip_id);
		} else {
			ERROR2("Invalid Chip ID: expected 0x%02x, got 0x%02x\n", device->chip_id, chip_id);
		}
	}

	cmdopts.action(cmdopts.filename, handle, device);

	minipro_end_transaction(handle);
	minipro_close(handle);

	return(0);
}
