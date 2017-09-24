#include <sys/types.h>
#include <inttypes.h>
#include <stdlib.h> /* malloc, exit */
#include <stdio.h> /* snprintf, fprintf */
#include <string.h>
#include <errno.h>
#include <libusb.h>

#include "strh2num.h"
#include "minipro.h"


typedef struct minipro_handle_s {
	libusb_device_handle *usb_handle;
	libusb_context	*ctx;
	chip_p		chip;
	uint8_t		icsp;
	uint8_t		msg_hdr[16]; /* Message constan header with chip settings. */
	uint8_t		msg[4096];
	uint8_t		*read_block_buf;
	uint8_t		*write_block_buf;
	int		verboce;
	minipro_ver_t	ver;
} minipro_t;

static const uint8_t mp_chip_page_read_cmd[] = {
	MP_CMD_READ_CODE,
	MP_CMD_READ_DATA,
	0,
	0
};

static const uint8_t mp_chip_page_write_cmd[] = {
	MP_CMD_WRITE_CODE,
	MP_CMD_WRITE_DATA,
	0,
	0
};


#define zalloc(__size)		calloc(1, (__size))

#ifndef min
#	define min(__a, __b)	(((__a) < (__b)) ? (__a) : (__b))
#endif

#ifndef max
#	define max(__a, __b)	(((__a) > (__b)) ? (__a) : (__b))
#endif


#define MP_LOG_ERR(__error, __descr)					\
	if (0 != (__error) && 0 != mp->verboce)				\
		fprintf(stderr, "%s:%i %s: error: %i - %s: %s\n",	\
		    __FILE__, __LINE__, __FUNCTION__,			\
		    (__error), strerror((__error)), (__descr))

#define MP_LOG_USB_ERR(__error, __descr)				\
	if (0 != (__error) && 0 != mp->verboce)				\
		fprintf(stderr, "%s:%i %s: error: %i = %s - %s: %s\n",	\
		    __FILE__, __LINE__, __FUNCTION__, (__error),	\
		    libusb_error_name((__error)),			\
		    libusb_strerror((__error)), (__descr))

#define MP_LOG_ERR_FMT(__error, __fmt, args...)				\
	if (0 != (__error) && 0 != mp->verboce)				\
		fprintf(stderr, "%s:%i %s: error: %i - %s: " __fmt "\n", \
		    __FILE__, __LINE__, __FUNCTION__,			\
		    (__error), strerror((__error)), ##args)

#define MP_RET_ON_ERR(__error) {					\
	int ret_error = (__error);					\
	if (0 != ret_error) {						\
		if (0 != mp->verboce)					\
			fprintf(stderr, "%s:%i %s: error = %i.\r\n",	\
			    __FILE__, __LINE__, __FUNCTION__, ret_error); \
		return (ret_error);					\
	}								\
}

#define MP_RET_ON_ERR_CLEANUP(__error) {				\
	error = (__error);						\
	if (0 != error) {						\
		if (0 != mp->verboce)					\
			fprintf(stderr, "%s:%i %s: error = %i.\r\n",	\
			    __FILE__, __LINE__, __FUNCTION__, error);	\
		goto err_out;						\
	}								\
}

#define MP_PROGRESS_UPDATE(__cb, __mp, __done, __total, __udata)	\
	if (NULL != (__cb))						\
		(__cb)((__mp), (__done), (__total), (__udata))



static inline uint16_t
U8TO16_LITTLE(const uint8_t *p) {

	return (((uint16_t)p[0]) | ((uint16_t)(((uint16_t)p[1]) << 8)));
}

static inline uint32_t
U8TO32n_LITTLE(const uint8_t *p, size_t size) {
	uint32_t ret = 0;

	if (4 < size) {
		size = 4;
	}
	memcpy(&ret, p, size);

	return (ret);
}

static inline uint32_t
U8TO32n_BIG(const uint8_t *p, size_t size) {
	size_t i;
	uint32_t ret = 0;

	if (4 < size) {
		size = 4;
	}
	for (i = 0; i < size; i ++) {
		ret = ((ret << 8) | p[i]);
	}

	return (ret);
}

