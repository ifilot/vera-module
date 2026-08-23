#include <Arduino.h>

#include "p2000c_font.h"

/*
  Arduino Mega -> CX16 VERA module smoke/demo program

  This sketch drives the VERA module through the module's 8-bit parallel bus.
  It does five things:

    1. Pulses hardware reset and then issues VERA's software reset command.
    2. Performs a tiny VRAM write/readback test so the serial monitor can tell
       us whether the bus path is basically alive.
    3. Renders text with the P2000C-derived 8x12 font into a VERA 1bpp bitmap
       framebuffer, producing an 80x40 amber-on-black text display.
    4. Plays a simple stereo PSG check: one beep on the left channel, then two
       beeps on the right channel.
    5. Initializes an SD card over VERA SPI, reads sector 0, and dumps it to
       the display as hex plus printable ASCII.
    6. Loads a small two-frame sprite and walks it across the sector dump.
    7. Runs an autonomous scene loop that continuously exercises video modes,
       palettes, glyph rendering, sprites, layer scrolling, audio, and SD reads.

  VERA exposes only 32 CPU-visible registers. VRAM is reached indirectly:
  first write ADDR_L/M/H, then read/write DATA0. The helper functions below
  hide that indirection so later drawing code can talk in VRAM addresses.
*/

// ---------------------------------------------------------------------------
// Arduino Mega pin mapping
// ---------------------------------------------------------------------------

// Arduino Mega pins 22..29 map to AVR PORTA bits 0..7.
// Wire these to VERA D0..D7 in the same order. Using a whole AVR port lets us
// place/read all data bits in a single register access instead of eight slower
// digitalWrite/digitalRead calls.
#define DATA_DDR DDRA
#define DATA_PORT PORTA
#define DATA_PIN PINA

// Arduino Mega pins 30..37 map to AVR PORTC bits 7..0:
//   pin 30 = PC7 = VERA A0
//   pin 31 = PC6 = VERA A1
//   pin 32 = PC5 = VERA A2
//   pin 33 = PC4 = VERA A3
//   pin 34 = PC3 = VERA A4
//   pin 35 = PC2 = VERA /CS
//   pin 36 = PC1 = VERA /RD
//   pin 37 = PC0 = VERA /WR
//
// The bit order is reversed for the address pins, but using one direct port
// write is still much faster than eight digitalWrite() calls per bus access.
#define CONTROL_DDR DDRC
#define CONTROL_PORT PORTC

const uint8_t control_cs_n = 0x04;
const uint8_t control_rd_n = 0x02;
const uint8_t control_wr_n = 0x01;
const uint8_t control_idle = control_cs_n | control_rd_n | control_wr_n;

// SRAM-style active-low control pins exposed by the VERA module carrier:
//   /CS gates an access,
//   /RD makes VERA drive D0..D7,
//   /WR makes VERA sample D0..D7,
//   /RESET resets/reconfigures the module.
const uint8_t pin_reset_n = 38;

// ---------------------------------------------------------------------------
// VERA register map constants
// ---------------------------------------------------------------------------

// VERA register numbers relative to the CX16 I/O window at $9F20.
// For example, CPU address $9F20 is register 0, $9F21 is register 1, etc.
const uint8_t reg_addr_l = 0x00; // $9F20
const uint8_t reg_addr_m = 0x01; // $9F21
const uint8_t reg_addr_h = 0x02; // $9F22
const uint8_t reg_data0 = 0x03;  // $9F23
const uint8_t reg_ctrl = 0x05; // $9F25
const uint8_t reg_dc_video = 0x09; // $9F29
const uint8_t reg_dc_hscale = 0x0A; // $9F2A
const uint8_t reg_dc_vscale = 0x0B; // $9F2B
const uint8_t reg_dc_border = 0x0C; // $9F2C
const uint8_t reg_l0_config = 0x0D; // $9F2D
const uint8_t reg_l0_mapbase = 0x0E; // $9F2E
const uint8_t reg_l0_tilebase = 0x0F; // $9F2F
const uint8_t reg_l0_hscroll_l = 0x10; // $9F30
const uint8_t reg_l0_hscroll_h = 0x11; // $9F31
const uint8_t reg_l0_vscroll_l = 0x12; // $9F32
const uint8_t reg_l0_vscroll_h = 0x13; // $9F33
const uint8_t reg_l1_config = 0x14; // $9F34
const uint8_t reg_l1_mapbase = 0x15; // $9F35
const uint8_t reg_l1_tilebase = 0x16; // $9F36
const uint8_t reg_l1_hscroll_l = 0x17; // $9F37
const uint8_t reg_l1_hscroll_h = 0x18; // $9F38
const uint8_t reg_l1_vscroll_l = 0x19; // $9F39
const uint8_t reg_l1_vscroll_h = 0x1A; // $9F3A
const uint8_t reg_spi_data = 0x1E; // $9F3E
const uint8_t reg_spi_ctrl = 0x1F; // $9F3F

// ---------------------------------------------------------------------------
// Demo memory layout
// ---------------------------------------------------------------------------

// A harmless location used only to prove VRAM reads/writes work before we
// spend time clearing the bitmap and drawing text.
const uint32_t test_address = 0x00000;
const uint8_t test_value = 0x5A;

// Layer 0 bitmap framebuffer starts at VRAM $00000. In 640x480 1bpp mode this
// consumes 640 * 480 / 8 = 38400 bytes, which fits comfortably in VERA VRAM.
const uint32_t bitmap_base = 0x00000;

// Palette and sprite/PSG registers live at the top of VRAM space. Palette
// entries are write-through hardware registers; reading this area returns VRAM
// backing bytes, not necessarily the live palette state.
const uint32_t psg_base = 0x1F9C0;
const uint32_t palette_base = 0x1FA00;
const uint32_t tile_map_base = 0x0A000;
const uint32_t tile_data_base = 0x0A800;
const uint32_t sprite_image_base = 0x13000;
const uint32_t sprite_attr_base = 0x1FC00;

const uint16_t screen_width_pixels = 640;
const uint16_t screen_height_pixels = 480;
const uint8_t bitmap_bytes_per_row = screen_width_pixels / 8;
const uint32_t bitmap_size_bytes =
    (uint32_t)bitmap_bytes_per_row * screen_height_pixels;
