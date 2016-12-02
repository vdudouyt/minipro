#include <sys/types.h>
#include <inttypes.h>
#include "byte_utils.h"


void format_int(uint8_t *out, uint32_t in, uint8_t length, uint8_t endianess) {
	int i, idx;

	for (i = 0; i < length; i ++) {
		idx = ((endianess == MP_LITTLE_ENDIAN) ? i : (length - 1 - i));
		out[i] = (in & 0xFF << idx*8) >> idx*8;
	}
}

int load_int(uint8_t *buf, uint8_t length, uint8_t endianess) {
	int i, idx, result = 0;

	for (i = 0; i < length; i++) {
		idx = ((endianess == MP_LITTLE_ENDIAN) ? i : (length - 1 - i));
		result |= (buf[i] << idx*8);
	}

	return (result);
}