static inline void
U32TO8_LITTLE(const uint32_t v, uint8_t *p) {
	p[0] = (uint8_t)(v      );
	p[1] = (uint8_t)(v >>  8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static inline void
U24TO8_LITTLE(const uint32_t v, uint8_t *p) {
	p[0] = (uint8_t)(v      );
	p[1] = (uint8_t)(v >>  8);
	p[2] = (uint8_t)(v >> 16);
}

static inline void
U16TO8_LITTLE(const uint16_t v, uint8_t *p) {
	p[0] = (uint8_t)(v      );
	p[1] = (uint8_t)(v >>  8);
}

static inline void
U32TO8n_LITTLE(const uint32_t v, size_t size, uint8_t *p) {

	if (4 < size) {
		size = 4;
	}
	memcpy(p, &v, size);
}


static inline void *
mem_chr_ptr(const void *ptr, const void *buf, const size_t size,
    const uint8_t what_find) {
	size_t offset;

	if (NULL == buf || buf > ptr)
		return (NULL);
	offset = (size_t)(((const uint8_t*)ptr) - ((const uint8_t*)buf));
	if (offset >= size)
		return (NULL);
	return ((void*)memchr(ptr, what_find, (size - offset)));
}

/* Internal staff. */
/* abc=zxy */
static int
buf_get_named_line_val32(const uint8_t *buf, size_t buf_size,
    const char *val_name, size_t val_name_size, uint32_t *value) {
	const uint8_t *ptr, *end;

	if (NULL == buf || 0 == buf_size ||
	    NULL == val_name || 0 == val_name_size || NULL == value)
		return (EINVAL);

	for (;;) {
		ptr = memmem(buf, buf_size, val_name, val_name_size);
		if (NULL == ptr)
			return (-1);
		/* Is start from new line? */
		if (buf < ptr && (
		    0x0a != ptr[-1] && /* LF */
		    0x0d != ptr[-1] && /* CR */
		    '\t' != ptr[-1] && /* TAB */
		    ' ' != ptr[-1]))
			continue; /* Not mutch, skip. */
		ptr += val_name_size;
		/* Is this name not part of another name? */
		if ('\t' == (*ptr) ||
		    ' ' == (*ptr) ||
		    '=' == (*ptr))
			break;
	}
	/* Found, extract value data. */
	ptr ++;
	/* Find end of line. */
	end = mem_chr_ptr(ptr, buf, buf_size, 0x0a/* LF */);
	if (NULL == end) {
		end = (buf + buf_size);
	} else if (ptr < end && 0x0d == end[-1]) { /* CR */
		end --;
	}
	/* Find value start. */
	for (; ptr < end && (' ' == (*ptr) || '=' == (*ptr) || '\t' == (*ptr)); ptr ++)
		;
	if (ptr == end)
		return (EINVAL);
	(*value) = ustrh2u32(ptr, (size_t)(end - ptr));

	return (0);
}

static size_t
memcmp_idx(const uint8_t *buf1, const uint8_t *buf2, const size_t size) {
	register size_t i;

	if (0 != memcmp(buf1, buf2, size)) {
		for (i = 0; size > i; i ++) {
			if (buf1[i] != buf2[i])
				return (i);
		}
		/* Never gets here. */
	}

	return (size);
}


static int
msg_transfer(minipro_p mp, uint8_t direction,
    uint8_t *buf, size_t buf_size, size_t *transferred) {
	int error, bytes_transferred = 0;

	error = libusb_claim_interface(mp->usb_handle, 0);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_claim_interface().");
		goto err_out;
	}
	error = libusb_bulk_transfer(mp->usb_handle, (1 | direction),
	    buf, (int)buf_size, &bytes_transferred, 0);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_bulk_transfer().");
		goto err_out;
	}
	error = libusb_release_interface(mp->usb_handle, 0);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_release_interface().");
		goto err_out;
	}

err_out:
	if (NULL != transferred) {
		(*transferred) = (size_t)bytes_transferred;
	}

	return (error);
}

static int
msg_send(minipro_p mp, uint8_t *buf, size_t buf_size,
    size_t *transferred) {
	int error;
	size_t bytes_transferred;

	error = msg_transfer(mp, LIBUSB_ENDPOINT_OUT, buf, buf_size,
	    &bytes_transferred);
	if (NULL != transferred) {
		(*transferred) = bytes_transferred;
	}
	if (0 != error)
		return (error);
	/* Always check sended size! */
	if (bytes_transferred != buf_size) {
		error = EIO;
		MP_LOG_ERR_FMT(error, "expected %zu bytes but %zu bytes transferred.",
		    buf_size, bytes_transferred);
	}
	return (error);
}

static int
msg_recv(minipro_p mp, uint8_t *buf, size_t buf_size,
    size_t *transferred) {

	return (msg_transfer(mp, LIBUSB_ENDPOINT_IN, buf, buf_size, transferred));
}

static void
msg_init(minipro_p mp, uint8_t cmd, size_t msg_size) {

	memset(mp->msg, 0x00, msg_size);
	mp->msg[0] = cmd;
}

static void
msg_chip_hdr_gen(chip_p chip, uint8_t icsp, uint8_t *buf) {

	buf[0] = 0x00;
	buf[1] = chip->protocol_id;
	buf[2] = chip->variant;
	buf[3] = 0x00;
	buf[4] = ((chip->data_memory_size >> 8) & 0xff);

	U16TO8_LITTLE(chip->opts1, &buf[5]);
	buf[8] = buf[6];
	U16TO8_LITTLE(chip->opts2, &buf[6]);
	U16TO8_LITTLE(chip->opts3, &buf[9]);

	buf[11] = icsp;
	U32TO8_LITTLE(chip->code_memory_size, &buf[12]);
}

static void
msg_chip_hdr_set(minipro_p mp, uint8_t cmd, size_t msg_size) {

	memcpy(mp->msg, mp->msg_hdr, msg_size);
	mp->msg[0] = cmd;
	if (sizeof(mp->msg_hdr) < msg_size) {
		memset((mp->msg + sizeof(mp->msg_hdr)), 0x00,
		    (msg_size - sizeof(mp->msg_hdr)));
	}
}

static int
msh_send_chip_hdr(minipro_p mp, uint8_t cmd, size_t msg_size,
    size_t *transferred) {

	if (NULL == mp || NULL == mp->chip)
		return (EINVAL);
	msg_chip_hdr_set(mp, cmd, msg_size);
	return (msg_send(mp, mp->msg, msg_size, transferred));
}


/* API */