const uint8_t display_output_mode = 1;

// The P2000C-style font is rendered at its natural 8x12 pixel pitch.
const uint8_t text_columns = screen_width_pixels / p2000c_font_width;
const uint8_t text_rows = screen_height_pixels / p2000c_font_height;
const uint8_t text_bg_color = 0;
const uint8_t text_fg_color = 1;

// VERA palette entries are 12-bit RGB stored as two bytes:
// byte 0: GGGGBBBB, byte 1: ----RRRR.
// Amber here is full red plus medium green, no blue.
const uint8_t palette_black_lo = 0x00;
const uint8_t palette_black_hi = 0x00;
const uint8_t palette_amber_lo = 0x80;
const uint8_t palette_amber_hi = 0x0F;

const uint8_t palette_sprite_red = 2;
const uint8_t palette_sprite_blue = 3;
const uint8_t palette_sprite_skin = 4;
const uint8_t palette_sprite_white = 5;
const uint8_t palette_sprite_dark = 6;

// VERA PSG has 16 voices, each occupying four VRAM-mapped write-only registers:
// frequency low, frequency high, stereo/volume, and waveform/PW. A 440 Hz tone
// uses frequency word 1181 according to the VERA PSG frequency formula.
const uint8_t psg_voice_count = 16;
const uint8_t psg_voice_stride = 4;
const uint8_t psg_test_voice = 0;
const uint16_t psg_beep_frequency_word = 1181;
const uint8_t psg_beep_volume = 36;
const uint8_t psg_pan_left = 0x40;
const uint8_t psg_pan_right = 0x80;
const uint8_t psg_square_wave = 0x3F;
const uint16_t psg_beep_ms = 180;
const uint16_t psg_gap_ms = 140;

// VERA SPI_CTRL bits used for SD card access.
const uint8_t spi_ctrl_busy = 0x80;
const uint8_t spi_ctrl_slow_clock = 0x02;
const uint8_t spi_ctrl_select = 0x01;

// SD command constants. All commands are read-only here; CMD17 reads one
// 512-byte block, and sector 0 uses argument 0 on both SDHC and SDSC cards.
const uint8_t sd_cmd0_go_idle = 0;
const uint8_t sd_cmd8_send_if_cond = 8;
const uint8_t sd_cmd17_read_single_block = 17;
const uint8_t sd_cmd55_app_cmd = 55;
const uint8_t sd_cmd58_read_ocr = 58;
const uint8_t sd_acmd41_send_op_cond = 41;
const uint8_t sd_r1_idle = 0x01;
const uint8_t sd_r1_illegal_command = 0x04;
const uint8_t sd_data_token = 0xFE;
const uint16_t sd_sector_size = 512;
const uint8_t sd_init_attempts = 8;
const uint16_t sd_ready_retries = 500;
const uint16_t sd_command_response_retries = 32;
const uint16_t sd_acmd41_retries = 200;
const uint8_t sd_read_sector_retries = 4;

uint8_t sd_sector[sd_sector_size];

const uint8_t sprite_index = 0;
const uint8_t sprite_width_pixels = 16;
const uint8_t sprite_height_pixels = 16;
const uint16_t sprite_frame_bytes =
    (uint16_t)sprite_width_pixels * sprite_height_pixels / 2;
const uint16_t sprite_walk_start_y = 220;
const uint16_t sprite_walk_min_y = 160;
const uint16_t sprite_walk_max_y = 340;
const uint8_t sprite_walk_step_pixels = 4;
const uint8_t sprite_walk_y_step_pixels = 3;
const uint16_t sprite_walk_frame_ms = 45;
const uint16_t sprite_attr_bytes = 128 * 8;
const uint16_t tile_map_entries = 32 * 32;
const uint8_t tile_pattern_count = 8;
const uint16_t tile_scroll_frame_ms = 35;

enum DemoScene {
  scene_boot_summary,
  scene_color_bars,
  scene_glyph_grid,
  scene_sprite_walk,
  scene_tile_scroll,
  scene_audio,
  scene_sd_dump,
  scene_count
};

const uint16_t scene_durations_ms[scene_count] = {
    6000, 6000, 7000, 11000, 11000, 6000, 10000};

bool sprite_walk_enabled = false;
uint16_t sprite_walk_x = 0;
uint16_t sprite_walk_y = sprite_walk_start_y;
int8_t sprite_walk_y_direction = 1;
uint8_t sprite_walk_frame = 0;
unsigned long sprite_walk_last_ms = 0;
bool tile_scroll_enabled = false;
uint16_t tile_scroll_x = 0;
uint16_t tile_scroll_y = 0;
unsigned long tile_scroll_last_ms = 0;
DemoScene current_scene = scene_boot_summary;
unsigned long scene_started_ms = 0;
bool bus_test_ok = false;
bool sd_card_ok = false;

// Pack VERA A0..A4 into PORTC bits 7..3. The wiring uses consecutive Arduino
// pins, but the underlying AVR port bit order runs in the opposite direction.
uint8_t reg_address_bits(uint8_t reg) {
  uint8_t value = 0;
  if ((reg & 0x01) != 0) {
    value |= 0x80;
  }
  if ((reg & 0x02) != 0) {
    value |= 0x40;
  }
  if ((reg & 0x04) != 0) {
    value |= 0x20;
  }
  if ((reg & 0x08) != 0) {
    value |= 0x10;
  }
  if ((reg & 0x10) != 0) {
    value |= 0x08;
  }
  return value;
}

void bus_settle() {
  asm volatile("nop\n\t"
               "nop\n\t"
               "nop\n\t"
               "nop\n\t");
}

// Put the Arduino Mega data bus into output mode and present one byte.
void data_output(uint8_t value) {
  DATA_DDR = 0xFF;
  DATA_PORT = value;
}

// Release D0..D7 so VERA can drive the bus during reads. Pullups are disabled
// to avoid biasing or fighting the VERA side.
void data_input() {
  DATA_DDR = 0x00;
  DATA_PORT = 0x00;
}

