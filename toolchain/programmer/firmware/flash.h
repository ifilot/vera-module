#ifndef VERA_PROGRAMMER_FLASH_H
#define VERA_PROGRAMMER_FLASH_H

#include <stdbool.h>
#include <stdint.h>

void programmer_write_id(void);
void flash_read_jedec_id(void);
void flash_erase_block_64k(uint8_t block);
void flash_reset(void);
void flash_check_block_erased(uint8_t block);
void flash_read_page(uint16_t page);
void flash_write_sector(uint8_t sector);

void flash_cs_select(void);
void flash_cs_deselect(void);
bool flash_write_enable(void);
void flash_read_page_256(uint32_t address, uint8_t *buffer);
bool flash_wait_busy(uint32_t timeout_ms);
uint16_t crc16_xmodem(const uint8_t *data, uint16_t length);

#endif // VERA_PROGRAMMER_FLASH_H
