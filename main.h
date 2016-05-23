#include "minipro.h"

// Parameters for GET_ID
// 8 pin package
device_t device_8_pin[] ={
	{
	.protocol_id = 0x03,
	.variant = 0x02,
	.chip_id_bytes_count = 0x03
	}
};
// 16 pin package
device_t device_16_pin[] ={
	{
	.protocol_id = 0x03,
	.variant = 0x22,
	.chip_id_bytes_count = 0x03
	}
};

void action_read(const char *filename, minipro_handle_t *handle, device_t *device);
void action_write(const char *filename, minipro_handle_t *handle, device_t *device);