// Write one CPU-visible VERA register.
//
// Set address/data first, assert /CS, pulse /WR low, then release the bus.
// PORTC strobes plus a few NOPs keep this much closer to a normal 6502-style
// bus cycle than the original digitalWrite()/microsecond-delay version.
void vera_write_reg(uint8_t reg, uint8_t value) {
  const uint8_t address = reg_address_bits(reg);
  CONTROL_PORT = address | control_idle;
  data_output(value);

  CONTROL_PORT = address | control_rd_n | control_wr_n;
  bus_settle();

  CONTROL_PORT = address | control_rd_n;
  bus_settle();
  CONTROL_PORT = address | control_rd_n | control_wr_n;

  bus_settle();
  CONTROL_PORT = address | control_idle;

  data_input();
}

// Read one CPU-visible VERA register.
//
// During the read strobe the Arduino data port must already be high-Z, or the
// Arduino and VERA could both drive D0..D7. The read value is sampled while
// /RD is low and /CS is active.
uint8_t vera_read_reg(uint8_t reg) {
  const uint8_t address = reg_address_bits(reg);
  CONTROL_PORT = address | control_idle;
  data_input();

  CONTROL_PORT = address | control_rd_n | control_wr_n;
  bus_settle();

  CONTROL_PORT = address | control_wr_n;
  bus_settle();
  const uint8_t value = DATA_PIN;
  CONTROL_PORT = address | control_rd_n | control_wr_n;

  bus_settle();
  CONTROL_PORT = address | control_idle;

  return value;
}

// Select the VRAM address used by DATA0.
//
// VERA VRAM is 17 bits wide. ADDR_L/M carry bits 0..15, and bit 0 of ADDR_H
// carries address bit 16. Bits 4..7 of ADDR_H select auto-increment; 0x10
// means "increment by 1 after every DATA0 access", which is ideal for bulk
// writes like clearing the bitmap framebuffer.
void vera_set_vram_address(uint32_t address) {
  vera_write_reg(reg_addr_l, address & 0xFF);
  vera_write_reg(reg_addr_m, (address >> 8) & 0xFF);

  // Bit 0 is address bit 16. Bits 4..7 set the DATA0 auto-increment mode.
  // 0x10 means increment by 1 after each DATA0 access.
  vera_write_reg(reg_addr_h, ((address >> 16) & 0x01) | 0x10);
}

// Random-access write of a single VRAM byte.
void vram_write(uint32_t address, uint8_t value) {
  vera_set_vram_address(address);
  vera_write_reg(reg_data0, value);
}

// Sequential write to the current DATA0 VRAM address. The address advances by
// one because vera_set_vram_address() selected auto-increment mode.
void vram_write_next(uint8_t value) {
  vera_write_reg(reg_data0, value);
}

// Fill a contiguous VRAM region. Used for simple clears or test patterns.
void vram_fill(uint32_t address, uint8_t value, uint32_t count) {
  vera_set_vram_address(address);

  for (uint32_t i = 0; i < count; i++) {
    vram_write_next(value);
  }
}

// Random-access read of a single VRAM byte.
uint8_t vram_read(uint32_t address) {
  vera_set_vram_address(address);
  return vera_read_reg(reg_data0);
}

// Return the VRAM byte address for a byte-aligned pixel position. In 1bpp
// bitmap mode each byte represents eight horizontal pixels. VERA renders bit 7
// first, then bit 6, down to bit 0.
uint32_t bitmap_byte_address(uint8_t text_column, uint8_t text_row,
                             uint8_t glyph_row) {
  const uint16_t y = (uint16_t)text_row * p2000c_font_height + glyph_row;
  return bitmap_base + (uint32_t)y * bitmap_bytes_per_row + text_column;
}

// Clear the full 640x480 1bpp framebuffer to palette color 0.
void clear_bitmap() {
  Serial.println(F("Clearing 1bpp bitmap..."));
  vram_fill(bitmap_base, 0x00, bitmap_size_bytes);
}

// Render one 8x12 character into the 1bpp framebuffer. Because the font width
// is exactly eight pixels, every character starts on a byte boundary and each
// glyph row is a single VRAM byte.
void draw_char(uint8_t text_column, uint8_t text_row, uint8_t character_code) {
  if (text_column >= text_columns || text_row >= text_rows) {
    return;
  }

  for (uint8_t row = 0; row < p2000c_font_height; row++) {
    const uint8_t pixels = pgm_read_byte(&p2000c_font[character_code][row]);
    vram_write(bitmap_byte_address(text_column, text_row, row), pixels);
  }
}

// Draw a string at the natural 8x12 text grid position.
void draw_text(uint8_t text_column, uint8_t text_row, const char *text) {
  while (*text != '\0' && text_column < text_columns) {
    draw_char(text_column, text_row, (uint8_t)*text);
    text_column++;
    text++;
  }
}

char hex_digit(uint8_t value) {
  value &= 0x0F;
  return value < 10 ? '0' + value : 'A' + value - 10;
}

void draw_hex_byte(uint8_t text_column, uint8_t text_row, uint8_t value) {
  draw_char(text_column, text_row, hex_digit(value >> 4));
  draw_char(text_column + 1, text_row, hex_digit(value));
}

void draw_hex_word(uint8_t text_column, uint8_t text_row, uint16_t value) {
  draw_hex_byte(text_column, text_row, value >> 8);
  draw_hex_byte(text_column + 2, text_row, value & 0xFF);
}

void draw_printable_byte(uint8_t text_column, uint8_t text_row, uint8_t value) {
  draw_char(text_column, text_row, value >= 32 && value <= 126 ? value : '.');
}

// Draw a filled 8x12 marker in one character cell. This is useful for checking
// the visible bitmap edges with the same pitch as the font renderer.
void draw_solid_cell(uint8_t text_column, uint8_t text_row) {
  if (text_column >= text_columns || text_row >= text_rows) {
    return;
  }

  for (uint8_t row = 0; row < p2000c_font_height; row++) {
    vram_write(bitmap_byte_address(text_column, text_row, row), 0xFF);
  }
}

void set_bitmap_pixel(uint16_t x, uint16_t y, bool enabled) {
  if (x >= screen_width_pixels || y >= screen_height_pixels) {
    return;
  }

  const uint32_t address = bitmap_base + (uint32_t)y * bitmap_bytes_per_row +
                           (x >> 3);
  const uint8_t mask = 0x80 >> (x & 0x07);
  const uint8_t current = vram_read(address);
  vram_write(address, enabled ? (current | mask) : (current & ~mask));
}

