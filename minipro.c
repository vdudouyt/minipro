#include <sys/types.h>
#include <inttypes.h>
#include <stdlib.h> /* malloc, exit */
#include <stdio.h> /* snprintf, fprintf */
#include <string.h>
#include <libusb.h>
#include "minipro.h"
#include "byte_utils.h"
#include "error.h"


typedef struct minipro_handle_s {
	libusb_device_handle *usb_handle;
	libusb_context	*ctx;
	device_p 	device;
	uint8_t		icsp;
} minipro_t;




minipro_p 
minipro_open(device_p dev, uint8_t icsp) {
	int error;
	minipro_p handle;

	handle = malloc(sizeof(minipro_t));
	if (handle == NULL)
		ERROR("Couldn't malloc");
	memset(handle, 0x00, sizeof(minipro_t));

	error = libusb_init(&handle->ctx);
	if (error < 0) {
		free(handle);
		ERROR2("Error initializing libusb: %s", libusb_error_name(error));
	}

	handle->usb_handle = libusb_open_device_with_vid_pid(handle->ctx, 0x04d8, 0xe11c);
	if (handle->usb_handle == NULL) {
		free(handle);
		ERROR("Error opening device");
	}

	handle->device = dev;
	handle->icsp = icsp;

	return (handle);
}

void
minipro_close(minipro_p handle) {

	libusb_close(handle->usb_handle);
	free(handle);
}

device_p
minipro_device_get(minipro_p handle) {

	if (NULL == handle)
		return (NULL);
	return (handle->device);
}

static void
msg_init(minipro_p handle, uint8_t cmd, uint8_t *buf, size_t buf_size) {

	memset(buf, 0x00, buf_size);
	buf[0] = cmd;
	buf[1] = handle->device->protocol_id;
	buf[2] = handle->device->variant;
	//buf[3] = 0x00;
	buf[4] = ((handle->device->data_memory_size >> 8) & 0xFF);

	format_int(&buf[5], handle->device->opts1, 2, MP_LITTLE_ENDIAN);
	//buf[8] = buf[6];
	format_int(&buf[6], handle->device->opts2, 2, MP_LITTLE_ENDIAN);
	format_int(&buf[9], handle->device->opts3, 2, MP_LITTLE_ENDIAN);

	buf[11] = handle->icsp;
	format_int(&buf[12], handle->device->code_memory_size, 4, MP_LITTLE_ENDIAN);
}

static uint32_t
msg_transfer(minipro_p handle, uint8_t *buf, int length, int direction) {
	int bytes_transferred;
	int ret;

	ret = libusb_claim_interface(handle->usb_handle, 0);
	if (ret != 0)
		ERROR2("IO error: claim_interface: %s\n", libusb_error_name(ret));
	ret = libusb_bulk_transfer(handle->usb_handle, (1 | direction), buf, length, &bytes_transferred, 0);
	if (ret != 0)
		ERROR2("IO error: bulk_transfer: %s\n", libusb_error_name(ret));
	ret = libusb_release_interface(handle->usb_handle, 0);
	if (ret != 0)
		ERROR2("IO error: release_interface: %s\n", libusb_error_name(ret));
	if (bytes_transferred != length)
		ERROR2("IO error: expected %d bytes but %d bytes transferred\n", length, bytes_transferred);

	return (bytes_transferred);
}

#ifndef TEST
static uint32_t
msg_send(minipro_p handle, uint8_t *buf, int length) {

	return (msg_transfer(handle, buf, length, LIBUSB_ENDPOINT_OUT));
}

static uint32_t
msg_recv(minipro_p handle, uint8_t *buf, int length) {

	return (msg_transfer(handle, buf, length, LIBUSB_ENDPOINT_IN));
}
#endif

void
minipro_begin_transaction(minipro_p handle) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];

	msg_init(handle, MP_REQUEST_STATUS1_MSG1, msg, 48);
	msg_send(handle, msg, 48);
}

void
minipro_end_transaction(minipro_p handle) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];

	msg_init(handle, 0x04, msg, 4);
	msg[3] = 0x00;
	msg_send(handle, msg, 4);
}

void
minipro_protect_off(minipro_p handle) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];

	msg_init(handle, MP_PROTECT_OFF, msg, 10);
	msg_send(handle, msg, 10);
}

void
minipro_protect_on(minipro_p handle) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];

	msg_init(handle, MP_PROTECT_ON, msg, 10);
	msg_send(handle, msg, 10);
}

uint16_t
minipro_get_status(minipro_p handle) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];
	uint8_t buf[32];

	msg_init(handle, MP_REQUEST_STATUS1_MSG2, msg, 5);
	msg_send(handle, msg, 5);
	msg_recv(handle, buf, 32);

	if (buf[9] != 0)
		ERROR("Overcurrency protection");

	return (load_int(buf, 2, MP_LITTLE_ENDIAN));
}

