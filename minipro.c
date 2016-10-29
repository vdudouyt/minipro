#include <libusb.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <stdio.h>
#else
#include <malloc.h>
#endif
#include "minipro.h"
#include "byte_utils.h"
#include "error.h"

minipro_handle_t *minipro_open(device_t *device) {
	int ret;
	minipro_handle_t *handle = malloc(sizeof(minipro_handle_t));
	if(handle == NULL) {
		ERROR("Couldn't malloc");
	}

	ret = libusb_init(&(handle->ctx));
	if(ret < 0) {
		free(handle);
		ERROR2("Error initializing libusb: %s", libusb_error_name(ret));
	}

	handle->usb_handle = libusb_open_device_with_vid_pid(handle->ctx, 0x04d8, 0xe11c);
	if(handle->usb_handle == NULL) {
		free(handle);
		ERROR("Error opening device");
	}

	handle->device = device;

	return(handle);
}

void minipro_close(minipro_handle_t *handle) {
	libusb_close(handle->usb_handle);
	free(handle);
}

unsigned char msg[MAX_WRITE_BUFFER_SIZE];

static void msg_init(unsigned char *out_buf, unsigned char cmd, device_t *device, int icsp) {
	out_buf[0] = cmd;
	out_buf[1] = device->protocol_id;
	out_buf[2] = device->variant;
	out_buf[3] = 0x00;
	out_buf[4] = device->data_memory_size >> 8 & 0xFF;

	format_int(&(out_buf[5]), device->opts1, 2, MP_LITTLE_ENDIAN);
	out_buf[8] = out_buf[6];
	format_int(&(out_buf[6]), device->opts2, 2, MP_LITTLE_ENDIAN);
	format_int(&(out_buf[9]), device->opts3, 2, MP_LITTLE_ENDIAN);

	out_buf[11] = icsp;
	format_int(&(out_buf[12]), device->code_memory_size, 4, MP_LITTLE_ENDIAN);
}

static unsigned int msg_transfer(minipro_handle_t *handle, unsigned char *buf, int length, int direction) {
	int bytes_transferred;
	int ret;
	ret = libusb_claim_interface(handle->usb_handle, 0);
	if(ret != 0) ERROR2("IO error: claim_interface: %s\n", libusb_error_name(ret));
	ret = libusb_bulk_transfer(handle->usb_handle, (1 | direction), buf, length, &bytes_transferred, 0);
	if(ret != 0) ERROR2("IO error: bulk_transfer: %s\n", libusb_error_name(ret));
	ret = libusb_release_interface(handle->usb_handle, 0);
	if(ret != 0) ERROR2("IO error: release_interface: %s\n", libusb_error_name(ret));
	if(bytes_transferred != length) ERROR2("IO error: expected %d bytes but %d bytes transferred\n", length, bytes_transferred);
	return bytes_transferred;
}

#ifndef TEST
static unsigned int msg_send(minipro_handle_t *handle, unsigned char *buf, int length) {
	return msg_transfer(handle, buf, length, LIBUSB_ENDPOINT_OUT);
}

static unsigned int msg_recv(minipro_handle_t *handle, unsigned char *buf, int length) {
	return msg_transfer(handle, buf, length, LIBUSB_ENDPOINT_IN);
}
#endif

void minipro_begin_transaction(minipro_handle_t *handle) {
	memset(msg, 0, sizeof(msg));
	msg_init(msg, MP_REQUEST_STATUS1_MSG1, handle->device, handle->icsp);
	msg_send(handle, msg, 48);
}

void minipro_end_transaction(minipro_handle_t *handle) {
	msg_init(msg, 0x04, handle->device, handle->icsp);
	msg[3] = 0x00;
	msg_send(handle, msg, 4);
}

void minipro_protect_off(minipro_handle_t *handle) {
	memset(msg, 0, sizeof(msg));
	msg_init(msg, MP_PROTECT_OFF, handle->device, handle->icsp);
	msg_send(handle, msg, 10);
}

void minipro_protect_on(minipro_handle_t *handle) {
	memset(msg, 0, sizeof(msg));
	msg_init(msg, MP_PROTECT_ON, handle->device, handle->icsp);
	msg_send(handle, msg, 10);
}

int minipro_get_status(minipro_handle_t *handle) {
	unsigned char buf[32];
	msg_init(msg, MP_REQUEST_STATUS1_MSG2, handle->device, handle->icsp);
	msg_send(handle, msg, 5);
	msg_recv(handle, buf, 32);

	if(buf[9] != 0) {
		ERROR("Overcurrency protection");
	}

	return(load_int(buf, 2, MP_LITTLE_ENDIAN));
}

