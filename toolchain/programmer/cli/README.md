# VERA Windows flashing CLI

`vera-flash.exe` writes a VERA FPGA binary through the Raspberry Pi Pico
programmer, reads the programmed pages back, and only boots the FPGA after every
byte has been verified.

Build from an MSYS2 UCRT64 shell:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The MinGW build is statically linked. It does not require MinGW runtime DLLs
such as `libgcc_s`, `libstdc++`, or `libwinpthread`; its only imports are Windows
system DLLs provided by the operating system.

Flash a binary by passing the programmer's Windows COM port:

```console
vera-flash.exe COM3 vera.bin
```

Print the ecosystem version with:

```console
vera-flash.exe --version
```

The CLI rejects devices that do not report the expected VERA programmer ID. It
holds the FPGA in reset, resets and erases the SPI flash, verifies each erased
block, writes 4 KiB sectors with CRC checks, reads all pages back for a bytewise
comparison, and then checks FPGA `CDONE` after boot.

The Pico protocol addresses at most 256 sectors, so images are limited to 1 MiB.
