#ifndef __BYTE_UTILS_H
#define __BYTE_UTILS_H

#define MP_LITTLE_ENDIAN	0
#define MP_BIG_ENDIAN		1

void	format_int(uint8_t *out, uint32_t in, uint8_t length, uint8_t endianess);
int	load_int(uint8_t *buf, uint8_t length, uint8_t endianess);

#endif
