#ifndef VERA_PROGRAMMER_CONFIG_H
#define VERA_PROGRAMMER_CONFIG_H

#include "vera_version.h"

#define PIN_CDONE     14
#define PIN_CRESET    15
#define PIN_MISO      16
#define PIN_SEL       17
#define PIN_SCK       18
#define PIN_MOSI      19
#define PIN_LED_BOOT  25
#define PIN_LED_READ  26
#define PIN_LED_WRITE 27

// D2 (READ) and D3 (WRITE) are wired active-high on the programmer PCB.
#define LED_ON  1
#define LED_OFF 0

// SPI flash commands
#define FLASH_CMD_WRITE_ENABLE      0x06
#define FLASH_CMD_PAGE_PROGRAM      0x02
#define FLASH_CMD_READ_STATUS       0x05
#define FLASH_CMD_ERASE_SECTOR      0x20
#define FLASH_CMD_READ_DATA         0x03
#define FLASH_CMD_ENABLE_RESET      0x66
#define FLASH_CMD_RESET             0x99
#define FLASH_CMD_ERASE_BLOCK_64K   0xD8
#define FLASH_STATUS_BUSY           0x01
#define FLASH_STATUS_WRITE_ENABLE   0x02
#define FLASH_JEDEC_ID              0x9F

#define SPI_PORT spi0

#define BLOCK_SIZE  0x10000
#define SECTOR_SIZE 0x1000
#define PAGE_SIZE   256
#define BUFFER_SIZE 256

#define FLASH_PAGE_PROGRAM_TIMEOUT_MS 1000u
#define FLASH_BLOCK_ERASE_TIMEOUT_MS  5000u
#define SECTOR_RECEIVE_TIMEOUT_MS     10000u

#define BOARD_ID VERA_PROGRAMMER_ID
#define BOARD_ID_LENGTH (sizeof(BOARD_ID) - 1u)

#endif // VERA_PROGRAMMER_CONFIG_H
