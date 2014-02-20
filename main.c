#include <stdio.h>
#include <stdlib.h>
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
	enum { CODE_MEMORY, DATA_MEMORY, CONFIG } part;
} cmdopts;

void print_help_and_exit(const char *progname) {
	char usage[] = 
		"Usage: %s [options]\n"
		"options:\n"
		"	-r <filename>	Read memory\n"
		"	-w <filename>	Write memory\n"
		"	-p <device>	Specify device\n";
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

	while((c = getopt(argc, argv, "r:w:p:")) != -1) {
		switch(c) {
			case 'p':
				if(!strcmp(optarg, "help"))
					print_devices_and_exit();
				cmdopts.device = get_device_by_name(optarg);
				if(!cmdopts.device)
					ERROR("Unknown device");
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

/* RAM-centric IO operations */
void read_page_ram(minipro_handle_t *handle, char *buf, unsigned int type, const char *name, int size) {
	printf("Reading %s... ", name);
	fflush(stdout);

	device_t *device = handle->device;
	if(size % device->read_buffer_size != 0) {
		ERROR2("Wrong page size: %d bytes", size);
	}

	int i;
	for(i = 0; i < size / device->read_buffer_size; i++) {
		// Translating address to protocol-specific
		int addr = i * device->read_buffer_size;
		if(device->opts4 & 0x2000) {
			addr = addr >> 1;
		}
		minipro_read_block(handle, type, addr, buf + i * device->read_buffer_size);
	}

	printf("OK\n");
}

void write_page_ram(minipro_handle_t *handle, char *buf, unsigned int type, const char *name, int size) {
	printf("Writing %s... ", name);
	fflush(stdout);

	device_t *device = handle->device;
	if(size % device->write_buffer_size != 0) {
		ERROR2("Wrong page size: %d bytes", size);
	}
	
	int i;
	for(i = 0; i < size / device->write_buffer_size; i++) {
		// Translating address to protocol-specific
		int addr = i * device->write_buffer_size;
		if(device->opts4 & 0x2000) {
			addr = addr >> 1;
		}
		minipro_write_block(handle, type, addr, buf + i * device->write_buffer_size);
	}

	printf("OK\n");
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
	free(buf);

	fwrite(buf, 1, size, file);
	fclose(file);
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
	free(buf);

	fclose(file);
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

	/* Downloading data from chip*/
	char *chip_data = malloc(size);
	read_page_ram(handle, chip_data, type, name, size);
	int errors_found = memcmp(file_data, chip_data, file_size);

	/* No memory leaks */
	free(file_data);
	free(chip_data);

	if(errors_found) {
		ERROR("Verification failed");
	} else {
		printf("Verification OK\n");
	}
}

/* Higher-level logic (e.g. write the code and data memory and perform verification) */
void action_read(const char *filename, minipro_handle_t *handle, device_t *device) {
	read_page_file(handle, filename, MP_READ_CODE, "Code", device->code_memory_size);
	if(device->data_memory_size) {
		read_page_file(handle, "eeprom.bin", MP_READ_DATA, "Data", device->data_memory_size);
	}
}

void action_write(const char *filename, minipro_handle_t *handle, device_t *device) {
	if(get_file_size(filename) > device->code_memory_size) {
		ERROR("File is too large");
	}
	minipro_prepare_writing(handle);
	minipro_end_transaction(handle); // Let prepare_writing() to make an effect
	minipro_begin_transaction(handle); // Prevent device from hanging
	char status[32];
	minipro_get_status(handle, status);
	write_page_file(handle, filename, MP_WRITE_CODE, "Code", device->code_memory_size);
	verify_page_file(handle, filename, MP_READ_CODE, "Code", device->code_memory_size);
}

int main(int argc, char **argv) {
	parse_cmdline(argc, argv);
	if(!cmdopts.filename) {
		print_help_and_exit(argv[0]);
	}
	if(cmdopts.action && !cmdopts.device) {
		USAGE_ERROR("Device required");
	}

	char status[32];
	device_t *device = cmdopts.device;
	minipro_handle_t *handle = minipro_open(device);

	// Printing system info
	minipro_system_info_t info;
	minipro_get_system_info(handle, &info);
	printf("Found Minipro %s v%s\n", info.model_str, info.firmware_str);

	minipro_begin_transaction(handle);
	minipro_get_status(handle, status);

	// Verifying Chip ID (if applicable)
	if(device->chip_id_bytes_count) {
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
