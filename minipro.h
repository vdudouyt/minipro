#ifndef __MINIPRO_H
#define __MINIPRO_H

/* This header is only containing the low-level wrappers against typical requests.
 * Please refer main.c if you're looking for a higher-level logic. */

#include <sys/types.h>
#include <inttypes.h>

#include "database.h"
#include "version.h"


#define MP_TL866_UNKNOWN	0
#define MP_TL866A		1
#define MP_TL866CS		2
static const char *minipro_model_str[] = {
	"TL866 - unknown device",
	"TL866A",
	"TL866CS",
	NULL
};


#define MP_FIRMWARE_VERSION	0x023d

#define MP_REQUEST_STATUS1_MSG1	0x03
#define MP_REQUEST_STATUS1_MSG2	0xfe

#define MP_GET_SYSTEM_INFO	0x00
#define MP_GET_CHIP_ID		0x05
#define MP_READ_CODE		0x21 
#define MP_READ_DATA		0x30 
#define MP_WRITE_CODE		0x20 
#define MP_WRITE_DATA		0x31 
#define MP_PREPARE_WRITING	0x22
#define MP_READ_CFG		0x12
#define MP_WRITE_CFG		0x13
#define MP_PROTECT_OFF		0x44
#define MP_PROTECT_ON		0x45

#define MP_ICSP_ENABLE		0x80
#define MP_ICSP_VCC		0x01

#define MAX_READ_BUFFER_SIZE	0x400
#define MAX_WRITE_BUFFER_SIZE	0x210


typedef struct minipro_system_info_s {
	uint8_t		protocol;
	uint8_t		model;
	const char	*model_str;
	uint16_t	firmware;
	char		firmware_str[16];
} minipro_system_info_t;

typedef struct minipro_handle_s *minipro_p;


minipro_p minipro_open(device_p device, uint8_t icsp);
void	minipro_close(minipro_p handle);

device_p minipro_device_get(minipro_p handle);

void	minipro_begin_transaction(minipro_p handle);
void	minipro_end_transaction(minipro_p handle);
void	minipro_protect_off(minipro_p handle);
void	minipro_protect_on(minipro_p handle);
uint16_t minipro_get_status(minipro_p handle);
void	minipro_read_block(minipro_p handle, uint32_t type, uint32_t addr,
	    uint8_t *buf, uint16_t len);
void	minipro_write_block(minipro_p handle, uint32_t type, uint32_t addr,
	    const uint8_t *buf, uint16_t len);
uint32_t minipro_get_chip_id(minipro_p handle);
void	minipro_read_fuses(minipro_p handle, uint32_t type,
	    uint32_t length, uint8_t *buf);
void	minipro_write_fuses(minipro_p handle, uint32_t type,
	    uint32_t length, const uint8_t *buf);
void	minipro_prepare_writing(minipro_p handle);
void	minipro_get_system_info(minipro_p handle, minipro_system_info_t *out);


#endif
