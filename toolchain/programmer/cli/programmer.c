#include "programmer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMAND_SIZE 8u
#define RESPONSE_TIMEOUT_MS 10000u

static bool send_command(serial_port_t *port, const char command[COMMAND_SIZE]) {
    char echo[COMMAND_SIZE];
    if (!serial_port_write(port, command, COMMAND_SIZE) ||
        !serial_port_read(port, echo, sizeof(echo), RESPONSE_TIMEOUT_MS)) {
        return false;
    }
    if (memcmp(command, echo, COMMAND_SIZE) != 0) {
        fprintf(stderr, "Programmer returned an invalid command echo.\n");
        return false;
    }
    return true;
}

static bool identify_programmer(serial_port_t *port) {
    static const char expected_id[] = "VERA-PROG-v0.1.0";
    char id[sizeof(expected_id)] = {0};

    if (!send_command(port, "READINFO") ||
        !serial_port_read(port, id, sizeof(expected_id) - 1,
                          RESPONSE_TIMEOUT_MS)) {
        return false;
    }
    if (memcmp(id, expected_id, sizeof(expected_id) - 1) != 0) {
        fprintf(stderr, "Unexpected programmer ID: '%.*s'.\n",
                (int)(sizeof(expected_id) - 1), id);
        return false;
    }
    printf("Programmer: %s\n", id);
    return true;
}

static bool hold_fpga(serial_port_t *port) {
    uint8_t response;
    return send_command(port, "HOLDFPGA") &&
           serial_port_read(port, &response, 1, RESPONSE_TIMEOUT_MS) &&
           response == 0;
}

static bool reset_flash(serial_port_t *port) {
    return send_command(port, "RESETCHP");
}

static bool erase_and_check(serial_port_t *port, size_t padded_size) {
    unsigned block_count = (unsigned)((padded_size + PROGRAMMER_BLOCK_SIZE - 1) /
                                      PROGRAMMER_BLOCK_SIZE);
    for (unsigned block = 0; block < block_count; block++) {
        char command[COMMAND_SIZE + 1];
        uint8_t response;

        snprintf(command, sizeof(command), "ERASBK%02X", block);
        printf("\rErasing:  %3u%%", (block * 100u) / block_count);
        fflush(stdout);
        if (!send_command(port, command) ||
            !serial_port_read(port, &response, 1, RESPONSE_TIMEOUT_MS) ||
            response != (uint8_t)block) {
            fprintf(stderr, "\nFailed to erase block %u.\n", block);
            return false;
        }

        snprintf(command, sizeof(command), "CHCKBK%02X", block);
        if (!send_command(port, command) ||
            !serial_port_read(port, &response, 1, RESPONSE_TIMEOUT_MS) ||
            response != 0) {
            fprintf(stderr, "\nErase verification failed for block %u.\n", block);
            return false;
        }
    }
    printf("\rErasing:  100%%\n");
    return true;
}

static bool write_sectors(serial_port_t *port, const uint8_t *image,
                          size_t padded_size) {
    unsigned sector_count = (unsigned)(padded_size / PROGRAMMER_SECTOR_SIZE);
    for (unsigned sector = 0; sector < sector_count; sector++) {
        const uint8_t *data = image + sector * PROGRAMMER_SECTOR_SIZE;
        char command[COMMAND_SIZE + 1];
        uint8_t response[2];
        uint16_t expected_crc = programmer_crc16_xmodem(data, PROGRAMMER_SECTOR_SIZE);

        snprintf(command, sizeof(command), "WRSECT%02X", sector);
        printf("\rWriting:  %3u%%", (sector * 100u) / sector_count);
        fflush(stdout);
        if (!send_command(port, command) ||
            !serial_port_write(port, data, PROGRAMMER_SECTOR_SIZE) ||
            !serial_port_read(port, response, sizeof(response), RESPONSE_TIMEOUT_MS)) {
            fprintf(stderr, "\nFailed to write sector %u.\n", sector);
            return false;
        }
        uint16_t actual_crc = (uint16_t)response[0] | ((uint16_t)response[1] << 8);
        if (actual_crc != expected_crc) {
            fprintf(stderr, "\nCRC mismatch in sector %u (expected %04X, got %04X).\n",
                    sector, expected_crc, actual_crc);
            return false;
        }
    }
    printf("\rWriting:  100%%\n");
    return true;
}

static bool verify_pages(serial_port_t *port, const uint8_t *image,
                         size_t padded_size) {
    unsigned page_count = (unsigned)(padded_size / PROGRAMMER_PAGE_SIZE);
    uint8_t response[PROGRAMMER_PAGE_SIZE];

    for (unsigned page = 0; page < page_count; page++) {
        char command[COMMAND_SIZE + 1];
        snprintf(command, sizeof(command), "RDPG%04X", page);
        printf("\rVerifying: %3u%%", (page * 100u) / page_count);
        fflush(stdout);
        if (!send_command(port, command) ||
            !serial_port_read(port, response, sizeof(response), RESPONSE_TIMEOUT_MS)) {
            fprintf(stderr, "\nFailed to read page %u.\n", page);
            return false;
        }
        const uint8_t *expected = image + page * PROGRAMMER_PAGE_SIZE;
        if (memcmp(response, expected, PROGRAMMER_PAGE_SIZE) != 0) {
            size_t byte = 0;
            while (byte < PROGRAMMER_PAGE_SIZE && response[byte] == expected[byte]) {
                byte++;
            }
            fprintf(stderr,
                    "\nVerification failed at flash address 0x%06zX "
                    "(expected %02X, got %02X).\n",
                    page * PROGRAMMER_PAGE_SIZE + byte,
                    expected[byte], response[byte]);
            return false;
        }
    }
    printf("\rVerifying: 100%%\n");
    return true;
}

static bool boot_fpga(serial_port_t *port) {
    uint8_t cdone;
    if (!send_command(port, "BOOTFPGA") ||
        !serial_port_read(port, &cdone, 1, RESPONSE_TIMEOUT_MS)) {
        return false;
    }
    if (cdone == 0) {
        fprintf(stderr, "The image verified, but FPGA CDONE remained low.\n");
        return false;
    }
    return true;
}

uint16_t programmer_crc16_xmodem(const uint8_t *data, size_t size) {
    uint16_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

bool programmer_flash_image(serial_port_t *port, const uint8_t *image,
                            size_t image_size) {
    if (image_size == 0 || image_size > PROGRAMMER_MAX_IMAGE_SIZE) {
        fprintf(stderr, "Image size must be between 1 byte and %u bytes.\n",
                PROGRAMMER_MAX_IMAGE_SIZE);
        return false;
    }

    size_t padded_size = (image_size + PROGRAMMER_SECTOR_SIZE - 1) /
                         PROGRAMMER_SECTOR_SIZE * PROGRAMMER_SECTOR_SIZE;
    uint8_t *padded_image = malloc(padded_size);
    if (!padded_image) {
        fprintf(stderr, "Could not allocate %zu bytes for the image.\n", padded_size);
        return false;
    }
    memset(padded_image, 0xFF, padded_size);
    memcpy(padded_image, image, image_size);

    bool success = identify_programmer(port) &&
                   hold_fpga(port) &&
                   reset_flash(port) &&
                   erase_and_check(port, padded_size) &&
                   write_sectors(port, padded_image, padded_size) &&
                   verify_pages(port, padded_image, padded_size) &&
                   boot_fpga(port);
    free(padded_image);
    return success;
}
