#include <Arduino.h>

/*
  VERA USB bridge for Arduino Mega 2560.

  Graphics policy, image assets, and display setup live in the desktop GUI.
  The bridge solely exposes VERA's parallel register/VRAM bus.

  Request:  A5 opcode length-lo length-hi payload checksum
  Response: 5A status opcode length-lo length-hi payload checksum
  The checksum is XOR of every byte after the marker through the payload.

  01 PING                 -> response payload: protocol version (01)
  02 RESET                -> reset FPGA
  10 WRITE_REGISTER       -> payload: register, value
  11 READ_REGISTER        -> payload: register; response: value
  12 WRITE_VRAM           -> payload: 3-byte address, 1..1024 data bytes
  13 READ_VRAM            -> payload: 3-byte address; response: value
  14 SPI_TRANSFER         -> payload: 1..1024 bytes to clock over VERA SPI;
                                response: received bytes
*/

// Mega 22..29 (PORTA bits 0..7) connect to VERA D0..D7.
#define DATA_DDR DDRA
#define DATA_PORT PORTA
#define DATA_PIN PINA

// Mega 30..37 (PORTC bits 7..0): PC7..PC3=A0..A4; PC2=/CS; PC1=/RD; PC0=/WR.
#define CONTROL_DDR DDRC
#define CONTROL_PORT PORTC

static const uint8_t CONTROL_CS_N = 0x04;
static const uint8_t CONTROL_RD_N = 0x02;
static const uint8_t CONTROL_WR_N = 0x01;
static const uint8_t CONTROL_IDLE = CONTROL_CS_N | CONTROL_RD_N | CONTROL_WR_N;
static const uint8_t PIN_RESET_N = 38;

static const uint8_t REG_ADDR_L = 0x00;
static const uint8_t REG_ADDR_M = 0x01;
static const uint8_t REG_ADDR_H = 0x02;
static const uint8_t REG_DATA0 = 0x03;

static const uint8_t REQUEST_MARKER = 0xA5;
static const uint8_t RESPONSE_MARKER = 0x5A;
static const uint8_t STATUS_OK = 0x00;
static const uint8_t STATUS_BAD_FRAME = 0x01;
static const uint8_t STATUS_BAD_REQUEST = 0x02;
static const uint8_t STATUS_BAD_LENGTH = 0x03;

static const uint8_t OP_PING = 0x01;
static const uint8_t OP_RESET = 0x02;
static const uint8_t OP_WRITE_REGISTER = 0x10;
static const uint8_t OP_READ_REGISTER = 0x11;
static const uint8_t OP_WRITE_VRAM = 0x12;
static const uint8_t OP_READ_VRAM = 0x13;
static const uint8_t OP_SPI_TRANSFER = 0x14;

static const uint16_t MAX_PAYLOAD = 1027;  // 3-byte address + 1024 data bytes.
static uint8_t payload[MAX_PAYLOAD];
static uint8_t opcode;
static uint16_t payload_length;
static uint16_t payload_index;
static uint8_t checksum;

enum ParserState : uint8_t { WAIT_MARKER, READ_OPCODE, READ_LENGTH_LOW,
                             READ_LENGTH_HIGH, READ_PAYLOAD, READ_CHECKSUM };
static ParserState parser_state = WAIT_MARKER;

static uint8_t register_address_bits(uint8_t reg) {
  uint8_t result = 0;
  if (reg & 0x01) result |= 0x80;
  if (reg & 0x02) result |= 0x40;
  if (reg & 0x04) result |= 0x20;
  if (reg & 0x08) result |= 0x10;
  if (reg & 0x10) result |= 0x08;
  return result;
}

static void bus_settle() {
  asm volatile("nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t");
}

static void data_input() { DATA_DDR = 0x00; DATA_PORT = 0x00; }
static void data_output(uint8_t value) { DATA_DDR = 0xFF; DATA_PORT = value; }

static void vera_write_register(uint8_t reg, uint8_t value) {
  const uint8_t address = register_address_bits(reg);
  CONTROL_PORT = address | CONTROL_IDLE;
  data_output(value);
  CONTROL_PORT = address | CONTROL_RD_N | CONTROL_WR_N;
  bus_settle();
  CONTROL_PORT = address | CONTROL_RD_N;
  bus_settle();
  CONTROL_PORT = address | CONTROL_RD_N | CONTROL_WR_N;
  bus_settle();
  CONTROL_PORT = address | CONTROL_IDLE;
  data_input();
}

static uint8_t vera_read_register(uint8_t reg) {
  const uint8_t address = register_address_bits(reg);
  CONTROL_PORT = address | CONTROL_IDLE;
  data_input();
  CONTROL_PORT = address | CONTROL_RD_N | CONTROL_WR_N;
  bus_settle();
  CONTROL_PORT = address | CONTROL_WR_N;
  bus_settle();
  const uint8_t value = DATA_PIN;
  CONTROL_PORT = address | CONTROL_RD_N | CONTROL_WR_N;
  bus_settle();
  CONTROL_PORT = address | CONTROL_IDLE;
  return value;
}

static void vera_set_vram_address(uint32_t address) {
  vera_write_register(REG_ADDR_L, address & 0xFF);
  vera_write_register(REG_ADDR_M, (address >> 8) & 0xFF);
  vera_write_register(REG_ADDR_H, ((address >> 16) & 0x01) | 0x10);
}

