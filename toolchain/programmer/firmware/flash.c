#include "flash.h"

#include <stddef.h>

#include <hardware/spi.h>
#include <pico/stdlib.h>
#include <tusb.h>

#include "programmer_config.h"

void programmer_write_id(void) {
    static const char board_id[] = BOARD_ID;
    tud_cdc_write(board_id, BOARD_ID_LENGTH);
    tud_cdc_write_flush();
}

void flash_read_jedec_id(void) {
    uint8_t command = FLASH_JEDEC_ID;
    uint8_t response[3] = {0};

    flash_cs_select();
    spi_write_blocking(SPI_PORT, &command, 1);
    spi_read_blocking(SPI_PORT, 0x00, response, 3);
    flash_cs_deselect();

    tud_cdc_write(response, sizeof(response));
    tud_cdc_write_flush();
}

void flash_erase_block_64k(uint8_t block) {
    flash_write_enable();

    uint8_t command[] = {
        FLASH_CMD_ERASE_BLOCK_64K,
        block,
        0x00,
        0x00,
    };

    flash_cs_select();
    spi_write_blocking(SPI_PORT, command, sizeof(command));
    flash_cs_deselect();

    flash_wait_busy();
    tud_cdc_write_char(block);
    tud_cdc_write_flush();
}

void flash_reset(void) {
    uint8_t command;

    flash_cs_select();
    command = FLASH_CMD_ENABLE_RESET;
    spi_write_blocking(SPI_PORT, &command, 1);
    flash_cs_deselect();

    flash_cs_select();
    command = FLASH_CMD_RESET;
    spi_write_blocking(SPI_PORT, &command, 1);
    flash_cs_deselect();

    sleep_ms(1);
}

void flash_check_block_erased(uint8_t block) {
    uint32_t address = (uint32_t)block * BLOCK_SIZE;
    uint8_t buffer[BUFFER_SIZE];

    for (uint32_t offset = 0; offset < BLOCK_SIZE; offset += BUFFER_SIZE) {
        flash_read_page_256(address + offset, buffer);

        for (size_t i = 0; i < BUFFER_SIZE; i++) {
            if (buffer[i] != 0xFF) {
                tud_cdc_write_char(0xFF);
                tud_cdc_write_flush();
                return;
            }
        }
    }

    tud_cdc_write_char(0x00);
    tud_cdc_write_flush();
}

void flash_read_page(uint16_t page) {
    uint32_t address = (uint32_t)page * PAGE_SIZE;
    uint8_t buffer[PAGE_SIZE];
    flash_read_page_256(address, buffer);

    for (size_t i = 0; i < PAGE_SIZE; i += 16) {
        while (tud_cdc_write_available() < 16) {
            tud_task();
        }
        tud_cdc_write(&buffer[i], 16);
        tud_task();
    }

    tud_cdc_write_flush();
    tud_task();
}

void flash_write_sector(uint8_t sector) {
    uint32_t bytes_read = 0;
    uint8_t buffer[SECTOR_SIZE];

    while (bytes_read < SECTOR_SIZE) {
        uint32_t available = tud_cdc_available();
        if (available > 0) {
            uint32_t amount = SECTOR_SIZE - bytes_read;
            if (amount > available) {
                amount = available;
            }
            bytes_read += tud_cdc_read(&buffer[bytes_read], amount);
        }
        tud_task();
    }

    uint32_t base_address = (uint32_t)sector * SECTOR_SIZE;
    for (uint32_t offset = 0; offset < SECTOR_SIZE; offset += PAGE_SIZE) {
        flash_write_enable();

        uint32_t address = base_address + offset;
        uint8_t command[4] = {
            FLASH_CMD_PAGE_PROGRAM,
            (uint8_t)(address >> 16),
            (uint8_t)(address >> 8),
            (uint8_t)address,
        };

        flash_cs_select();
        spi_write_blocking(SPI_PORT, command, sizeof(command));
        spi_write_blocking(SPI_PORT, &buffer[offset], PAGE_SIZE);
        flash_cs_deselect();
        flash_wait_busy();
    }

    uint16_t checksum = crc16_xmodem(buffer, SECTOR_SIZE);
    tud_cdc_write_char((uint8_t)checksum);
    tud_cdc_write_char((uint8_t)(checksum >> 8));
    tud_cdc_write_flush();
    tud_task();
}

void flash_cs_select(void) {
    gpio_put(PIN_SEL, 0);
}

void flash_cs_deselect(void) {
    gpio_put(PIN_SEL, 1);
}

void flash_write_enable(void) {
    uint8_t command = FLASH_CMD_WRITE_ENABLE;
    flash_cs_select();
    spi_write_blocking(SPI_PORT, &command, 1);
    flash_cs_deselect();
}

void flash_read_page_256(uint32_t address, uint8_t *buffer) {
    uint8_t command[4] = {
        FLASH_CMD_READ_DATA,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)address,
    };

    flash_cs_select();
    spi_write_blocking(SPI_PORT, command, sizeof(command));
    spi_read_blocking(SPI_PORT, 0x00, buffer, PAGE_SIZE);
    flash_cs_deselect();
}

void flash_wait_busy(void) {
    uint8_t status;
    absolute_time_t deadline = make_timeout_time_ms(1000);

    do {
        uint8_t command = FLASH_CMD_READ_STATUS;
        flash_cs_select();
        spi_write_blocking(SPI_PORT, &command, 1);
        spi_read_blocking(SPI_PORT, 0x00, &status, 1);
        flash_cs_deselect();
        sleep_ms(1);
    } while ((status & FLASH_STATUS_BUSY) && !time_reached(deadline));
}

uint16_t crc16_xmodem(const uint8_t *data, uint16_t length) {
    uint32_t crc = 0;
    static const uint16_t polynomial = 0x1021;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint32_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc <<= 1;
            if (crc & 0x10000) {
                crc = (crc ^ polynomial) & 0xFFFF;
            }
        }
    }

    return (uint16_t)crc;
}
