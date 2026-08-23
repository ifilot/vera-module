#ifndef VERA_PROGRAMMER_CONFIG_H
#define VERA_PROGRAMMER_CONFIG_H

#define PIN_MISO   16
#define PIN_SEL    17
#define PIN_SCK    18
#define PIN_MOSI   19
#define PIN_CDONE  14
#define PIN_CRESET 15
#define PIN_LED    25

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
#define FLASH_JEDEC_ID              0x9F

#define SPI_PORT spi0

#define BLOCK_SIZE  0x10000
#define SECTOR_SIZE 0x1000
#define PAGE_SIZE   256
#define BUFFER_SIZE 256

// READINFO returns exactly 16 bytes over USB CDC.
#define BOARD_ID "VERA-PROG-v0.1.0"
#define BOARD_ID_LENGTH 16

#endif // VERA_PROGRAMMER_CONFIG_H