void draw_horizontal_line(uint16_t y, uint16_t x0, uint16_t x1) {
  if (y >= screen_height_pixels) {
    return;
  }

  for (uint16_t x = x0; x <= x1 && x < screen_width_pixels; x++) {
    set_bitmap_pixel(x, y, true);
  }
}

void draw_vertical_line(uint16_t x, uint16_t y0, uint16_t y1) {
  if (x >= screen_width_pixels) {
    return;
  }

  for (uint16_t y = y0; y <= y1 && y < screen_height_pixels; y++) {
    set_bitmap_pixel(x, y, true);
  }
}

// Mark the visible upper-right, lower-left, and lower-right cells.
void draw_corner_markers() {
  draw_solid_cell(text_columns - 1, 0);
  draw_solid_cell(0, text_rows - 1);
  draw_solid_cell(text_columns - 1, text_rows - 1);
}

// Write one 12-bit RGB palette entry. The format is VERA-specific:
//   low byte  = GGGG BBBB
//   high byte = ---- RRRR
void set_palette_entry(uint8_t index, uint8_t low, uint8_t high) {
  vera_set_vram_address(palette_base + (uint32_t)index * 2UL);
  vram_write_next(low);
  vram_write_next(high);
}

uint8_t dc_video_value(bool sprites_enabled, bool layer1_enabled,
                       bool layer0_enabled) {
  uint8_t value = display_output_mode & 0x03;
  if (sprites_enabled) {
    value |= 0x40;
  }
  if (layer1_enabled) {
    value |= 0x20;
  }
  if (layer0_enabled) {
    value |= 0x10;
  }
  return value;
}

void set_display_layers(bool sprites_enabled, bool layer1_enabled,
                        bool layer0_enabled) {
  vera_write_reg(reg_dc_video,
                 dc_video_value(sprites_enabled, layer1_enabled,
                                layer0_enabled));
}

void configure_text_bitmap_mode(bool sprites_enabled, bool layer1_enabled) {
  vera_write_reg(reg_ctrl, 0x00); // ADDRSEL=0, DCSEL=0.

  vera_write_reg(reg_dc_hscale, 128);
  vera_write_reg(reg_dc_vscale, 128);
  vera_write_reg(reg_dc_border, text_bg_color);

  vera_write_reg(reg_l0_config, 0x04);
  vera_write_reg(reg_l0_mapbase, 0x00);
  vera_write_reg(reg_l0_tilebase, (bitmap_base >> 9) | 0x01);
  vera_write_reg(reg_l0_hscroll_l, 0x00);
  vera_write_reg(reg_l0_hscroll_h, 0x00);
  vera_write_reg(reg_l0_vscroll_l, 0x00);
  vera_write_reg(reg_l0_vscroll_h, 0x00);

  set_display_layers(sprites_enabled, layer1_enabled, true);
}

void configure_4bpp_bitmap_mode() {
  vera_write_reg(reg_ctrl, 0x00);
  vera_write_reg(reg_dc_hscale, 64);
  vera_write_reg(reg_dc_vscale, 64);
  vera_write_reg(reg_dc_border, 0);

  // 4bpp bitmap mode, 320 pixels wide. At 320x240 this consumes the same
  // 38400 bytes as the normal 640x480 1bpp framebuffer.
  vera_write_reg(reg_l0_config, 0x06);
  vera_write_reg(reg_l0_mapbase, 0x00);
  vera_write_reg(reg_l0_tilebase, (bitmap_base >> 9) & 0xFC);
  vera_write_reg(reg_l0_hscroll_l, 0x00);
  vera_write_reg(reg_l0_hscroll_h, 0x00);
  vera_write_reg(reg_l0_vscroll_l, 0x00);
  vera_write_reg(reg_l0_vscroll_h, 0x00);

  set_display_layers(false, false, true);
}

void stop_sprite_walk_test() {
  sprite_walk_enabled = false;
  vram_fill(sprite_attr_base, 0x00, sprite_attr_bytes);
}

void stop_tile_scroll_test() {
  tile_scroll_enabled = false;
  vera_write_reg(reg_l1_hscroll_l, 0x00);
  vera_write_reg(reg_l1_hscroll_h, 0x00);
  vera_write_reg(reg_l1_vscroll_l, 0x00);
  vera_write_reg(reg_l1_vscroll_h, 0x00);
}

void prepare_text_scene(bool clear_screen) {
  stop_sprite_walk_test();
  stop_tile_scroll_test();
  configure_text_bitmap_mode(false, false);
  set_palette_entry(text_bg_color, palette_black_lo, palette_black_hi);
  set_palette_entry(text_fg_color, palette_amber_lo, palette_amber_hi);
  if (clear_screen) {
    clear_bitmap();
  }
}

uint32_t psg_voice_address(uint8_t voice) {
  return psg_base + (uint32_t)voice * psg_voice_stride;
}

void psg_write_voice(uint8_t voice, uint16_t frequency_word,
                     uint8_t stereo_volume, uint8_t waveform) {
  vera_set_vram_address(psg_voice_address(voice));
  vram_write_next(frequency_word & 0xFF);
  vram_write_next((frequency_word >> 8) & 0xFF);
  vram_write_next(stereo_volume);
  vram_write_next(waveform);
}

void psg_mute_voice(uint8_t voice) {
  psg_write_voice(voice, 0, 0, psg_square_wave);
}

void psg_mute_all_voices() {
  for (uint8_t voice = 0; voice < psg_voice_count; voice++) {
    psg_mute_voice(voice);
  }
}

void play_psg_beep(uint8_t pan) {
  psg_write_voice(psg_test_voice, psg_beep_frequency_word,
                  pan | psg_beep_volume, psg_square_wave);
  delay(psg_beep_ms);
  psg_mute_voice(psg_test_voice);
  delay(psg_gap_ms);
}

void run_sound_check() {
  Serial.println(F("Starting VERA PSG stereo sound check..."));
  psg_mute_all_voices();

  play_psg_beep(psg_pan_left);
  delay(psg_gap_ms);
  play_psg_beep(psg_pan_right);
  play_psg_beep(psg_pan_right);

  psg_mute_all_voices();
  Serial.println(F("Sound check: left one beep, right two beeps"));
}

