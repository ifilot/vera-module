#include "commands.h"

#include <pico/stdlib.h>
#include <tusb.h>

#include "programmer_config.h"

static bool hex_nibble(char character, uint8_t *value) {
    if (character >= '0' && character <= '9') {
        *value = (uint8_t)(character - '0');
        return true;
    }
    if (character >= 'A' && character <= 'F') {
        *value = (uint8_t)(character - 'A' + 10);
        return true;
    }
    return false;
}

bool command_get_uint8(const char *instruction, uint8_t offset, uint8_t *value) {
    uint8_t high;
    uint8_t low;
    if (!hex_nibble(instruction[offset], &high) ||
        !hex_nibble(instruction[offset + 1], &low)) {
        return false;
    }
    *value = (uint8_t)((high << 4) | low);
    return true;
}

bool command_get_uint16(const char *instruction, uint8_t offset, uint16_t *value) {
    uint16_t result = 0;
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t nibble;
        if (!hex_nibble(instruction[offset + i], &nibble)) {
            return false;
        }
        result = (uint16_t)((result << 4) | nibble);
    }
    *value = result;
    return true;
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

    gpio_put(PIN_LED_BOOT, cdone ? LED_ON : LED_OFF);
}

void fpga_reset_hold(void) {
    gpio_put(PIN_CRESET, 0);
    gpio_put(PIN_LED_BOOT, LED_OFF);

    if (tud_cdc_connected()) {
        tud_cdc_write_char(0);
        tud_cdc_write_flush();
        tud_task();
    }
}
