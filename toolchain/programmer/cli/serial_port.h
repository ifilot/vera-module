#ifndef VERA_FLASH_SERIAL_PORT_H
#define VERA_FLASH_SERIAL_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <windows.h>

typedef struct {
    HANDLE handle;
} serial_port_t;

bool serial_port_open(serial_port_t *port, const char *name);
void serial_port_close(serial_port_t *port);
bool serial_port_write(serial_port_t *port, const void *data, size_t size);
bool serial_port_read(serial_port_t *port, void *data, size_t size,
                      uint32_t timeout_ms);

#endif // VERA_FLASH_SERIAL_PORT_H