uint8_t sprite_pixel_color(uint8_t x, uint8_t y, uint8_t frame) {
  if (y <= 1 && x >= 5 && x <= 10) {
    return palette_sprite_red;
  }
  if (y == 2 && x >= 4 && x <= 12) {
    return palette_sprite_red;
  }
  if (y >= 3 && y <= 5 && x >= 5 && x <= 10) {
    if (y == 4 && (x == 7 || x == 9)) {
      return palette_sprite_white;
    }
    return palette_sprite_skin;
  }
  if (y == 6 && x >= 7 && x <= 8) {
    return palette_sprite_skin;
  }
  if (y >= 7 && y <= 10 && x >= 5 && x <= 10) {
    return palette_sprite_blue;
  }

  const bool arm_swing_forward = frame == 0;
  if (y >= 7 && y <= 10) {
    if ((arm_swing_forward && x == 3) || (!arm_swing_forward && x == 12)) {
      return palette_sprite_skin;
    }
  }

  if (y >= 11 && y <= 14) {
    if (frame == 0) {
      if ((x >= 5 && x <= 6) || (x >= 9 && x <= 10)) {
        return palette_sprite_red;
      }
    } else {
      if ((x >= 4 && x <= 5) || (x >= 10 && x <= 11)) {
        return palette_sprite_red;
      }
    }
  }

  if (y == 15) {
    if (frame == 0) {
      if ((x >= 4 && x <= 6) || (x >= 9 && x <= 11)) {
        return palette_sprite_dark;
      }
    } else {
      if ((x >= 3 && x <= 5) || (x >= 10 && x <= 12)) {
        return palette_sprite_dark;
      }
    }
  }

  return 0;
}

void load_sprite_frame(uint8_t frame) {
  vera_set_vram_address(sprite_image_base +
                        (uint32_t)frame * sprite_frame_bytes);

  for (uint8_t y = 0; y < sprite_height_pixels; y++) {
    for (uint8_t x = 0; x < sprite_width_pixels; x += 2) {
      const uint8_t left = sprite_pixel_color(x, y, frame);
      const uint8_t right = sprite_pixel_color(x + 1, y, frame);
      vram_write_next((left << 4) | right);
    }
  }
}

void load_sprite_frames() {
  load_sprite_frame(0);
  load_sprite_frame(1);
}

void write_sprite_attributes(uint16_t x, uint16_t y, uint8_t frame) {
  const uint32_t image_address =
      sprite_image_base + (uint32_t)(frame & 0x01) * sprite_frame_bytes;
  const uint32_t attr_address = sprite_attr_base + (uint32_t)sprite_index * 8UL;

  vera_set_vram_address(attr_address);
  vram_write_next((image_address >> 5) & 0xFF);
  vram_write_next((image_address >> 13) & 0x0F); // 4bpp sprite mode.
  vram_write_next(x & 0xFF);
  vram_write_next((x >> 8) & 0x03);
  vram_write_next(y & 0xFF);
  vram_write_next((y >> 8) & 0x03);
  vram_write_next(0x0C); // Z-depth 3, no flips.
  vram_write_next(0x50); // 16x16 sprite, palette offset 0.
}

void setup_sprite_walk_test() {
  Serial.println(F("Starting VERA sprite walk test..."));

  set_palette_entry(palette_sprite_red, 0x00, 0x0F);
  set_palette_entry(palette_sprite_blue, 0x0F, 0x00);
  set_palette_entry(palette_sprite_skin, 0xA6, 0x0F);
  set_palette_entry(palette_sprite_white, 0xFF, 0x0F);
  set_palette_entry(palette_sprite_dark, 0x22, 0x02);

  vram_fill(sprite_attr_base, 0x00, sprite_attr_bytes);
  load_sprite_frames();
  write_sprite_attributes(0, sprite_walk_start_y, 0);

  // Keep layer 0 enabled and add sprite rendering on top.
  vera_write_reg(reg_dc_video, 0x50 | (display_output_mode & 0x03));

  draw_text(0, 38, "SPRITE WALK: 4BPP 16X16 ATTRIBUTE ANIMATION");
  sprite_walk_x = 0;
  sprite_walk_y = sprite_walk_start_y;
  sprite_walk_y_direction = 1;
  sprite_walk_frame = 0;
  sprite_walk_last_ms = millis();
  sprite_walk_enabled = true;
}

void update_sprite_walk_test() {
  if (!sprite_walk_enabled) {
    return;
  }

  const unsigned long now = millis();
  if (now - sprite_walk_last_ms < sprite_walk_frame_ms) {
    return;
  }

  sprite_walk_last_ms = now;
  sprite_walk_frame ^= 0x01;

  if (sprite_walk_x >= screen_width_pixels - sprite_width_pixels) {
    sprite_walk_x = 0;
  } else {
    sprite_walk_x += sprite_walk_step_pixels;
  }

  if (sprite_walk_y_direction > 0) {
    sprite_walk_y += sprite_walk_y_step_pixels;
    if (sprite_walk_y >= sprite_walk_max_y) {
      sprite_walk_y = sprite_walk_max_y;
      sprite_walk_y_direction = -1;
    }
  } else {
    sprite_walk_y -= sprite_walk_y_step_pixels;
    if (sprite_walk_y <= sprite_walk_min_y) {
      sprite_walk_y = sprite_walk_min_y;
      sprite_walk_y_direction = 1;
    }
  }

  write_sprite_attributes(sprite_walk_x, sprite_walk_y, sprite_walk_frame);
}

bool spi_wait_not_busy(uint16_t retries) {
  while (retries-- > 0) {
    if ((vera_read_reg(reg_spi_ctrl) & spi_ctrl_busy) == 0) {
      return true;
    }
    delayMicroseconds(10);
  }

  return false;
}

void spi_set_control(bool selected, bool slow_clock) {
  uint8_t value = 0;
  if (selected) {
    value |= spi_ctrl_select;
  }
  if (slow_clock) {
    value |= spi_ctrl_slow_clock;
  }
  vera_write_reg(reg_spi_ctrl, value);
}

uint8_t spi_transfer(uint8_t value) {
  if (!spi_wait_not_busy(1000)) {
    return 0xFF;
  }

  vera_write_reg(reg_spi_data, value);
  if (!spi_wait_not_busy(1000)) {
    return 0xFF;
  }

  return vera_read_reg(reg_spi_data);
}

