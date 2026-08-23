#ifndef VERA_FLASH_PROGRAMMER_H
#define VERA_FLASH_PROGRAMMER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "serial_port.h"

#define PROGRAMMER_SECTOR_SIZE 4096u
#define PROGRAMMER_PAGE_SIZE 256u
#define PROGRAMMER_BLOCK_SIZE 65536u
#define PROGRAMMER_MAX_IMAGE_SIZE (256u * PROGRAMMER_SECTOR_SIZE)

uint16_t programmer_crc16_xmodem(const uint8_t *data, size_t size);
bool programmer_flash_image(serial_port_t *port, const uint8_t *image,
                            size_t image_size);

#endif // VERA_FLASH_PROGRAMMER_H
