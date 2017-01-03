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
#define MP_DEV_VER_TL866_UNKNOWN	0
#define MP_DEV_VER_TL866A		1
#define MP_DEV_VER_TL866CS		2
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

#define MP_FUSES_BUF_SIZE_MAX	0x1000



typedef struct minipro_handle_s *minipro_p;
typedef void (*minipro_progress_cb)(minipro_p mp, size_t done, size_t total, void *udata);


int	minipro_open(uint16_t vendor_id, uint16_t product_id,
	    int verboce, minipro_p *handle_ret);
void	minipro_close(minipro_p mp);

void	minipro_print_info(minipro_p mp);

int	minipro_chip_set(minipro_p mp, chip_p chip, uint8_t icsp);
chip_p	minipro_chip_get(minipro_p mp);

int	minipro_get_version_info(minipro_p mp, minipro_ver_p ver);
int	minipro_begin_transaction(minipro_p mp);
int	minipro_end_transaction(minipro_p mp);
int	minipro_get_chip_id(minipro_p mp, uint32_t *chip_id, uint8_t *chip_id_size);

int	minipro_prepare_writing(minipro_p mp);

int	minipro_protect_set(minipro_p mp, int val);

int	minipro_get_status(minipro_p mp, uint16_t *status);

int	minipro_read_block(minipro_p mp, uint8_t cmd, uint32_t addr,
	    uint8_t *buf, size_t buf_size);
int	minipro_write_block(minipro_p mp, uint8_t cmd, uint32_t addr,
	    const uint8_t *buf, size_t buf_size);
	    
int	minipro_read_fuses(minipro_p mp, uint8_t type,
	    uint8_t *buf, size_t buf_size);
int	minipro_write_fuses(minipro_p mp, uint8_t type,
	    const uint8_t *buf, size_t buf_size);

int	minipro_read_buf(minipro_p mp, uint8_t cmd,
	    uint32_t addr, uint8_t *buf, size_t buf_size,
	    minipro_progress_cb cb, void *udata);
int	minipro_verify_buf(minipro_p mp, uint8_t cmd,
	    uint32_t addr, const uint8_t *buf, size_t buf_size,
	    size_t *err_offset, uint32_t *buf_val, uint32_t *chip_val,
	    minipro_progress_cb cb, void *udata);
int	minipro_write_buf(minipro_p mp, uint8_t cmd,
	    uint32_t addr, const uint8_t *buf, size_t size,
	    minipro_progress_cb cb, void *udata);

int	minipro_fuses_read(minipro_p mp,
	    uint8_t *buf, size_t buf_size, size_t *buf_used,
	    minipro_progress_cb cb, void *udata);
int	minipro_fuses_verify(minipro_p mp,
	    const uint8_t *buf, size_t buf_size,
	    size_t *err_offset, uint32_t *buf_val, uint32_t *chip_val,
	    minipro_progress_cb cb, void *udata);
int	minipro_fuses_write(minipro_p mp,
	    const uint8_t *buf, size_t buf_size,
	    minipro_progress_cb cb, void *udata);

#define MP_CHIP_PAGE_CODE	0
#define MP_CHIP_PAGE_DATA	1
#define MP_CHIP_PAGE_CONFIG	2
#define MP_CHIP_PAGE__COUNT__	3
static const char *mp_chip_page_str[] = {
	"code",
	"data",
	"config",
	NULL
};

int	minipro_page_read(minipro_p mp, int page,
	    uint32_t address, size_t size, uint8_t **buf, size_t *buf_size,
	    minipro_progress_cb cb, void *udata);
int	minipro_page_verify(minipro_p mp, int page, uint32_t address,
	    const uint8_t *buf, size_t buf_size,
	    size_t *err_offset, uint32_t *buf_val, uint32_t *chip_val,
	    minipro_progress_cb cb, void *udata);
int	minipro_page_write(minipro_p mp, uint32_t flags, int page,
	    uint32_t address, const uint8_t *buf, size_t buf_size,
	    minipro_progress_cb cb, void *udata);
#define MP_PAGE_WR_F_NO_ERASE		0x00000001
#define MP_PAGE_WR_F_PRE_NO_UNPROTECT	0x00000002
#define MP_PAGE_WR_F_POST_NO_PROTECT	0x00000004



#endif