int
minipro_open(uint16_t vendor_id, uint16_t product_id,
    int verboce, minipro_p *handle_ret) {
	int error;
	minipro_p mp;

	if (NULL == handle_ret)
		return (EINVAL);
	mp = zalloc(sizeof(minipro_t));
	if (NULL == mp)
		return (ENOMEM);
	mp->verboce = verboce;

	error = libusb_init(&mp->ctx);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_init().");
		goto err_out;
	}

	mp->usb_handle = libusb_open_device_with_vid_pid(mp->ctx,
	    vendor_id, product_id);
	if (NULL == mp->usb_handle) {
		error = ENOENT;
		MP_LOG_ERR(error, "error opening device.");
		goto err_out;
	}

	error = minipro_get_version_info(mp, &mp->ver);
	if (0 != error) {
		MP_LOG_ERR(error, "minipro_get_version_info().");
		goto err_out;
	}

	/* Protocol version check. */
	switch (mp->ver.device_status) {
	case 1:
	case 2:
		break;
	default:
		error = EPROTONOSUPPORT;
		MP_LOG_ERR_FMT(error, "Protocol version unknown: %i.",
		    mp->ver.device_status);
	}

	(*handle_ret) = mp;

	return (0);

err_out:
	minipro_close(mp);
	return (error);
}

void
minipro_close(minipro_p mp) {

	if (NULL == mp)
		return;

	libusb_close(mp->usb_handle);
	free(mp->read_block_buf);
	free(mp->write_block_buf);
	free(mp);
}

void
minipro_print_info(minipro_p mp) {
	const char *dev_ver_str;
	char str_buf[64];

	/* Model/dev ver preprocess. */
	switch (mp->ver.device_version) {
	case MP_DEV_VER_TL866A:
	case MP_DEV_VER_TL866CS:
		dev_ver_str = minipro_dev_ver_str[mp->ver.device_version];
		break;
	default:
		snprintf(str_buf, sizeof(str_buf), "%s (ver = %"PRIu8")",
		    minipro_dev_ver_str[MP_DEV_VER_TL866_UNKNOWN],
		    mp->ver.device_version);
		dev_ver_str = (const char*)str_buf;
	}
	printf("Minipro: %s, fw: v%02d.%"PRIu8".%"PRIu8"%s, code: %.*s, serial: %.*s, proto: %"PRIu8"\n",
	    dev_ver_str,
	    mp->ver.hardware_version, mp->ver.firmware_version_major,
	    mp->ver.firmware_version_minor,
	    ((MP_FW_VER_MIN > ((mp->ver.firmware_version_major << 8) | mp->ver.firmware_version_minor)) ? " (too old)" : ""),
	    (int)sizeof(mp->ver.device_code), mp->ver.device_code,
	    (int)sizeof(mp->ver.serial_num), mp->ver.serial_num,
	    mp->ver.device_status);
}


int
minipro_chip_set(minipro_p mp, chip_p chip, uint8_t icsp) {

	if (NULL == mp)
		return (EINVAL);
	/* Cleanup old. */
	free(mp->read_block_buf);
	free(mp->write_block_buf);
	mp->read_block_buf = NULL;
	mp->write_block_buf = NULL;
	mp->chip = NULL;
	if (NULL == chip)
		return (0);
	if ((sizeof(mp->msg) - 7) < chip->read_block_size ||
	    (sizeof(mp->msg) - 7) < chip->write_block_size) {
		MP_LOG_ERR_FMT(EINVAL, "Cant handle this chip: increase msg_hdr[%zu] buf to %i and recompile.",
		    (size_t)sizeof(mp->msg),
		    (7 + max(chip->read_block_size, chip->write_block_size)));
		return (EINVAL);
	}
	/* Set new. */
	mp->read_block_buf = malloc((chip->read_block_size + 16));
	mp->write_block_buf = malloc((chip->write_block_size + 16));
	if (NULL == mp->read_block_buf ||
	    NULL == mp->write_block_buf)
		return (ENOMEM);
	mp->chip = chip;
	mp->icsp = icsp;
	/* Generate msg header with chip constans. */
	msg_chip_hdr_gen(chip, icsp, mp->msg_hdr);

	return (0);
}
	
chip_p
minipro_chip_get(minipro_p mp) {

	if (NULL == mp)
		return (NULL);
	return (mp->chip);
}

int
minipro_get_version_info(minipro_p mp, minipro_ver_p ver) {
	size_t rcvd;

	if (NULL == mp || NULL == ver)
		return (EINVAL);
	msg_init(mp, MP_CMD_GET_VERSION, 5);
	MP_RET_ON_ERR(msg_send(mp, mp->msg, 5, NULL));
	MP_RET_ON_ERR(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd));
	if (sizeof(minipro_ver_t) > rcvd) {
		MP_LOG_ERR_FMT(EMSGSIZE, "expected %zu bytes but %zu bytes received.",
		    sizeof(minipro_ver_t), rcvd);
		return (EMSGSIZE);
	}
	memcpy(ver, mp->msg, sizeof(minipro_ver_t));

	return (0);
}

int
minipro_begin_transaction(minipro_p mp) {

	return (msh_send_chip_hdr(mp, MP_CMD_WRITE_CONFIG, 48, NULL));
}

int
minipro_end_transaction(minipro_p mp) {

	return (msh_send_chip_hdr(mp, MP_CMD_WRITE_INFO, 4, NULL));
}