void sd_deselect() {
  spi_set_control(false, true);
  spi_transfer(0xFF);
}

void sd_select() {
  spi_set_control(true, true);
  spi_transfer(0xFF);
}

bool sd_wait_ready(uint16_t retries) {
  while (retries-- > 0) {
    if (spi_transfer(0xFF) == 0xFF) {
      return true;
    }
    delay(1);
  }

  return false;
}

uint8_t sd_command_crc(uint8_t command) {
  if (command == sd_cmd0_go_idle) {
    return 0x95;
  }
  if (command == sd_cmd8_send_if_cond) {
    return 0x87;
  }
  return 0x01;
}

uint8_t sd_send_command(uint8_t command, uint32_t argument) {
  sd_deselect();
  sd_select();

  if (!sd_wait_ready(sd_ready_retries)) {
    return 0xFF;
  }

  spi_transfer(0x40 | command);
  spi_transfer((argument >> 24) & 0xFF);
  spi_transfer((argument >> 16) & 0xFF);
  spi_transfer((argument >> 8) & 0xFF);
  spi_transfer(argument & 0xFF);
  spi_transfer(sd_command_crc(command));

  for (uint16_t i = 0; i < sd_command_response_retries; i++) {
    const uint8_t response = spi_transfer(0xFF);
    if ((response & 0x80) == 0) {
      return response;
    }
  }

  return 0xFF;
}

void sd_send_initial_clocks() {
  spi_set_control(false, true);
  for (uint8_t i = 0; i < 10; i++) {
    spi_transfer(0xFF);
  }
}

bool sd_enter_idle() {
  for (uint8_t attempt = 0; attempt < sd_init_attempts; attempt++) {
    const uint8_t response = sd_send_command(sd_cmd0_go_idle, 0);
    sd_deselect();

    if (response == sd_r1_idle) {
      return true;
    }

    delay(50);
  }

  return false;
}

bool sd_check_voltage(bool *sd_v2) {
  *sd_v2 = false;

  const uint8_t response = sd_send_command(sd_cmd8_send_if_cond, 0x000001AA);
  if (response == sd_r1_idle) {
    const uint8_t r7_0 = spi_transfer(0xFF);
    const uint8_t r7_1 = spi_transfer(0xFF);
    const uint8_t r7_2 = spi_transfer(0xFF);
    const uint8_t r7_3 = spi_transfer(0xFF);
    sd_deselect();

    *sd_v2 = r7_0 == 0x00 && r7_1 == 0x00 && r7_2 == 0x01 && r7_3 == 0xAA;
    return *sd_v2;
  }

  sd_deselect();

  // Older SDSC cards can reject CMD8. They can still be initialized with ACMD41.
  if ((response & sd_r1_illegal_command) != 0) {
    return true;
  }

  return false;
}

bool sd_finish_initialization(bool sd_v2) {
  const uint32_t argument = sd_v2 ? 0x40000000UL : 0x00000000UL;

  for (uint16_t attempt = 0; attempt < sd_acmd41_retries; attempt++) {
    uint8_t response = sd_send_command(sd_cmd55_app_cmd, 0);
    sd_deselect();

    if (response > sd_r1_idle) {
      delay(10);
      continue;
    }

    response = sd_send_command(sd_acmd41_send_op_cond, argument);
    sd_deselect();

    if (response == 0x00) {
      return true;
    }

    delay(10);
  }

  return false;
}

bool sd_read_ocr_if_available(bool sd_v2) {
  if (!sd_v2) {
    return true;
  }

  const uint8_t response = sd_send_command(sd_cmd58_read_ocr, 0);
  if (response != 0x00) {
    sd_deselect();
    return false;
  }

  for (uint8_t i = 0; i < 4; i++) {
    spi_transfer(0xFF);
  }
  sd_deselect();
  return true;
}

bool sd_read_sector0() {
  for (uint8_t read_attempt = 0; read_attempt < sd_read_sector_retries;
       read_attempt++) {
    const uint8_t response = sd_send_command(sd_cmd17_read_single_block, 0);
    if (response != 0x00) {
      sd_deselect();
      delay(20);
      continue;
    }

    uint8_t token = 0xFF;
    for (uint16_t token_attempt = 0; token_attempt < 60000; token_attempt++) {
      token = spi_transfer(0xFF);
      if (token == sd_data_token) {
        break;
      }
      delayMicroseconds(50);
    }

    if (token != sd_data_token) {
      sd_deselect();
      delay(20);
      continue;
    }

    for (uint16_t i = 0; i < sd_sector_size; i++) {
      sd_sector[i] = spi_transfer(0xFF);
    }

    // Discard the two-byte block CRC. CRC checking is disabled in SPI mode after
    // reset, and this is a smoke test rather than a full storage stack.
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    sd_deselect();
    return true;
  }

  return false;
}

void draw_sd_status_screen(const char *status) {
  clear_bitmap();
  draw_text(0, 0, "VERA SD CARD CHECK");
  draw_text(0, 2, status);
  draw_text(0, 4, "NO SECTOR DATA TO DISPLAY.");
  draw_text(0, 6, "CHECK CARD INSERTION, VERA SD WIRING, AND JP1/CS TARGET.");
}

void draw_sd_sector_dump() {
  clear_bitmap();
  draw_text(0, 0, "VERA SD CARD CHECK - SECTOR 0");
  draw_text(0, 1, "OFFSET  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII");

  for (uint8_t row = 0; row < 32; row++) {
    const uint16_t offset = (uint16_t)row * 16;
    const uint8_t text_row = row + 3;

    draw_hex_word(0, text_row, offset);
    draw_char(4, text_row, ':');

    for (uint8_t column = 0; column < 16; column++) {
      const uint8_t value = sd_sector[offset + column];
      const uint8_t hex_column = 8 + column * 3;
      draw_hex_byte(hex_column, text_row, value);
      draw_printable_byte(59 + column, text_row, value);
    }
  }

  draw_text(0, 36, "SD READ: OK  CMD0/CMD8/ACMD41/CMD17");
}

