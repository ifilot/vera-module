#include "serial_port.h"

#include <stdio.h>
#include <string.h>

static void print_windows_error(const char *operation) {
    DWORD error = GetLastError();
    char *message = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, error, 0, (char *)&message, 0, NULL);
    fprintf(stderr, "%s failed (Windows error %lu)%s%s\n", operation,
            (unsigned long)error, message ? ": " : "", message ? message : "");
    if (message) {
        LocalFree(message);
    }
}

bool serial_port_open(serial_port_t *port, const char *name) {
    char device_path[64];
    if (strncmp(name, "\\\\.\\", 4) == 0) {
        snprintf(device_path, sizeof(device_path), "%s", name);
    } else {
        snprintf(device_path, sizeof(device_path), "\\\\.\\%s", name);
    }

    port->handle = CreateFileA(device_path, GENERIC_READ | GENERIC_WRITE, 0,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (port->handle == INVALID_HANDLE_VALUE) {
        print_windows_error("opening serial port");
        return false;
    }

    SetupComm(port->handle, 64 * 1024, 64 * 1024);

    DCB settings = {0};
    settings.DCBlength = sizeof(settings);
    if (!GetCommState(port->handle, &settings)) {
        print_windows_error("reading serial settings");
        serial_port_close(port);
        return false;
    }
    settings.BaudRate = CBR_115200;
    settings.ByteSize = 8;
    settings.Parity = NOPARITY;
    settings.StopBits = ONESTOPBIT;
    settings.fBinary = TRUE;
    settings.fDtrControl = DTR_CONTROL_ENABLE;
    settings.fRtsControl = RTS_CONTROL_DISABLE;
    settings.fOutxCtsFlow = FALSE;
    settings.fOutxDsrFlow = FALSE;
    settings.fOutX = FALSE;
    settings.fInX = FALSE;
    if (!SetCommState(port->handle, &settings)) {
        print_windows_error("configuring serial port");
        serial_port_close(port);
        return false;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutConstant = 20;
    timeouts.WriteTotalTimeoutConstant = 5000;
    if (!SetCommTimeouts(port->handle, &timeouts)) {
        print_windows_error("configuring serial timeouts");
        serial_port_close(port);
        return false;
    }

    PurgeComm(port->handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    Sleep(250);
    return true;
}

void serial_port_close(serial_port_t *port) {
    if (port->handle != NULL && port->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(port->handle);
    }
    port->handle = INVALID_HANDLE_VALUE;
}

bool serial_port_write(serial_port_t *port, const void *data, size_t size) {
    const uint8_t *bytes = data;
    while (size > 0) {
        DWORD chunk = size > MAXDWORD ? MAXDWORD : (DWORD)size;
        DWORD written = 0;
        if (!WriteFile(port->handle, bytes, chunk, &written, NULL)) {
            print_windows_error("writing serial port");
            return false;
        }
        if (written == 0) {
            fprintf(stderr, "Serial write made no progress.\n");
            return false;
        }
        bytes += written;
        size -= written;
    }
    return true;
}

bool serial_port_read(serial_port_t *port, void *data, size_t size,
                      uint32_t timeout_ms) {
    uint8_t *bytes = data;
    size_t received = 0;
    ULONGLONG deadline = GetTickCount64() + timeout_ms;

    while (received < size) {
        DWORD chunk = size - received > MAXDWORD ? MAXDWORD : (DWORD)(size - received);
        DWORD count = 0;
        if (!ReadFile(port->handle, bytes + received, chunk, &count, NULL)) {
            print_windows_error("reading serial port");
            return false;
        }
        received += count;
        if (received == size) {
            return true;
        }
        if (GetTickCount64() >= deadline) {
            fprintf(stderr, "Timed out after receiving %zu of %zu bytes.\n",
                    received, size);
            return false;
        }
    }
    return true;
}
