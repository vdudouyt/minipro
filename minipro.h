/*
 * minipro.h - Low level operations declarations and definitions.
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

#ifndef __MINIPRO_H
#define __MINIPRO_H

/*
 * This header only contains low-level wrappers against typical requests.
 * Please refer main.c if you're looking for higher-level logic.
 */

#include <libusb.h>

#include "version.h"

/*
 * These are the known firmware versions along with the versions of the
 * official software from whence they came.
 *
 * Firmware	Official	Release		Firmware
 * Version	Program		Date		Version
 * String	Version		Date		ID
 *
 * 3.2.81	6.70		Mar  7, 2018	0x0251
 * 3.2.80	6.60		May  9, 2017	0x0250
 * 3.2.72	6.50		Dec 25, 2015	0x0248
 * 3.2.69	6.17		Jul 11, 2015	0x0245
 * 3.2.68	6.16		Jun 12, 2015	0x0244
 * 3.2.66	6.13		Jun  9, 2015	0x0242
 * 3.2.63	6.10		Jul 16, 2014	0x023f
 * 3.2.62	6.00		Jan  7, 2014	0x023e
 * 3.2.61	5.91		Mar  9, 2013	0x023d
 * 3.2.60	5.90		Mar  4, 2013	0x023c
 * 3.2.59	5.80		Nov  1, 2012
 *		5.70		Aug 27, 2012
 *		1.00		Jun 18, 2010
 *
 */

#define MP_TL866A 1
#define MP_TL866CS 2

#define MP_FIRMWARE_VERSION 0x0251
#define MP_FIRMWARE_STRING "03.2.81"

#define MP_REQUEST_STATUS1_MSG1 0x03
#define MP_REQUEST_STATUS1_MSG2 0xfe

#define MP_GET_SYSTEM_INFO 0x00
#define MP_END_TRANSACTION 0x04
#define MP_GET_CHIP_ID 0x05
#define MP_READ_CODE 0x21
#define MP_READ_DATA 0x30
#define MP_WRITE_CODE 0x20
#define MP_WRITE_DATA 0x31
#define MP_PREPARE_WRITING 0x22

#define MP_READ_USER 0x10
#define MP_WRITE_USER 0x11

#define MP_READ_CFG 0x12
#define MP_WRITE_CFG 0x13

#define MP_WRITE_LOCK 0x40
#define MP_READ_LOCK 0x41

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
void minipro_read_block(minipro_handle_t *handle, unsigned int type, unsigned int addr, unsigned char *buf, size_t len);
void minipro_write_block(minipro_handle_t *handle, unsigned int type, unsigned int addr, unsigned char *buf, size_t len);
int minipro_get_chip_id(minipro_handle_t *handle);
void minipro_read_fuses(minipro_handle_t *handle, unsigned int type, size_t length, unsigned char *buf);
void minipro_write_fuses(minipro_handle_t *handle, unsigned int type, size_t length, unsigned char *buf);
void minipro_prepare_writing(minipro_handle_t *handle);
void minipro_get_system_info(minipro_handle_t *handle, minipro_system_info_t *out);

#endif
