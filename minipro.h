#ifndef __MINIPRO_H
#define __MINIPRO_H

/* This header is only containing the low-level wrappers against typical requests.
 * Please refer main.c if you're looking for a higher-level logic. */

#include <libusb.h>

#include "version.h"

#define MP_TL866A 1
#define MP_TL866CS 2

#define MP_FIRMWARE_VERSION 0x023d

#define MP_REQUEST_STATUS1_MSG1 0x03
#define MP_REQUEST_STATUS1_MSG2 0xfe

#define MP_GET_SYSTEM_INFO 0x00
#define MP_GET_CHIP_ID 0x05
#define MP_READ_CODE 0x21 
#define MP_READ_DATA 0x30 
#define MP_WRITE_CODE 0x20 
#define MP_WRITE_DATA 0x31 
#define MP_PREPARE_WRITING 0x22
#define MP_READ_CFG 0x12
#define MP_WRITE_CFG 0x13
#define MP_PROTECT_OFF 0x44
#define MP_PROTECT_ON 0x45

#define MP_ICSP_ENABLE 0x80
#define MP_ICSP_VCC 0x01

#define MAX_READ_BUFFER_SIZE 0x400
#define MAX_WRITE_BUFFER_SIZE 0x210

#include "database.h"

typedef struct minipro_system_info {
	unsigned char protocol;
	unsigned char model;
	char *model_str;
	unsigned int firmware;
	char firmware_str[16];
} minipro_system_info_t;

typedef struct minipro_handle {
	libusb_device_handle *usb_handle;
	libusb_context *ctx;
	device_t *device;
	int icsp;
} minipro_handle_t;

minipro_handle_t *minipro_open(device_t *device);
void minipro_close(minipro_handle_t *handle);
void minipro_begin_transaction(minipro_handle_t *handle);
void minipro_end_transaction(minipro_handle_t *handle);
void minipro_protect_off(minipro_handle_t *handle);
void minipro_protect_on(minipro_handle_t *handle);
int minipro_get_status(minipro_handle_t *handle);
void minipro_read_block(minipro_handle_t *handle, unsigned int type, unsigned int addr, unsigned char *buf, unsigned int len);
void minipro_write_block(minipro_handle_t *handle, unsigned int type, unsigned int addr, unsigned char *buf, unsigned int len);
int minipro_get_chip_id(minipro_handle_t *handle);
void minipro_read_fuses(minipro_handle_t *handle, unsigned int type, unsigned int length, unsigned char *buf);
void minipro_write_fuses(minipro_handle_t *handle, unsigned int type, unsigned int length, unsigned char *buf);
void minipro_prepare_writing(minipro_handle_t *handle);
void minipro_get_system_info(minipro_handle_t *handle, minipro_system_info_t *out);

#endif
