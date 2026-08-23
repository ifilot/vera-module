#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "programmer.h"
#include "vera_version.h"

static void usage(const char *program) {
    printf("Usage: %s <COM port> <firmware.bin>\n", program);
    printf("       %s --self-test\n", program);
    printf("       %s --version\n", program);
}

static bool read_file(const char *path, uint8_t **data, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        perror(path);
        fclose(file);
        return false;
    }
    long length = ftell(file);
    if (length <= 0 || (unsigned long)length > PROGRAMMER_MAX_IMAGE_SIZE) {
        fprintf(stderr, "%s: invalid image size %ld (maximum %u bytes).\n",
                path, length, PROGRAMMER_MAX_IMAGE_SIZE);
        fclose(file);
        return false;
    }
    rewind(file);

    *data = malloc((size_t)length);
    if (!*data) {
        fprintf(stderr, "Could not allocate %ld bytes.\n", length);
        fclose(file);
        return false;
    }
    *size = (size_t)length;
    if (fread(*data, 1, *size, file) != *size) {
        perror(path);
        free(*data);
        *data = NULL;
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static int self_test(void) {
    static const uint8_t test_data[] = "123456789";
    uint16_t crc = programmer_crc16_xmodem(test_data, sizeof(test_data) - 1);
    if (crc != 0x31C3u) {
        fprintf(stderr, "CRC self-test failed: expected 31C3, got %04X.\n", crc);
        return EXIT_FAILURE;
    }
    puts("PASS: CRC-16/XMODEM self-test");
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("vera-flash %s\n", VERA_VERSION_TAG);
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) {
        return self_test();
    }
    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint8_t *image = NULL;
    size_t image_size = 0;
    if (!read_file(argv[2], &image, &image_size)) {
        return EXIT_FAILURE;
    }

    printf("Image: %s (%zu bytes)\n", argv[2], image_size);
    serial_port_t port = {.handle = INVALID_HANDLE_VALUE};
    if (!serial_port_open(&port, argv[1])) {
        free(image);
        return EXIT_FAILURE;
    }

    bool success = programmer_flash_image(&port, image, image_size);
    serial_port_close(&port);
    free(image);

    if (!success) {
        fputs("Flashing failed.\n", stderr);
        return EXIT_FAILURE;
    }
    puts("PASS: image flashed, read back, and FPGA booted.");
    return EXIT_SUCCESS;
}