static void reset_vera() {
  digitalWrite(PIN_RESET_N, LOW);
  delay(10);
  digitalWrite(PIN_RESET_N, HIGH);
  delay(500);
}

static bool vera_spi_transfer(uint8_t transmit, uint8_t *receive) {
  // REG_SPI_DATA starts the transfer.  Polling happens entirely on the Mega,
  // avoiding a USB round trip for each SD-card byte.
  vera_write_register(0x1E, transmit);
  for (uint16_t poll = 0; poll < 2000; ++poll) {
    if ((vera_read_register(0x1F) & 0x80) == 0) {
      *receive = vera_read_register(0x1E);
      return true;
    }
  }
  return false;
}

static void send_response(uint8_t status, uint8_t response_opcode,
                          const uint8_t *response, uint16_t response_length) {
  uint8_t response_checksum = status ^ response_opcode ^ (uint8_t)response_length ^
                              (uint8_t)(response_length >> 8);
  Serial.write(RESPONSE_MARKER);
  Serial.write(status);
  Serial.write(response_opcode);
  Serial.write((uint8_t)response_length);
  Serial.write((uint8_t)(response_length >> 8));
  for (uint16_t i = 0; i < response_length; ++i) {
    Serial.write(response[i]);
    response_checksum ^= response[i];
  }
  Serial.write(response_checksum);
}

static void respond_status(uint8_t status) { send_response(status, opcode, nullptr, 0); }

static uint32_t payload_address() {
  return (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) |
         ((uint32_t)payload[2] << 16);
}

static void execute_request() {
  if (opcode == OP_PING && payload_length == 0) {
    const uint8_t protocol_version = 1;
    send_response(STATUS_OK, opcode, &protocol_version, 1);
  } else if (opcode == OP_RESET && payload_length == 0) {
    reset_vera();
    respond_status(STATUS_OK);
  } else if (opcode == OP_WRITE_REGISTER && payload_length == 2 && payload[0] < 32) {
    vera_write_register(payload[0], payload[1]);
    respond_status(STATUS_OK);
  } else if (opcode == OP_READ_REGISTER && payload_length == 1 && payload[0] < 32) {
    const uint8_t value = vera_read_register(payload[0]);
    send_response(STATUS_OK, opcode, &value, 1);
  } else if (opcode == OP_WRITE_VRAM && payload_length >= 4) {
    vera_set_vram_address(payload_address());
    for (uint16_t i = 3; i < payload_length; ++i) vera_write_register(REG_DATA0, payload[i]);
    respond_status(STATUS_OK);
  } else if (opcode == OP_READ_VRAM && payload_length == 3) {
    vera_set_vram_address(payload_address());
    delayMicroseconds(5);
    const uint8_t value = vera_read_register(REG_DATA0);
    send_response(STATUS_OK, opcode, &value, 1);
  } else if (opcode == OP_SPI_TRANSFER && payload_length > 0 && payload_length <= 1024) {
    for (uint16_t i = 0; i < payload_length; ++i) {
      if (!vera_spi_transfer(payload[i], &payload[i])) {
        send_response(STATUS_BAD_REQUEST, opcode, nullptr, 0);
        return;
      }
    }
    send_response(STATUS_OK, opcode, payload, payload_length);
  } else {
    respond_status(STATUS_BAD_REQUEST);
  }
}

static void reset_parser() {
  parser_state = WAIT_MARKER;
  payload_length = payload_index = 0;
  checksum = 0;
}

static void consume_byte(uint8_t value) {
  switch (parser_state) {
    case WAIT_MARKER:
      if (value == REQUEST_MARKER) parser_state = READ_OPCODE;
      break;
    case READ_OPCODE:
      opcode = value; checksum = value; parser_state = READ_LENGTH_LOW;
      break;
    case READ_LENGTH_LOW:
      payload_length = value; checksum ^= value; parser_state = READ_LENGTH_HIGH;
      break;
    case READ_LENGTH_HIGH:
      payload_length |= (uint16_t)value << 8; checksum ^= value;
      if (payload_length > MAX_PAYLOAD) {
        send_response(STATUS_BAD_LENGTH, opcode, nullptr, 0); reset_parser();
      } else {
        payload_index = 0;
        parser_state = payload_length == 0 ? READ_CHECKSUM : READ_PAYLOAD;
      }
      break;
    case READ_PAYLOAD:
      payload[payload_index++] = value; checksum ^= value;
      if (payload_index == payload_length) parser_state = READ_CHECKSUM;
      break;
    case READ_CHECKSUM:
      if (value == checksum) execute_request();
      else send_response(STATUS_BAD_FRAME, opcode, nullptr, 0);
      reset_parser();
      break;
  }
}

void setup() {
  // 500 kbaud is an exact divisor on the Mega's 16 MHz UART and is reliably
  // carried by its ATmega16U2 USB serial bridge.
  Serial.begin(500000);
  CONTROL_DDR = 0xFF;
  CONTROL_PORT = CONTROL_IDLE;
  data_input();
  pinMode(PIN_RESET_N, OUTPUT);
  digitalWrite(PIN_RESET_N, HIGH);
  reset_vera();
}

void loop() {
  while (Serial.available() > 0) consume_byte((uint8_t)Serial.read());
}