/* Model-specific ID, e.g. AVR Device ID (not longer than 4 bytes) */
int
minipro_get_chip_id(minipro_p mp, uint32_t *chip_id, uint8_t *chip_id_size) {
	int error;
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip || NULL == chip_id)
		return (EINVAL);

	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	MP_RET_ON_ERR_CLEANUP(msh_send_chip_hdr(mp, MP_CMD_GET_CHIP_ID, 8, NULL));
	MP_RET_ON_ERR_CLEANUP(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd));
	if (2 > rcvd) {
		error = EMSGSIZE;
	} else {
		rcvd -= 2;
		if (4 < rcvd) {
			rcvd = 4; /* Truncate / limit max size. */
		}
		(*chip_id) = U8TO32n_BIG(&mp->msg[2], rcvd);
		(*chip_id_size) = (uint8_t)rcvd;
	}

err_out:
	minipro_end_transaction(mp); /* Call after msg processed. */
	return (error);
}

int
minipro_prepare_writing(minipro_p mp) {
	int error;
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip)
		return (EINVAL);
	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	msg_chip_hdr_set(mp, MP_CMD_PREPARE_WRITING, 15);
	mp->msg[2] = mp->chip->write_unlock;
	MP_RET_ON_ERR_CLEANUP(msg_send(mp, mp->msg, 15, NULL));
	MP_RET_ON_ERR_CLEANUP(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd)); /* rcvd == 10 */

err_out:
	minipro_end_transaction(mp); /* Let MP_CMD_PREPARE_WRITING to take an effect. */
	return (error);
}

int
minipro_protect_set(minipro_p mp, int val) {
	int error;

	if (NULL == mp || NULL == mp->chip)
		return (EINVAL);
	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	MP_RET_ON_ERR_CLEANUP(msh_send_chip_hdr(mp,
	    ((0 != val) ? MP_CMD_PROTECT_ON : MP_CMD_PROTECT_OFF), 10, NULL));

err_out:
	minipro_end_transaction(mp);
	return (error);
}

int
minipro_get_status(minipro_p mp, uint16_t *status) {
	int error;
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip || NULL == status)
		return (EINVAL);
	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	MP_RET_ON_ERR_CLEANUP(msh_send_chip_hdr(mp, MP_CMD_REQ_STATUS, 5, NULL));
	MP_RET_ON_ERR_CLEANUP(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd)); /* rcvd == 32 */
	if (9 >= rcvd ||
	    0 != mp->msg[9]) {
		error = -1; /* Overcurrency protection. */
	} else {
		(*status) = U8TO16_LITTLE(mp->msg);
	}

err_out:
	minipro_end_transaction(mp); /* Call after msg processed. */
	return (error);
}


int
minipro_read_block(minipro_p mp, uint8_t cmd, uint32_t addr,
    uint8_t *buf, size_t buf_size) {
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip || (sizeof(mp->msg) - 7) < buf_size)
		return (EINVAL);
	msg_chip_hdr_set(mp, cmd, 18);
	U16TO8_LITTLE((uint16_t)buf_size, &mp->msg[2]);
	/* Translating address to protocol-specific. */
	if (0 != (0x2000 & mp->chip->opts4)) {
		addr = (addr >> 1);
	}
	U24TO8_LITTLE(addr, &mp->msg[4]);
	MP_RET_ON_ERR(msg_send(mp, mp->msg, 18, NULL));
	MP_RET_ON_ERR(msg_recv(mp, buf, buf_size, &rcvd));
	if (rcvd != buf_size)
		return (EMSGSIZE);
	return (0);
}

int
minipro_write_block(minipro_p mp, uint8_t cmd, uint32_t addr,
    const uint8_t *buf, size_t buf_size) {

	if (NULL == mp || NULL == mp->chip || (sizeof(mp->msg) - 7) < buf_size)
		return (EINVAL);
	msg_chip_hdr_set(mp, cmd, 7);
	U16TO8_LITTLE((uint16_t)buf_size, &mp->msg[2]);
	if (0 != (0x2000 & mp->chip->opts4)) {
		addr = (addr >> 1);
	}
	U24TO8_LITTLE(addr, &mp->msg[4]);
	memcpy(&mp->msg[7], buf, buf_size);

	return (msg_send(mp, mp->msg, (7 + buf_size), NULL));
}

int
minipro_read_fuses(minipro_p mp, uint8_t cmd,
    uint8_t *buf, size_t buf_size) {
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip || (sizeof(mp->msg) - 7) < buf_size)
		return (EINVAL);
	msg_chip_hdr_set(mp, cmd, 18);
	/* Note that PICs with 1 config word will show buf_size == 2. (for pic2_fuses) */
	mp->msg[2] = ((MP_CMD_READ_CFG == cmd && 4 == buf_size) ? 0x02 : 0x01);
	mp->msg[5] = 0x10;
	MP_RET_ON_ERR(msg_send(mp, mp->msg, 18, NULL));
	MP_RET_ON_ERR(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd)); /* rcvd == (7 + buf_size) */
	memcpy(buf, &mp->msg[7], buf_size);

	return (0);
}

