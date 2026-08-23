# VERA Arduino Mega Driver

This folder contains an Arduino Mega sketch for smoke-testing a CX16 VERA board
over an 8-bit parallel bus. After reset it performs a VRAM readback check, then
runs an autonomous diagnostics loop. No keyboard is required.

## Compilation

The simplest path is Arduino IDE or Arduino CLI on Windows. To compile and
upload in one step:

```powershell
arduino-cli core update-index
arduino-cli core install arduino:avr
arduino-cli board list
arduino-cli compile --upload -p COM22 --fqbn arduino:avr:mega src\vera_driver
```

Replace `COM22` with the port shown by Device Manager or `arduino-cli board
list`.

If building in Linux or WSL and flashing from Windows, compile with
`--output-dir src/build`, then flash the generated
`src/build/vera_driver.ino.hex` from Windows with Arduino CLI, Arduino IDE, or
`avrdude` from the Arduino AVR tools package.
