#include <sys/types.h>
#include <inttypes.h>
#include <stdlib.h> /* malloc, exit */
#include <stdio.h> /* snprintf, fprintf */
#include <string.h>
#include <errno.h>
#include <libusb.h>

#include "minipro.h"
#include "byte_utils.h"


typedef struct minipro_handle_s {
	libusb_device_handle *usb_handle;
	libusb_context	*ctx;
	device_p 	device;
	uint8_t		icsp;
	uint8_t		msg[(MAX_WRITE_BUFFER_SIZE + MAX_READ_BUFFER_SIZE)];
	int		verboce;
} minipro_t;


#define MP_LOG_ERR(__error, __descr)					\
	if (0 != (__error) && 0 != handle->verboce)			\
		fprintf(stderr, "%s, line: %i, error: %i - %s: %s\n",	\
		    __FUNCTION__, __LINE__, (__error), strerror((__error)), (__descr))

#define MP_LOG_USB_ERR(__error, __descr)				\
	if (0 != (__error) && 0 != handle->verboce)			\
		fprintf(stderr, "%s, line: %i, error: %i = %s - %s: %s\n", \
		    __FUNCTION__, __LINE__, (__error),			\
		    libusb_error_name((__error)),			\
		    libusb_strerror((__error)), (__descr))

#define MP_LOG_ERR_FMT(__error, __fmt, args...)				\
	    if (0 != (__error) && 0 != handle->verboce)			\
		fprintf(stderr, "%s, line: %i, error: %i - %s: " __fmt "\n", \
		    __FUNCTION__, __LINE__, (__error), strerror((__error)), ##args)

#define MP_RET_ON_ERR(__err) {						\
	int ret_error = (__err);					\
	if (0 != ret_error) {						\
		if (0 != handle->verboce)				\
			fprintf(stderr, "%s:%i %s: error = %i\r\n",	\
			    __FILE__, __LINE__, __FUNCTION__, ret_error); \
		return (ret_error);					\
	}								\
}

/* Internal staff. */
static int
msg_transfer(minipro_p handle, uint8_t direction,
    uint8_t *buf, uint32_t buf_size, uint32_t *transferred) {
	int error, bytes_transferred = 0;

	error = libusb_claim_interface(handle->usb_handle, 0);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_claim_interface()");
		goto err_out;
	}
	error = libusb_bulk_transfer(handle->usb_handle, (1 | direction),
	    buf, (int)buf_size, &bytes_transferred, 0);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_bulk_transfer()");
		goto err_out;
	}
	error = libusb_release_interface(handle->usb_handle, 0);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_release_interface()");
		goto err_out;
	}

err_out:
	if (NULL != transferred) {
		(*transferred) = (uint32_t)bytes_transferred;
	}

	return (error);
}

static int
msg_send(minipro_p handle, uint8_t *buf, uint32_t buf_size,
    uint32_t *transferred) {
	int error;
	uint32_t bytes_transferred;

	error = msg_transfer(handle, LIBUSB_ENDPOINT_OUT, buf, buf_size,
	    &bytes_transferred);
	if (NULL != transferred) {
		(*transferred) = bytes_transferred;
	}
	if (0 != error)
		return (error);
	/* Always check sended size! */
	if (bytes_transferred != buf_size) {
		error = EIO;
		MP_LOG_ERR_FMT(error, "expected %d bytes but %d bytes transferred",
		    buf_size, bytes_transferred);
	}
	return (error);
}

static int
msg_recv(minipro_p handle, uint8_t *buf, uint32_t buf_size,
    uint32_t *transferred) {

	return (msg_transfer(handle, LIBUSB_ENDPOINT_IN, buf, buf_size, transferred));
}

static void
msg_init(minipro_p handle, uint8_t cmd, uint32_t msg_size) {

	memset(handle->msg, 0x00, msg_size);
	handle->msg[0] = cmd;
}

static void
msg_dev_set(minipro_p handle) {

	handle->msg[1] = handle->device->protocol_id;
	handle->msg[2] = handle->device->variant;
	//handle->msg[3] = 0x00;
	handle->msg[4] = ((handle->device->data_memory_size >> 8) & 0xFF);

	format_int(&handle->msg[5], handle->device->opts1, 2, MP_LITTLE_ENDIAN);
	handle->msg[8] = handle->msg[6];
	format_int(&handle->msg[6], handle->device->opts2, 2, MP_LITTLE_ENDIAN);
	format_int(&handle->msg[9], handle->device->opts3, 2, MP_LITTLE_ENDIAN);

	handle->msg[11] = handle->icsp;
	format_int(&handle->msg[12], handle->device->code_memory_size, 4, MP_LITTLE_ENDIAN);
}