int
minipro_write_fuses(minipro_p mp, uint8_t cmd,
    const uint8_t *buf, size_t buf_size) {
	size_t rcvd;

	if (NULL == mp || NULL == mp->chip || (sizeof(mp->msg) - 7) < buf_size)
		return (EINVAL);
	/* Perform actual writing. */
	switch ((0xf0 & cmd)) {
	case 0x10:
		msg_chip_hdr_set(mp, (cmd + 1), 64);
		mp->msg[2] = ((4 == buf_size) ? 0x02 : 0x01); /* 2 fuse PICs have len = 8 */
		mp->msg[4] = 0xc8;
		mp->msg[5] = 0x0f;
		mp->msg[6] = 0x00;
		memcpy(&mp->msg[7], buf, buf_size);
		MP_RET_ON_ERR(msg_send(mp, mp->msg, 64, NULL));
		break;
	case 0x40:
		msg_chip_hdr_set(mp, (cmd - 1), 10);
		memcpy(&mp->msg[7], buf, buf_size);
		MP_RET_ON_ERR(msg_send(mp, mp->msg, 10, NULL));
		break;
	}

	/* The device waits us to get the status now. */
	msg_chip_hdr_set(mp, cmd, 18);
	/* Note that PICs with 1 config word will show buf_size == 2. (for pic2_fuses) */
	mp->msg[2] = ((MP_CMD_READ_CFG == cmd && 4 == buf_size) ? 0x02 : 0x01);
	memcpy(&mp->msg[7], buf, buf_size);
	MP_RET_ON_ERR(msg_send(mp, mp->msg, 18, NULL));
	MP_RET_ON_ERR(msg_recv(mp, mp->msg, sizeof(mp->msg), &rcvd));
	if ((7 + buf_size) > rcvd)
		return (EMSGSIZE);
	if (memcmp(buf, &mp->msg[7], buf_size))
		return (-1);
	return (0);
}


int
minipro_read_buf(minipro_p mp, uint8_t cmd,
    uint32_t addr, uint8_t *buf, size_t buf_size,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	uint32_t blk_size, offset;
	size_t to_read = buf_size, tm;

	if (NULL == mp || NULL == mp->chip || NULL == buf || 0 == buf_size)
		return (EINVAL);

	MP_RET_ON_ERR(minipro_begin_transaction(mp));

	blk_size = mp->chip->read_block_size;
	offset = (addr % blk_size); /* Offset from first block start. */
	/* Need to read part from first block / pre alligment. */
	if (0 != offset) {
		addr -= offset; /* Allign addr to block size. */
		MP_PROGRESS_UPDATE(cb, mp, 0, buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    mp->read_block_buf, blk_size));
		tm = min((blk_size - offset), to_read); /* Data size to store in buf. */
		memcpy(buf, (mp->read_block_buf + offset), tm);
		addr += tm;
		buf += tm;
		to_read -= tm;
	}

	/* Read alligned blocks. */
	while (blk_size <= to_read) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_read), buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    buf, blk_size));
		addr += blk_size;
		buf += blk_size;
		to_read -= blk_size;
	}

	/* Last block part / post alligment. */
	if (0 != to_read) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_read), buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    mp->read_block_buf, blk_size));
		memcpy(buf, mp->read_block_buf, to_read);
	}

	MP_PROGRESS_UPDATE(cb, mp, buf_size, buf_size, udata);

err_out:
	minipro_end_transaction(mp);
	return (error);
}

int
minipro_verify_buf(minipro_p mp, uint8_t cmd,
    uint32_t addr, const uint8_t *buf, size_t buf_size,
    size_t *err_offset, uint32_t *buf_val, uint32_t *chip_val,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	uint32_t blk_size, offset;
	size_t to_read = buf_size, tm, diff_off;

	if (NULL == mp || NULL == mp->chip || NULL == buf || 0 == buf_size ||
	    NULL == err_offset || NULL == buf_val || NULL == chip_val)
		return (EINVAL);

	MP_RET_ON_ERR(minipro_begin_transaction(mp));

	blk_size = mp->chip->read_block_size;
	offset = (addr % blk_size); /* Offset from first block start. */
	/* Need to read part from first block / pre alligment. */
	if (0 != offset) {
		addr -= offset; /* Allign addr to block size. */
		MP_PROGRESS_UPDATE(cb, mp, 0, buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    mp->read_block_buf, blk_size));
		tm = min((blk_size - offset), to_read); /* Data size to store in buf. */
		diff_off = memcmp_idx(buf, (mp->read_block_buf + offset), tm);
		if (diff_off != tm) {
			/* movemem() for get (*chip_val) at diff pos offset. */
			memmove(mp->read_block_buf,
			    (mp->read_block_buf + offset), tm);
			goto diff_out;
		}
		addr += tm;
		buf += tm;
		to_read -= tm;
	}

	/* Read alligned blocks. */
	while (blk_size <= to_read) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_read), buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    mp->read_block_buf, blk_size));
		diff_off = memcmp_idx(buf, mp->read_block_buf, blk_size);
		if (diff_off != blk_size)
			goto diff_out;
		addr += blk_size;
		buf += blk_size;
		to_read -= blk_size;
	}

	/* Last block part / post alligment. */
	if (0 != to_read) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_read), buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_read_block(mp, cmd, addr,
		    mp->read_block_buf, blk_size));
		diff_off = memcmp_idx(buf, mp->read_block_buf, to_read);
		if (diff_off != to_read)
			goto diff_out;
	}

	MP_PROGRESS_UPDATE(cb, mp, buf_size, buf_size, udata);
	MP_RET_ON_ERR(minipro_end_transaction(mp));
	
	(*err_offset) = buf_size;

	return (0);

