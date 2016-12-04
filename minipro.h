#ifndef __MINIPRO_H
#define __MINIPRO_H

/* This header is only containing the low-level wrappers against typical requests.
 * Please refer main.c if you're looking for a higher-level logic. */

#include <sys/types.h>
#include <inttypes.h>

#include "database.h"


#define MP_TL866_VID		0x04d8
#define MP_TL866_PID		0xe11c

#define MP_FW_VER_MIN		0x023d


/* Commands. */
#define MP_CMD_GET_VERSION	0x00
typedef struct minipro_version_info_s {
	uint8_t		echo;			/* = MP_CMD_GET_VERSION */
	uint8_t		device_status;		/* Protocol version? */
	uint16_t	report_size;
	uint8_t		firmware_version_minor;
	uint8_t		firmware_version_major;
	uint8_t		device_version;		/* Model. */
	uint8_t		device_code[8];
	uint8_t		serial_num[24];
	uint8_t		hardware_version;
} __attribute__((__packed__)) minipro_ver_t, *minipro_ver_p;
#define 	MP_DEV_VER_TL866_UNKNOWN	0
#define 	MP_DEV_VER_TL866A		1
#define 	MP_DEV_VER_TL866CS		2
static const char *minipro_dev_ver_str[] = {
	"TL866 unknown",
	"TL866A",
	"TL866CS",
	NULL
};

#define MP_CMD_READ_FLASH	0x01
#define MP_CMD_WRITE_BOOTLOADER	0x02
#define MP_CMD_WRITE_CONFIG	0x03
#define MP_CMD_WRITE_INFO	0x04
#define MP_CMD_GET_CHIP_ID	0x05 /* GET_INFO */
typedef struct minipro_dumper_info_s {
	uint8_t		device_code[8];
	uint8_t		serial_num[24];
	uint8_t		bootloader_ver;
	uint8_t		cp_bit;		/* Copy protect byte. */
} __attribute__((__packed__)) minipro_dinfo_t, *minipro_dinfo_p;

#define MP_CMD_READ_CFG		0x12
#define MP_CMD_WRITE_CFG	0x13
#define MP_CMD_WRITE_CODE	0x20 
#define MP_CMD_READ_CODE	0x21 
#define MP_CMD_PREPARE_WRITING	0x22
#define MP_CMD_READ_DATA	0x30 
#define MP_CMD_WRITE_DATA	0x31 
#define MP_CMD_PROTECT_OFF	0x44
#define MP_CMD_PROTECT_ON	0x45
//#define MP_CMD_DEBUG_PACKET	0x7f
#define MP_CMD_GET_PORT_INP	0x80
#define MP_CMD_GET_PORT_LAT	0x81
#define MP_CMD_GET_PORT_TRIS	0x82
#define MP_CMD_SET_PORT_TRIS	0x83
#define MP_CMD_SET_PORT_LAT	0x84
#define MP_CMD_SET_SHIFTREG	0x85
#define MP_CMD_WRITE_COMMAND	0xaa
#define MP_CMD_ERASE_COMMAND	0xcc
#define MP_CMD_REQ_STATUS	0xfe
#define MP_CMD_RESET_COMMAND	0xff


/* ICSP flags. */
#define MP_ICSP_FLAG_ENABLE	0x80
#define MP_ICSP_FLAG_VCC	0x01

#define MAX_READ_BUFFER_SIZE	0x400
#define MAX_WRITE_BUFFER_SIZE	0x210






typedef struct minipro_handle_s *minipro_p;


int	minipro_open(uint16_t vendor_id, uint16_t product_id,
	    device_p dev, uint8_t icsp, int verboce, minipro_p *handle_ret);
void	minipro_close(minipro_p handle);

device_p minipro_device_get(minipro_p handle);

int	minipro_get_version_info(minipro_p handle, minipro_ver_p ver);
int	minipro_begin_transaction(minipro_p handle);
int	minipro_end_transaction(minipro_p handle);
int	minipro_get_chip_id(minipro_p handle, uint32_t *chip_id, uint8_t *chip_id_size);

int	minipro_protect_off(minipro_p handle);
int	minipro_protect_on(minipro_p handle);

int	minipro_get_status(minipro_p handle, uint16_t *status);

int	minipro_read_block(minipro_p handle, uint8_t cmd, uint32_t addr,
	    uint8_t *buf, uint16_t buf_size);
int	minipro_write_block(minipro_p handle, uint8_t cmd, uint32_t addr,
	    const uint8_t *buf, uint16_t buf_size);
	    
int	minipro_read_fuses(minipro_p handle, uint8_t type,
	    uint32_t length, uint8_t *buf);
int	minipro_write_fuses(minipro_p handle, uint8_t type,
	    uint32_t length, const uint8_t *buf);

int	minipro_prepare_writing(minipro_p handle);



#endif