static void
msg_init_dev(minipro_p handle, uint8_t cmd, uint32_t msg_size) {

	msg_init(handle, cmd, msg_size);
	msg_dev_set(handle);
}

static int
msg_init_dev_send(minipro_p handle, uint8_t cmd, uint32_t msg_size,
    uint32_t *transferred) {

	if (NULL == handle)
		return (EINVAL);
	msg_init_dev(handle, cmd, msg_size);
	return (msg_send(handle, handle->msg, msg_size, transferred));
}


/* API */

int
minipro_open(uint16_t vendor_id, uint16_t product_id,
    device_p dev, uint8_t icsp, int verboce, minipro_p *handle_ret) {
	int error;
	minipro_p handle;

	if (NULL == handle_ret)
		return (EINVAL);
	handle = malloc(sizeof(minipro_t));
	if (NULL == handle)
		return (ENOMEM);
	memset(handle, 0x00, sizeof(minipro_t));
	handle->device = dev;
	handle->icsp = icsp;
	handle->verboce = verboce;

	error = libusb_init(&handle->ctx);
	if (0 != error) {
		MP_LOG_USB_ERR(error, "libusb_init()");
		goto err_out;
	}

	handle->usb_handle = libusb_open_device_with_vid_pid(handle->ctx,
	    vendor_id, product_id);
	if (NULL == handle->usb_handle) {
		error = ENOENT;
		MP_LOG_ERR(error, "Error opening device");
		goto err_out;
	}
	(*handle_ret) = handle;

	return (0);

err_out:
	free(handle);
	return (error);
}

void
minipro_close(minipro_p handle) {

	if (NULL == handle)
		return;

	libusb_close(handle->usb_handle);
	free(handle);
}

device_p
minipro_device_get(minipro_p handle) {

	if (NULL == handle)
		return (NULL);
	return (handle->device);
}

int
minipro_get_version_info(minipro_p handle, minipro_ver_p ver) {
	uint32_t rcvd;

	if (NULL == handle || NULL == ver)
		return (EINVAL);
	MP_RET_ON_ERR(msg_init_dev_send(handle, MP_CMD_GET_VERSION, 5, NULL));
	MP_RET_ON_ERR(msg_recv(handle, handle->msg, sizeof(handle->msg), &rcvd));
	if (sizeof(minipro_ver_t) > rcvd) {
		MP_LOG_ERR_FMT(EMSGSIZE, "expected %"PRIu32" bytes but %"PRIu32" bytes received",
		    (uint32_t)sizeof(minipro_ver_t), rcvd);
		return (EMSGSIZE);
	}
	memcpy(ver, handle->msg, sizeof(minipro_ver_t));

	return (0);
}

int
minipro_begin_transaction(minipro_p handle) {

	return (msg_init_dev_send(handle, MP_CMD_WRITE_CONFIG, 48, NULL));
}

int
minipro_end_transaction(minipro_p handle) {

	return (msg_init_dev_send(handle, MP_CMD_WRITE_INFO, 4, NULL));
}

/* Model-specific ID, e.g. AVR Device ID (not longer than 4 bytes) */
int
minipro_get_chip_id(minipro_p handle, uint32_t *chip_id, uint8_t *chip_id_size) {
	uint32_t rcvd;

	if (NULL == handle || NULL == chip_id)
		return (EINVAL);

	MP_RET_ON_ERR(msg_init_dev_send(handle, MP_CMD_GET_CHIP_ID, 8, NULL));
	MP_RET_ON_ERR(msg_recv(handle, handle->msg, sizeof(handle->msg), &rcvd));
	if (2 > rcvd)
		return (EMSGSIZE);
	rcvd -= 2;
	if (4 < rcvd) {
		rcvd = 4; /* Truncate / limit max size. */
	}
	(*chip_id) = load_int(&handle->msg[2], rcvd, MP_BIG_ENDIAN);
	(*chip_id_size) = (uint8_t)rcvd;

	return (0);
}

int
minipro_protect_off(minipro_p handle) {

	return (msg_init_dev_send(handle, MP_CMD_PROTECT_OFF, 10, NULL));
}

int
minipro_protect_on(minipro_p handle) {

	return (msg_init_dev_send(handle, MP_CMD_PROTECT_ON, 10, NULL));
}

int
minipro_get_status(minipro_p handle, uint16_t *status) {
	uint32_t rcvd;

	if (NULL == handle || NULL == status)
		return (EINVAL);
	MP_RET_ON_ERR(msg_init_dev_send(handle, MP_CMD_REQ_STATUS, 5, NULL));
	MP_RET_ON_ERR(msg_recv(handle, handle->msg, sizeof(handle->msg), &rcvd));
	// rcvd == 32
	if (0 != handle->msg[9])
		return (-1); /* Overcurrency protection. */
	(*status) = load_int(handle->msg, 2, MP_LITTLE_ENDIAN);
	return (0);
}