diff_out:
	(*err_offset) = (diff_off + (buf_size - to_read));
	(*chip_val) = mp->read_block_buf[diff_off];
	(*buf_val) = buf[diff_off];
err_out:
	minipro_end_transaction(mp);
	return (error);
}

int
minipro_write_buf(minipro_p mp, uint8_t cmd,
    uint32_t addr, const uint8_t *buf, size_t buf_size,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	uint8_t read_cmd;
	uint32_t blk_size, offset;
	size_t to_write = buf_size, tm;

	if (NULL == mp || NULL == mp->chip || NULL == buf || 0 == buf_size)
		return (EINVAL);

	/* Read block cmd. */
	switch (cmd) {
	case MP_CMD_WRITE_CODE:
		read_cmd = MP_CMD_READ_CODE;
		break;
	case MP_CMD_WRITE_DATA:
		read_cmd = MP_CMD_READ_DATA;
		break;
	default:
		return (EINVAL);
	}

	blk_size = mp->chip->write_block_size;
	offset = (addr % blk_size); /* Offset from first block start. */
	/* Need to write part from first block / pre alligment. */
	if (0 != offset) {
		addr -= offset; /* Allign addr to block size. */
		MP_PROGRESS_UPDATE(cb, mp, 0, buf_size, udata);
		/* Read head of first block. */
		/* read_block_size may not match write_block_size,
		 * use minipro_read_buf() to handle this case. */
		MP_RET_ON_ERR(minipro_read_buf(mp, read_cmd,
		    addr, mp->write_block_buf, offset, NULL, NULL));

		MP_RET_ON_ERR(minipro_begin_transaction(mp));

		/* Update block. */
		tm = min((blk_size - offset), to_write); /* Data size to store in buf. */
		memcpy((mp->write_block_buf + offset), buf, tm);
		/* Write updated block. */
		MP_RET_ON_ERR_CLEANUP(minipro_write_block(mp, cmd, addr,
		    mp->write_block_buf, blk_size));
		addr += tm;
		buf += tm;
		to_write -= tm;
	} else {
		MP_RET_ON_ERR(minipro_begin_transaction(mp));
	}

	/* Write alligned blocks. */
	while (blk_size <= to_write) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_write), buf_size, udata);
		MP_RET_ON_ERR_CLEANUP(minipro_write_block(mp, cmd, addr,
		    buf, blk_size));
		addr += blk_size;
		buf += blk_size;
		to_write -= blk_size;
	}

	/* Last block part / post alligment. */
	if (0 != to_write) {
		MP_PROGRESS_UPDATE(cb, mp, (buf_size - to_write), buf_size, udata);
		/* Read tail of last block. */
		MP_RET_ON_ERR(minipro_end_transaction(mp));
		MP_RET_ON_ERR(minipro_read_buf(mp, read_cmd,
		    (addr + (uint32_t)to_write), (mp->write_block_buf + to_write),
		    (blk_size - to_write), NULL, NULL));
		/* Set data and write. */
		memcpy(mp->write_block_buf, buf, to_write);
		MP_RET_ON_ERR(minipro_begin_transaction(mp));
		MP_RET_ON_ERR_CLEANUP(minipro_write_block(mp, cmd, addr,
		    mp->write_block_buf, blk_size));
	}

	MP_PROGRESS_UPDATE(cb, mp, buf_size, buf_size, udata);

err_out:
	minipro_end_transaction(mp);
	return (error);
}


int
minipro_fuses_read(minipro_p mp,
    uint8_t *buf, size_t buf_size, size_t *buf_used,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	size_t i, j, size, count, used = 0;
	fuse_decl_p fuses;
	uint8_t cmd, tmbuf[512];
	uint32_t val;

	if (NULL == mp || NULL == mp->chip || NULL == mp->chip->fuses ||
	    NULL == buf || 0 == buf_size || NULL == buf_used)
		return (EINVAL);

	fuses = mp->chip->fuses;
	count = fuses[0].size;
	cmd = fuses[1].cmd;
	size = fuses[1].size;

	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	MP_PROGRESS_UPDATE(cb, mp, 0, count, udata);

	for (i = 2; i < count; i ++) {
		size += fuses[i].size;
		MP_PROGRESS_UPDATE(cb, mp, i, count, udata);
		if (fuses[i].cmd < cmd) {
			error = EINVAL;
			MP_LOG_ERR_FMT(error, "fuse_decls are not sorted: item %zu = 0x%02x is less then 0x%02x.",
			    i, fuses[i].cmd, cmd);
			goto err_out;
		}
		/* Skip already processed items. */
		if ((i + 1) < count &&
		    cmd >= fuses[(i + 1)].cmd)
			continue;
		MP_RET_ON_ERR_CLEANUP(minipro_read_fuses(mp, cmd, tmbuf, size));
		/* Unpacking readed tmbuf to fuse_decls with same cmd. */
		for (j = 1; j < count; j ++) {
			if (cmd != fuses[j].cmd)
				continue;
			val = U8TO32n_LITTLE(&tmbuf[fuses[j].offset], fuses[j].size);
			used += (size_t)snprintf((char*)(buf + used), (buf_size - used),
			    "%s = 0x%04x\n",
			    fuses[j].name, val);
		}
		cmd = fuses[(i + 1)].cmd;
		size = 0;
	}

	MP_PROGRESS_UPDATE(cb, mp, count, count, udata);

	(*buf_used) = used;

err_out:
	minipro_end_transaction(mp);
	return (error);
}