void minipro_read_block(minipro_handle_t *handle, unsigned int type, unsigned int addr, unsigned char *buf, unsigned int len) {
	msg_init(msg, type, handle->device, handle->icsp);
	format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[4]), addr, 3, MP_LITTLE_ENDIAN);
	msg_send(handle, msg, 18);
	msg_recv(handle, buf, len);
}

void minipro_write_block(minipro_handle_t *handle, unsigned int type, unsigned int addr, unsigned char *buf, unsigned int len) {
	msg_init(msg, type, handle->device, handle->icsp);
	format_int(&(msg[2]), len, 2, MP_LITTLE_ENDIAN);
	format_int(&(msg[4]), addr, 3, MP_LITTLE_ENDIAN);
	memcpy(&(msg[7]), buf, len);
	msg_send(handle, msg, 7 + len);
}

/* Model-specific ID, e.g. AVR Device ID (not longer than 4 bytes) */
int minipro_get_chip_id(minipro_handle_t *handle) {
	msg_init(msg, MP_GET_CHIP_ID, handle->device, handle->icsp);
	msg_send(handle, msg, 8);
	msg_recv(handle, msg, 5 + handle->device->chip_id_bytes_count);

	return(load_int(&(msg[2]), handle->device->chip_id_bytes_count, MP_BIG_ENDIAN));
}

void minipro_read_fuses(minipro_handle_t *handle, unsigned int type, unsigned int length, unsigned char *buf) {
	msg_init(msg, type, handle->device, handle->icsp);
	msg[2]=(type==18 && length==4)?2:1;  // note that PICs with 1 config word will show length==2
	msg[5]=0x10;
	msg_send(handle, msg, 18);
	msg_recv(handle, msg, 7 + length );
	memcpy(buf, &(msg[7]), length);
}

void minipro_write_fuses(minipro_handle_t *handle, unsigned int type, unsigned int length, unsigned char *buf) {
	// Perform actual writing
	switch(type & 0xf0) {
		case 0x10:
			msg_init(msg, type + 1, handle->device, handle->icsp);
			msg[2] = (length==4)?0x02:0x01;  // 2 fuse PICs have len=8
			msg[4] = 0xc8;
			msg[5] = 0x0f;
			msg[6] = 0x00;
			memcpy(&(msg[7]), buf, length);

			msg_send(handle, msg, 64);
			break;
		case 0x40:
			msg_init(msg, type - 1, handle->device, handle->icsp);
			memcpy(&(msg[7]), buf, length);

			msg_send(handle, msg, 10);
			break;
	}

	// The device waits us to get the status now
	msg_init(msg, type, handle->device, handle->icsp);
	msg[2]=(type==18 && length==4)?2:1;  // note that PICs with 1 config word will show length==2
	memcpy(&(msg[7]), buf, length);
	
	msg_send(handle, msg, 18);
	msg_recv(handle, msg, 7 + length);

	if(memcmp(buf, &(msg[7]), length)) {
		ERROR("Failed while writing config bytes");
	}
}

void minipro_get_system_info(minipro_handle_t *handle, minipro_system_info_t *out) {
	unsigned char buf[40];
	memset(msg, 0x0, 5);
	msg[0] = MP_GET_SYSTEM_INFO;
	msg_send(handle, msg, 5);
	msg_recv(handle, buf, 40);

	// Protocol version
	switch(out->protocol = buf[1]) {
		case 1:
		case 2:
			break;
		default:
			ERROR("Protocol version error");
	}

	// Model
	switch(out->protocol = buf[6]) {
		case MP_TL866A:
			out->model_str = "TL866A";
			break;
		case MP_TL866CS:
			out->model_str = "TL866CS";
			break;
		default:
			ERROR("Unknown device");
	}

	// Firmware
	out->firmware = load_int(&(buf[4]), 2, MP_LITTLE_ENDIAN);
	if(out->firmware < MP_FIRMWARE_VERSION) {
		fprintf(stderr, "Warning: firmware is too old\n");
	}
	sprintf(out->firmware_str, "%02d.%d.%d", buf[39], buf[5], buf[4]);
}

void minipro_prepare_writing(minipro_handle_t *handle) {
	unsigned char buf[10];
	msg_init(msg, MP_PREPARE_WRITING, handle->device, handle->icsp);
	format_int(&(msg[2]), 0x03, 2, MP_LITTLE_ENDIAN);
	msg[2] = handle->device->write_unlock;
	msg_send(handle, msg, 15);
	msg_recv(handle, buf, 10);
}
