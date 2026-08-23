SKETCH := vera_driver
FQBN ?= arduino:avr:mega
PORT ?= /dev/ttyACM0
BUILD_DIR ?= build

.PHONY: all compile upload list-boards install-avr

all: compile

compile:
	arduino-cli compile --output-dir $(BUILD_DIR) --fqbn $(FQBN) $(SKETCH)

upload:
	arduino-cli upload -p $(PORT) --fqbn $(FQBN) $(SKETCH)

list-boards:
	arduino-cli board list

install-avr:
	arduino-cli core update-index
	arduino-cli core install arduino:avr
