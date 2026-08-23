#include <stdbool.h>
#include <stdint.h>

#include <hardware/spi.h>
#include <pico/stdlib.h>
#include <tusb.h>

#include "commands.h"
#include "flash.h"
#include "programmer_config.h"

static char instruction[8];
static uint8_t instruction_position;

static void parse_instruction(void);
static void programmer_init(void);

int main(void) {
    programmer_init();

    while (true) {
        if (tud_cdc_connected() && tud_cdc_available()) {
            char character = tud_cdc_read_char();

            if ((character >= '0' && character <= '9') ||
                (character >= 'A' && character <= 'Z')) {
                instruction[instruction_position++] = character;
            }

            if (instruction_position == sizeof(instruction)) {
                parse_instruction();
                instruction_position = 0;
            }
        }

        tud_task();
    }
}

static void parse_instruction(void) {
    command_echo(instruction, sizeof(instruction));

    if (command_matches(instruction, "READINFO", 0, 8)) {
        programmer_write_id();
    } else if (command_matches(instruction, "DEVIDSST", 0, 8)) {
        flash_read_jedec_id();
    } else if (command_matches(instruction, "ERASBK", 0, 6)) {
        flash_erase_block_64k(command_get_uint8(instruction, 6));
    } else if (command_matches(instruction, "RESETCHP", 0, 8)) {
        flash_reset();
    } else if (command_matches(instruction, "CHCKBK", 0, 6)) {
        flash_check_block_erased(command_get_uint8(instruction, 6));
    } else if (command_matches(instruction, "RDPG", 0, 4)) {
        flash_read_page(command_get_uint16(instruction, 4));
    } else if (command_matches(instruction, "WRSECT", 0, 6)) {
        flash_write_sector(command_get_uint8(instruction, 6));
    } else if (command_matches(instruction, "BOOTFPGA", 0, 8)) {
        fpga_release_and_check_boot();
    } else if (command_matches(instruction, "HOLDFPGA", 0, 8)) {
        fpga_reset_hold();
    }
}

static void programmer_init(void) {
    stdio_init_all();

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);

    gpio_init(PIN_CDONE);
    gpio_set_dir(PIN_CDONE, GPIO_IN);
    gpio_pull_up(PIN_CDONE);

    gpio_init(PIN_CRESET);
    gpio_set_dir(PIN_CRESET, GPIO_OUT);
    gpio_put(PIN_CRESET, 0);

    gpio_init(PIN_SEL);
    gpio_set_dir(PIN_SEL, GPIO_OUT);
    flash_cs_deselect();

    spi_init(SPI_PORT, 1000 * 1000);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
}