bool run_sd_card_check() {
  Serial.println(F("Starting VERA SD card sector 0 check..."));
  prepare_text_scene(true);
  sd_send_initial_clocks();

  if (!sd_enter_idle()) {
    Serial.println(F("SD check failed: CMD0 did not enter idle state"));
    draw_sd_status_screen("SD FAIL: CMD0 DID NOT ENTER IDLE STATE");
    return false;
  }

  bool sd_v2 = false;
  if (!sd_check_voltage(&sd_v2)) {
    Serial.println(F("SD check failed: CMD8 voltage check failed"));
    draw_sd_status_screen("SD FAIL: CMD8 VOLTAGE CHECK FAILED");
    return false;
  }

  if (!sd_finish_initialization(sd_v2)) {
    Serial.println(F("SD check failed: ACMD41 did not finish initialization"));
    draw_sd_status_screen("SD FAIL: ACMD41 INIT TIMEOUT");
    return false;
  }

  if (!sd_read_ocr_if_available(sd_v2)) {
    Serial.println(F("SD check failed: CMD58 OCR read failed"));
    draw_sd_status_screen("SD FAIL: CMD58 OCR READ FAILED");
    return false;
  }

  if (!sd_read_sector0()) {
    Serial.println(F("SD check failed: CMD17 sector 0 read failed"));
    draw_sd_status_screen("SD FAIL: CMD17 SECTOR 0 READ FAILED");
    return false;
  }

  draw_sd_sector_dump();
  Serial.println(F("SD check: sector 0 read and displayed"));
  return true;
}

void draw_scene_title(const char *title, const char *subtitle) {
  draw_text(0, 0, "VERA AUTONOMOUS DIAGNOSTICS");
  draw_text(0, 1, title);
  if (subtitle != nullptr) {
    draw_text(0, 2, subtitle);
  }
}

void enter_boot_summary_scene() {
  Serial.println(F("Scene: boot summary"));
  prepare_text_scene(true);
  draw_scene_title("SCENE 1: BOOT SUMMARY", "BUS, VRAM, VIDEO, AUDIO, SD, SPRITES, LAYERS");
  draw_text(0, 5, bus_test_ok ? "VRAM READBACK: PASS" : "VRAM READBACK: FAIL");
  draw_text(0, 6, sd_card_ok ? "SD CARD:       PASS" : "SD CARD:       NOT YET CHECKED / FAIL");
  draw_text(0, 8, "THIS LOOP RUNS WITHOUT KEYBOARD INPUT.");
  draw_text(0, 10, "NEXT: 4BPP BITMAP PALETTE BARS");
  draw_corner_markers();
}

void enter_color_bars_scene() {
  Serial.println(F("Scene: 4bpp bitmap color bars"));
  stop_sprite_walk_test();
  stop_tile_scroll_test();
  configure_4bpp_bitmap_mode();

  const uint8_t palette_lows[16] = {
      0x00, 0x00, 0x0F, 0xF0, 0xFF, 0x08, 0x80, 0x88,
      0x44, 0x04, 0x40, 0x0A, 0xA0, 0xAA, 0x66, 0xFF};
  const uint8_t palette_highs[16] = {
      0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x08,
      0x04, 0x08, 0x08, 0x02, 0x02, 0x0A, 0x06, 0x0F};

  for (uint8_t i = 0; i < 16; i++) {
    set_palette_entry(i, palette_lows[i], palette_highs[i]);
  }

  vera_set_vram_address(bitmap_base);
  for (uint16_t y = 0; y < 240; y++) {
    for (uint16_t byte_x = 0; byte_x < 160; byte_x++) {
      uint8_t color = byte_x / 10;
      if ((y & 0x10) != 0) {
        color = 15 - color;
      }
      vram_write_next((color << 4) | color);
    }
  }
}

void enter_glyph_grid_scene() {
  Serial.println(F("Scene: glyph grid"));
  prepare_text_scene(true);
  draw_scene_title("SCENE 3: GLYPH GRID", "FONT, BITMAP ADDRESSING, AND FULL-SCREEN WRITES");

  uint8_t glyph = 0;
  for (uint8_t row = 4; row < text_rows; row++) {
    for (uint8_t column = 0; column < text_columns; column++) {
      draw_char(column, row, glyph);
      glyph++;
    }
  }
}

void enter_sprite_walk_scene() {
  Serial.println(F("Scene: sprite walk"));
  prepare_text_scene(true);
  draw_scene_title("SCENE 4: SPRITE WALK", "4BPP SPRITE DATA, PALETTE, ATTRIBUTES, Z-DEPTH");
  draw_text(0, 5, "THE SPRITE IS GENERATED IN CODE AND WALKS ACROSS THIS SCREEN.");
  draw_text(0, 7, "TESTING: SPRITE VRAM, ATTR TABLE, X/Y UPDATES, FRAME SWITCHING.");
  setup_sprite_walk_test();
}

uint8_t tile_pattern_byte(uint8_t tile, uint8_t row) {
  switch (tile & 0x07) {
  case 0:
    return 0x00;
  case 1:
    return row < 4 ? 0xF0 : 0x0F;
  case 2:
    return row < 4 ? 0x0F : 0xF0;
  case 3:
    return row < 4 ? 0xFF : 0x00;
  case 4:
    return row < 4 ? 0x00 : 0xFF;
  case 5:
    return 0xF0;
  case 6:
    return 0x0F;
  default:
    return (row & 0x04) ? 0x0F : 0xF0;
  }
}

void load_tile_scroll_assets() {
  vera_set_vram_address(tile_data_base);
  for (uint8_t tile = 0; tile < tile_pattern_count; tile++) {
    for (uint8_t row = 0; row < 8; row++) {
      vram_write_next(tile_pattern_byte(tile, row));
    }
  }

  vera_set_vram_address(tile_map_base);
  for (uint16_t i = 0; i < tile_map_entries; i++) {
    const uint8_t x = i & 31;
    const uint8_t y = i >> 5;
    const uint8_t tile = 1 + ((x + y) & 0x01);
    const uint8_t color = ((x ^ y) & 0x02) ? 3 : 2;

    vram_write_next(tile);
    vram_write_next(color);
  }
}

