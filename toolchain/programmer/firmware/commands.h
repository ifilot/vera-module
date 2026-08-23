#ifndef VERA_PROGRAMMER_COMMANDS_H
#define VERA_PROGRAMMER_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>

uint8_t command_get_uint8(const char *instruction, uint8_t offset);
uint16_t command_get_uint16(const char *instruction, uint8_t offset);
void command_echo(const char *instruction, uint8_t size);
bool command_matches(const char *command, const char *reference,
                     uint8_t offset, uint8_t length);

void fpga_release_and_check_boot(void);
void fpga_reset_hold(void);

#endif // VERA_PROGRAMMER_COMMANDS_H
