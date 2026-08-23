#include "commands.h"

#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>
#include <tusb.h>

#include "programmer_config.h"

uint8_t command_get_uint8(const char *instruction, uint8_t offset) {
    char buffer[3];
    buffer[0] = instruction[offset];
    buffer[1] = instruction[offset + 1];
    buffer[2] = '\0';
    return (uint8_t)strtoul(buffer, NULL, 16);
}

uint16_t command_get_uint16(const char *instruction, uint8_t offset) {
    char buffer[5];
    memcpy(buffer, &instruction[offset], 4);
    buffer[4] = '\0';
    return (uint16_t)strtoul(buffer, NULL, 16);
}

void command_echo(const char *instruction, uint8_t size) {
    tud_cdc_write(instruction, size);
    tud_cdc_write_flush();
}

bool command_matches(const char *command, const char *reference,
                     uint8_t offset, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
        if (command[i + offset] != reference[i]) {
            return false;
        }
    }
    return true;
}

void fpga_release_and_check_boot(void) {
    gpio_put(PIN_CRESET, 1);
    sleep_ms(150);

    bool cdone = gpio_get(PIN_CDONE);
    if (tud_cdc_connected()) {
        tud_cdc_write_char(cdone ? 1 : 0);
        tud_cdc_write_flush();
        tud_task();
    }

    gpio_put(PIN_LED, cdone);
}

void fpga_reset_hold(void) {
    gpio_put(PIN_CRESET, 0);
    gpio_put(PIN_LED, 0);

    if (tud_cdc_connected()) {
        tud_cdc_write_char(0);
        tud_cdc_write_flush();
        tud_task();
    }
}