int
minipro_fuses_verify(minipro_p mp, const uint8_t *buf, size_t buf_size,
    size_t *err_offset, uint32_t *buf_val, uint32_t *chip_val,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	size_t i, j, size, count;
	fuse_decl_p fuses;
	uint8_t cmd, tmbuf[512];
	uint32_t val, valb;

	if (NULL == mp || NULL == mp->chip || NULL == mp->chip->fuses ||
	    NULL == buf || 0 == buf_size ||
	    NULL == err_offset || NULL == buf_val || NULL == chip_val)
		return (EINVAL);

	fuses = mp->chip->fuses;
	count = fuses[0].size;
	cmd = fuses[1].cmd;
	size = fuses[1].size;

	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	MP_PROGRESS_UPDATE(cb, mp, 0, count, udata);

	for (i = 2; i < count; i ++) {
		size += fuses[i].size;
		MP_PROGRESS_UPDATE(cb, mp, i, count, udata);
		if (fuses[i].cmd < cmd) {
			error = EINVAL;
			MP_LOG_ERR_FMT(error, "fuse_decls are not sorted: item %zu = 0x%02x is less then 0x%02x.",
			    i, fuses[i].cmd, cmd);
			goto err_out;
		}
		/* Skip already processed items. */
		if ((i + 1) < count &&
		    cmd >= fuses[(i + 1)].cmd)
			continue;
		MP_RET_ON_ERR_CLEANUP(minipro_read_fuses(mp, cmd, tmbuf, size));
		/* Unpacking readed tmbuf to fuse_decls with same cmd. */
		for (j = 1; j < count; j ++) {
			if (cmd != fuses[j].cmd)
				continue;
			val = U8TO32n_LITTLE(&tmbuf[fuses[j].offset], fuses[j].size);
			error = buf_get_named_line_val32(buf, buf_size,
			    fuses[j].name, strlen(fuses[j].name), &valb);
			if (0 != error) {
				MP_LOG_ERR_FMT(error, "value for item %zu - \"%s\" not found.",
				    j, fuses[j].name);
				goto err_out;
			}
			if (valb != val) {
				(*err_offset) = j;
				(*chip_val) = val;
				(*buf_val) = valb;
				goto err_out;
			}
		}
		cmd = fuses[(i + 1)].cmd;
		size = 0;
	}

	MP_PROGRESS_UPDATE(cb, mp, count, count, udata);

	(*err_offset) = buf_size;

err_out:
	minipro_end_transaction(mp);
	return (error);
}

int
minipro_fuses_write(minipro_p mp, const uint8_t *buf, size_t buf_size,
    minipro_progress_cb cb, void *udata) {
	int error = 0;
	size_t i, j, size, count;
	fuse_decl_p fuses;
	uint8_t cmd, tmbuf[512];
	uint32_t val;

	if (NULL == mp || NULL == mp->chip || NULL == mp->chip->fuses ||
	    NULL == buf || 0 == buf_size)
		return (EINVAL);

	fuses = mp->chip->fuses;
	count = fuses[0].size;
	cmd = fuses[1].cmd;
	size = fuses[1].size;

	MP_RET_ON_ERR(minipro_begin_transaction(mp));
	MP_PROGRESS_UPDATE(cb, mp, 0, count, udata);

	for (i = 2; i < count; i ++) {
		size += fuses[i].size;
		MP_PROGRESS_UPDATE(cb, mp, i, count, udata);
		if (fuses[i].cmd < cmd) {
			error = EINVAL;
			MP_LOG_ERR_FMT(error, "fuse_decls are not sorted: item %zu (\"%s\") = 0x%02x is less then 0x%02x.",
			    i, fuses[i].name, fuses[i].cmd, cmd);
			goto err_out;
		}
		/* Skip already processed items. */
		if ((i + 1) < count &&
		    cmd >= fuses[(i + 1)].cmd)
			continue;
		/* Packing fuse_decls to tmbuf. */
		memset(tmbuf, 0x00, size);
		for (j = 1; i < count; j ++) {
			if (cmd != fuses[j].cmd)
				continue;
			error = buf_get_named_line_val32(buf, buf_size,
			    fuses[j].name, strlen(fuses[j].name), &val);
			if (0 != error) {
				MP_LOG_ERR_FMT(error, "value for item %zu - \"%s\" not found.",
				    j, fuses[j].name);
				goto err_out;
			}
			U32TO8n_LITTLE(val, fuses[j].size, &tmbuf[fuses[j].offset]);
		}
		MP_RET_ON_ERR_CLEANUP(minipro_write_fuses(mp, cmd, tmbuf, size));
		cmd = fuses[(i + 1)].cmd;
		size = 0;
	}

	MP_PROGRESS_UPDATE(cb, mp, count, count, udata);

err_out:
	minipro_end_transaction(mp);
	return (error);
}