void
minipro_read_block(minipro_p handle, uint32_t type, uint32_t addr,
    uint8_t *buf, uint16_t len) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];

	msg_init(handle, type, msg, 18);
	format_int(&msg[2], len, 2, MP_LITTLE_ENDIAN);
	format_int(&msg[4], addr, 3, MP_LITTLE_ENDIAN);
	msg_send(handle, msg, 18);
	msg_recv(handle, buf, len);
}

void
minipro_write_block(minipro_p handle, uint32_t type, uint32_t addr,
    const uint8_t *buf, uint16_t len) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];

	msg_init(handle, type, msg, 7);
	format_int(&msg[2], len, 2, MP_LITTLE_ENDIAN);
	format_int(&msg[4], addr, 3, MP_LITTLE_ENDIAN);
	memcpy(&msg[7], buf, len);
	msg_send(handle, msg, (7 + len));
}

/* Model-specific ID, e.g. AVR Device ID (not longer than 4 bytes) */
uint32_t
minipro_get_chip_id(minipro_p handle) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];

	msg_init(handle, MP_GET_CHIP_ID, msg, 8);
	msg_send(handle, msg, 8);
	msg_recv(handle, msg, (5 + handle->device->chip_id_bytes_count));

	return (load_int(&msg[2], handle->device->chip_id_bytes_count, MP_BIG_ENDIAN));
}

void
minipro_read_fuses(minipro_p handle, uint32_t type,
    uint32_t length, uint8_t *buf) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];

	msg_init(handle, type, msg, 18);
	msg[2] = ((type == 18 && length == 4) ? 2 : 1);  // note that PICs with 1 config word will show length==2
	msg[5] = 0x10;
	msg_send(handle, msg, 18);
	msg_recv(handle, msg, (7 + length));
	memcpy(buf, &msg[7], length);
}

void
minipro_write_fuses(minipro_p handle, uint32_t type,
    uint32_t length, const uint8_t *buf) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];

	// Perform actual writing
	switch ((type & 0xf0)) {
	case 0x10:
		msg_init(handle, (type + 1), msg, 64);
		msg[2] = ((length == 4) ? 0x02 : 0x01); /* 2 fuse PICs have len = 8 */
		msg[4] = 0xc8;
		msg[5] = 0x0f;
		msg[6] = 0x00;
		memcpy(&msg[7], buf, length);
		msg_send(handle, msg, 64);
		break;
	case 0x40:
		msg_init(handle, (type - 1), msg, 10);
		memcpy(&msg[7], buf, length);
		msg_send(handle, msg, 10);
		break;
	}

	// The device waits us to get the status now
	msg_init(handle, type, msg, 18);
	msg[2] = ((type == 18 && length == 4) ? 2 : 1);  // note that PICs with 1 config word will show length==2
	memcpy(&msg[7], buf, length);
	msg_send(handle, msg, 18);
	msg_recv(handle, msg, (7 + length));

	if (memcmp(buf, &msg[7], length)) {
		ERROR("Failed while writing config bytes");
	}
}

void
minipro_get_system_info(minipro_p handle, minipro_system_info_t *out) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];
	uint8_t buf[40];

	memset(msg, 0x00, 5);
	msg[0] = MP_GET_SYSTEM_INFO;
	msg_send(handle, msg, 5);
	msg_recv(handle, buf, 40);

	// Protocol version
	out->protocol = buf[1];
	switch (out->protocol) {
	case 1:
	case 2:
		break;
	default:
		ERROR("Protocol version error");
	}

	// Model
	out->model = buf[6];
	switch (out->model) {
	case MP_TL866A:
	case MP_TL866CS:
		out->model_str = minipro_model_str[out->model];
		break;
	default:
		out->model_str = minipro_model_str[MP_TL866_UNKNOWN];
	}

	// Firmware
	out->firmware = load_int(&buf[4], 2, MP_LITTLE_ENDIAN);
	if (out->firmware < MP_FIRMWARE_VERSION) {
		fprintf(stderr, "Warning: firmware is too old.\n");
	}
	snprintf(out->firmware_str, sizeof(out->firmware_str),
	    "%02d.%d.%d",
	    buf[39], buf[5], buf[4]);
}

void
minipro_prepare_writing(minipro_p handle) {
	uint8_t msg[MAX_WRITE_BUFFER_SIZE];
	uint8_t buf[10];

	msg_init(handle, MP_PREPARE_WRITING, msg, 15);
	format_int(&msg[2], 0x03, 2, MP_LITTLE_ENDIAN);
	msg[2] = handle->device->write_unlock;
	msg_send(handle, msg, 15);
	msg_recv(handle, buf, 10);
}