void enter_tile_scroll_scene() {
  Serial.println(F("Scene: layer 1 tile scroll"));
  prepare_text_scene(true);
  draw_scene_title("SCENE 5: LAYER 1 TILE SCROLL", "TRANSPARENT TILEMAP OVER 1BPP BITMAP LAYER");
  draw_text(0, 5, "TESTING: LAYER 1 MAPBASE, TILEBASE, TRANSPARENCY, H/V SCROLL.");
  draw_text(0, 7, "STATIC BACKGROUND GRID; SOFT 4X4 CHECKER TILES DRIFT DIAGONALLY.");

  for (uint16_t x = 0; x < screen_width_pixels; x += 80) {
    draw_vertical_line(x, 120, 420);
  }
  for (uint16_t y = 120; y <= 420; y += 60) {
    draw_horizontal_line(y, 0, screen_width_pixels - 1);
  }

  set_palette_entry(2, 0xC6, 0x04); // soft teal
  set_palette_entry(3, 0x9D, 0x0B); // soft peach

  load_tile_scroll_assets();
  vera_write_reg(reg_l1_config, 0x08); // 1bpp tile mode, T256C transparent bg.
  vera_write_reg(reg_l1_mapbase, tile_map_base >> 9);
  vera_write_reg(reg_l1_tilebase, (tile_data_base >> 9) & 0xFC);
  vera_write_reg(reg_l1_hscroll_l, 0x00);
  vera_write_reg(reg_l1_hscroll_h, 0x00);
  vera_write_reg(reg_l1_vscroll_l, 0x00);
  vera_write_reg(reg_l1_vscroll_h, 0x00);
  set_display_layers(false, true, true);

  tile_scroll_x = 0;
  tile_scroll_y = 0;
  tile_scroll_last_ms = millis();
  tile_scroll_enabled = true;
}

void update_tile_scroll_scene() {
  if (!tile_scroll_enabled) {
    return;
  }

  const unsigned long now = millis();
  if (now - tile_scroll_last_ms < tile_scroll_frame_ms) {
    return;
  }

  tile_scroll_last_ms = now;
  tile_scroll_x += 1;
  tile_scroll_y += 2;
  vera_write_reg(reg_l1_hscroll_l, tile_scroll_x & 0xFF);
  vera_write_reg(reg_l1_hscroll_h, (tile_scroll_x >> 8) & 0x0F);
  vera_write_reg(reg_l1_vscroll_l, tile_scroll_y & 0xFF);
  vera_write_reg(reg_l1_vscroll_h, (tile_scroll_y >> 8) & 0x0F);
}

void enter_audio_scene() {
  Serial.println(F("Scene: PSG audio"));
  prepare_text_scene(true);
  draw_scene_title("SCENE 6: PSG AUDIO", "LEFT BEEP, RIGHT-RIGHT BEEP, THEN SILENCE");
  draw_text(0, 5, "LISTEN FOR: LEFT ONCE, RIGHT TWICE.");
  run_sound_check();
  draw_text(0, 8, "AUDIO CYCLE COMPLETE.");
}

void enter_sd_dump_scene() {
  Serial.println(F("Scene: SD sector read"));
  sd_card_ok = run_sd_card_check();
  if (sd_card_ok) {
    setup_sprite_walk_test();
  }
}

void enter_scene(DemoScene scene) {
  current_scene = scene;

  switch (scene) {
  case scene_boot_summary:
    enter_boot_summary_scene();
    break;
  case scene_color_bars:
    enter_color_bars_scene();
    break;
  case scene_glyph_grid:
    enter_glyph_grid_scene();
    break;
  case scene_sprite_walk:
    enter_sprite_walk_scene();
    break;
  case scene_tile_scroll:
    enter_tile_scroll_scene();
    break;
  case scene_audio:
    enter_audio_scene();
    break;
  case scene_sd_dump:
    enter_sd_dump_scene();
    break;
  default:
    enter_boot_summary_scene();
    break;
  }

  scene_started_ms = millis();
}

void update_scene() {
  switch (current_scene) {
  case scene_sprite_walk:
  case scene_sd_dump:
    update_sprite_walk_test();
    break;
  case scene_tile_scroll:
    update_tile_scroll_scene();
    break;
  default:
    break;
  }

  const unsigned long now = millis();
  if (now - scene_started_ms >= scene_durations_ms[current_scene]) {
    const DemoScene next_scene =
        (DemoScene)(((uint8_t)current_scene + 1) % scene_count);
    enter_scene(next_scene);
  }
}

// Hardware reset through the module header. The long post-reset delay gives the
// FPGA time to configure itself before we begin register accesses.
void reset_vera() {
  digitalWrite(pin_reset_n, LOW);
  delay(10);
  digitalWrite(pin_reset_n, HIGH);
  delay(500);
}

// VERA also has a software reset/reconfigure bit in CTRL bit 7. We use it
// after the hardware reset so the demo starts from a clean internal state even
// if the board had been left configured by an earlier run.
void command_reset_vera() {
  Serial.println(F("Commanding VERA reset..."));
  vera_write_reg(reg_ctrl, 0x80);
  delay(500);
  vera_write_reg(reg_ctrl, 0x00);
}

// Put all Arduino pins into a benign idle state before talking to VERA.
//
// Idle means:
//   /CS high, so VERA ignores the bus
//   /RD high and /WR high, so no cycle is active
//   D0..D7 as inputs, so the Arduino is not driving the data bus
void setup_pins() {
  CONTROL_DDR = 0xFF;
  CONTROL_PORT = control_idle;

  pinMode(pin_reset_n, OUTPUT);
  digitalWrite(pin_reset_n, HIGH);

  data_input();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }

  // Bring the hardware into a known state first, then ask VERA to reconfigure
  // itself via CTRL_RESET.
  setup_pins();
  reset_vera();
  command_reset_vera();

  // Minimal bus confidence test. If this fails, there is no point debugging
  // higher-level video setup yet: focus on D0..D7, A0..A4, /CS, /RD, /WR, reset,
  // common ground, and power.
  vram_write(test_address, test_value);
  const uint8_t got = vram_read(test_address);

  Serial.print(F("VERA VRAM readback: 0x"));
  if (got < 0x10) {
    Serial.print('0');
  }
  Serial.println(got, HEX);

  bus_test_ok = got == test_value;

  if (bus_test_ok) {
    Serial.println(F("PASS: VERA bus and VRAM access work"));
  } else {
    Serial.println(F("FAIL: check wiring, voltage levels, reset, and strobes"));
  }

  enter_scene(scene_boot_summary);
}

void loop() {
  update_scene();
}