int
minipro_page_read(minipro_p mp, int page, uint32_t address, size_t size,
    uint8_t **buf, size_t *buf_size, minipro_progress_cb cb, void *udata) {
	int error;
	uint8_t *buffer = NULL;
	size_t chip_size;

	if (NULL == mp || NULL == mp->chip || NULL == buf || NULL == buf_size)
		return (EINVAL);

	switch (page) {
	case MP_CHIP_PAGE_CODE:
	case MP_CHIP_PAGE_DATA:
		chip_size = ((MP_CHIP_PAGE_CODE == page) ? mp->chip->code_memory_size : mp->chip->data_memory_size);
		if (0 == chip_size ||
		    ((size_t)address + size) > chip_size)
			return (EINVAL); /* No page or out of range. */
		buffer = malloc(size + sizeof(void*));
		if (NULL == buffer)
			return (ENOMEM);
		error = minipro_read_buf(mp, mp_chip_page_read_cmd[page],
		    address, buffer, size, cb, udata);
		break;
	case MP_CHIP_PAGE_CONFIG:
		if (NULL == mp->chip->fuses)
			return (EINVAL); /* No page. */
		buffer = malloc(MP_FUSES_BUF_SIZE_MAX);
		if (NULL == buffer)
			return (ENOMEM);
		error = minipro_fuses_read(mp, buffer, MP_FUSES_BUF_SIZE_MAX,
		    &size, cb, udata);
		break;
	default:
		return (EINVAL);
	}

	if (0 != error) { /* Fail, cleanup. */
		free(buffer);
		return (error);
	}
	/* OK. */
	(*buf) = buffer;
	(*buf_size) = size;

	return (0);
}

int
minipro_page_verify(minipro_p mp, int page, uint32_t address,
    const uint8_t *buf, size_t buf_size,
    size_t *err_offset, uint32_t *buf_val, uint32_t *chip_val,
    minipro_progress_cb cb, void *udata) {
	int error;
	size_t chip_size;

	if (NULL == mp || NULL == mp->chip || NULL == buf || 0 == buf_size ||
	    NULL == err_offset || NULL == buf_val || NULL == chip_val)
		return (EINVAL);

	switch (page) {
	case MP_CHIP_PAGE_CODE:
	case MP_CHIP_PAGE_DATA:
		chip_size = ((MP_CHIP_PAGE_CODE == page) ? mp->chip->code_memory_size : mp->chip->data_memory_size);
		if (0 == chip_size ||
		    ((size_t)address + buf_size) > chip_size)
			return (EINVAL); /* No page or out of range. */
		error = minipro_verify_buf(mp, mp_chip_page_read_cmd[page],
		    address, buf, buf_size,
		    err_offset, buf_val, chip_val, cb, udata);
		break;
	case MP_CHIP_PAGE_CONFIG:
		if (NULL == mp->chip->fuses)
			return (EINVAL); /* No page. */
		error = minipro_fuses_verify(mp, buf, buf_size,
		    err_offset, buf_val, chip_val, cb, udata);
		break;
	default:
		return (EINVAL);
	}

	return (error);
}

int
minipro_page_write(minipro_p mp, uint32_t flags, int page, uint32_t address,
    const uint8_t *buf, size_t buf_size, minipro_progress_cb cb, void *udata) {
	int error;
	size_t chip_size;
	uint16_t status;

	if (NULL == mp || NULL == mp->chip || NULL == buf || 0 == buf_size)
		return (EINVAL);

	/* Pre checks. */
	switch (page) {
	case MP_CHIP_PAGE_CODE:
	case MP_CHIP_PAGE_DATA:
		chip_size = ((MP_CHIP_PAGE_CODE == page) ? mp->chip->code_memory_size : mp->chip->data_memory_size);
		if (0 == chip_size ||
		    ((size_t)address + buf_size) > chip_size)
			return (EINVAL); /* No page or out of range. */
		break;
	case MP_CHIP_PAGE_CONFIG:
		if (NULL == mp->chip->fuses)
			return (EINVAL); /* No page. */
		break;
	default:
		return (EINVAL);
	}

	/* Erase before writing. */
	if (0 == (MP_PAGE_WR_F_NO_ERASE & flags)) {
		MP_RET_ON_ERR(minipro_prepare_writing(mp));
	}

	/* Status check. */
	error = minipro_get_status(mp, &status);
	if (0 != error) {
		if (-1 == error) {
			MP_LOG_ERR(error, "Overcurrency protection.\n");
		} else {
			MP_LOG_ERR(error, "minipro_get_status() fail.");
		}
		return (error);
	}

	/* Turn off protection before writing. */
	if (0 != (0xc000 & mp->chip->opts4) &&
	    0 == (MP_PAGE_WR_F_PRE_NO_UNPROTECT & flags)) {
		minipro_protect_set(mp, 0);
	}

	/* Write. */
	switch (page) {
	case MP_CHIP_PAGE_CODE:
	case MP_CHIP_PAGE_DATA:
		error = minipro_write_buf(mp, mp_chip_page_write_cmd[page],
		    address, buf, buf_size, cb, udata);
		break;
	case MP_CHIP_PAGE_CONFIG:
		error = minipro_fuses_write(mp, buf, buf_size, cb, udata);
		break;
	}

	/* Turn on protection after writing. */
	if (0 != (0xc000 & mp->chip->opts4) &&
	    0 == (MP_PAGE_WR_F_POST_NO_PROTECT & flags)) {
		minipro_protect_set(mp, 1);
	}

	return (error);
}
