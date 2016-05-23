#ifndef __ROM_CRC32_H__
#define __ROM_CRC32_H__

#include <stdint.h>

uint32_t rom_crc32(uint32_t crc. const void *buf, size_t size);

#endif /* __ROM_CRC32_H__ */