int
minipro_read_block(minipro_p handle, uint8_t cmd, uint32_t addr,
    uint8_t *buf, uint16_t buf_size) {
	uint32_t rcvd;

	if (NULL == handle)
		return (EINVAL);
	msg_init_dev(handle, cmd, 18);
	format_int(&handle->msg[2], buf_size, 2, MP_LITTLE_ENDIAN);
	format_int(&handle->msg[4], addr, 3, MP_LITTLE_ENDIAN);
	MP_RET_ON_ERR(msg_send(handle, handle->msg, 18, NULL));
	MP_RET_ON_ERR(msg_recv(handle, buf, buf_size, &rcvd));
	if (rcvd != (uint32_t)buf_size)
		return (EMSGSIZE);
	return (0);
}

int
minipro_write_block(minipro_p handle, uint8_t cmd, uint32_t addr,
    const uint8_t *buf, uint16_t buf_size) {

	if (NULL == handle)
		return (EINVAL);
	msg_init_dev(handle, cmd, 7);
	format_int(&handle->msg[2], buf_size, 2, MP_LITTLE_ENDIAN);
	format_int(&handle->msg[4], addr, 3, MP_LITTLE_ENDIAN);
	memcpy(&handle->msg[7], buf, buf_size);

	return (msg_send(handle, handle->msg, (7 + buf_size), NULL));
}

int
minipro_read_fuses(minipro_p handle, uint8_t cmd,
    uint32_t length, uint8_t *buf) {
	uint32_t rcvd;

	if (NULL == handle)
		return (EINVAL);
	msg_init_dev(handle, cmd, 18);
	handle->msg[2] = ((MP_CMD_READ_CFG == cmd && 4 == length) ? 2 : 1);  // note that PICs with 1 config word will show length==2
	handle->msg[5] = 0x10;
	MP_RET_ON_ERR(msg_send(handle, handle->msg, 18, NULL));
	MP_RET_ON_ERR(msg_recv(handle, handle->msg, sizeof(handle->msg), &rcvd));
	// rcvd == (7 + length)
	memcpy(buf, &handle->msg[7], length);

	return (0);
}

int
minipro_write_fuses(minipro_p handle, uint8_t cmd,
    uint32_t length, const uint8_t *buf) {
	uint32_t rcvd;

	if (NULL == handle)
		return (EINVAL);
	// Perform actual writing
	switch ((cmd & 0xf0)) {
	case 0x10:
		msg_init_dev(handle, (cmd + 1), 64);
		handle->msg[2] = ((length == 4) ? 0x02 : 0x01); /* 2 fuse PICs have len = 8 */
		handle->msg[4] = 0xc8;
		handle->msg[5] = 0x0f;
		handle->msg[6] = 0x00;
		memcpy(&handle->msg[7], buf, length);
		MP_RET_ON_ERR(msg_send(handle, handle->msg, 64, NULL)); // XXX 64? 7+len?
		break;
	case 0x40:
		msg_init_dev(handle, (cmd - 1), 10);
		memcpy(&handle->msg[7], buf, length);
		MP_RET_ON_ERR(msg_send(handle, handle->msg, 10, NULL)); // XXX 10? 7+len?
		break;
	}

	// The device waits us to get the status now
	msg_init_dev(handle, cmd, 18);
	handle->msg[2] = ((MP_CMD_READ_CFG == cmd && 4 == length) ? 2 : 1);  // note that PICs with 1 config word will show length==2
	memcpy(&handle->msg[7], buf, length);
	MP_RET_ON_ERR(msg_send(handle, handle->msg, 18, NULL)); // XXX 18? 7+len?
	MP_RET_ON_ERR(msg_recv(handle, handle->msg, sizeof(handle->msg), &rcvd));
	// rcvd == (7 + length)
	if (memcmp(buf, &handle->msg[7], length))
		return (-1);
	return (0);
}

int
minipro_prepare_writing(minipro_p handle) {
	uint32_t rcvd;

	if (NULL == handle)
		return (EINVAL);
	msg_init_dev(handle, MP_CMD_PREPARE_WRITING, 15);
	format_int(&handle->msg[2], 0x03, 2, MP_LITTLE_ENDIAN);
	handle->msg[2] = handle->device->write_unlock;
	MP_RET_ON_ERR(msg_send(handle, handle->msg, 15, NULL));

	return (msg_recv(handle, handle->msg, sizeof(handle->msg), &rcvd)); // rcvd == 10
}
