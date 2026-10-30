// SSD1317 driver for a 128x64 panel, for U8g2. U8g2 only ships an SSD1317 driver for 96x96
// panels (u8x8_d_ssd1317.c), so this copies that driver and changes the init for 128x64.
// Init values come from the SSD1317 datasheet command table (Rev 1.2, Appendix II).
// UNTESTED on the screen: the init order is ours, not from a vendor sample.
#pragma once
#include <U8g2lib.h>

static const uint8_t ssd1317_128x64_init_seq[] = {
  U8X8_START_TRANSFER(),
  U8X8_C(0x0ae),                  // display off
  U8X8_CA(0x0d5, 0x080),          // clock divide 1, default oscillator frequency
  U8X8_CA(0x0a8, 0x03f),          // multiplex ratio: 64 rows
  U8X8_CA(0x0d3, 0x000),          // display offset 0
  U8X8_CA(0x0a2, 0x000),          // display start line 0
  U8X8_CA(0x020, 0x000),          // horizontal addressing mode
  U8X8_C(0x0a1),                  // segment remap: column 127 -> SEG0 (same as the SSD1306 128x64 driver)
  U8X8_C(0x0c8),                  // COM scan: remapped (same as the SSD1306 128x64 driver)
  U8X8_CA(0x0da, 0x012),          // COM pin config: alternative, no left/right remap (reset value)
  U8X8_CA(0x081, 0x09f),          // contrast
  U8X8_CA(0x0d9, 0x0f1),          // pre-charge period
  U8X8_CA(0x0db, 0x020),          // VCOMH deselect level ~0.78 x VCC (reset value)
  U8X8_C(0x02e),                  // deactivate scroll
  U8X8_C(0x0a4),                  // output follows RAM content
  U8X8_C(0x0a6),                  // normal (not inverted) display
  U8X8_END_TRANSFER(),
  U8X8_END()
};

static const u8x8_display_info_t ssd1317_128x64_display_info = {
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  /* post_chip_enable_wait_ns = */ 20,
  /* pre_chip_disable_wait_ns = */ 10,
  /* reset_pulse_width_ms = */ 100,
  /* post_reset_wait_ms = */ 100,
  /* sda_setup_time_ns = */ 50,
  /* sck_pulse_width_ns = */ 50,
  /* sck_clock_hz = */ 8000000UL,
  /* spi_mode = */ 0,
  /* i2c_bus_clock_100kHz = */ 4,
  /* data_setup_time_ns = */ 40,
  /* write_pulse_width_ns = */ 150,
  /* tile_width = */ 16,
  /* tile_height = */ 8,
  /* default_x_offset = */ 0,
  /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 128,
  /* pixel_height = */ 64
};

static uint8_t ssd1317_128x64_proc(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  uint8_t x, c;
  uint8_t *ptr;
  switch (msg) {
    case U8X8_MSG_DISPLAY_INIT:
      u8x8_d_helper_display_init(u8x8);
      u8x8_cad_SendSequence(u8x8, ssd1317_128x64_init_seq);
      break;
    case U8X8_MSG_DISPLAY_SETUP_MEMORY:
      u8x8_d_helper_display_setup_memory(u8x8, &ssd1317_128x64_display_info);
      break;
    case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
      u8x8_cad_StartTransfer(u8x8);
      u8x8_cad_SendCmd(u8x8, arg_int == 0 ? 0x0af : 0x0ae);
      u8x8_cad_EndTransfer(u8x8);
      break;
    case U8X8_MSG_DISPLAY_SET_CONTRAST:
      u8x8_cad_StartTransfer(u8x8);
      u8x8_cad_SendCmd(u8x8, 0x081);
      u8x8_cad_SendArg(u8x8, arg_int);
      u8x8_cad_EndTransfer(u8x8);
      break;
    case U8X8_MSG_DISPLAY_DRAW_TILE:
      u8x8_cad_StartTransfer(u8x8);
      x = ((u8x8_tile_t *)arg_ptr)->x_pos;
      x *= 8;
      x += u8x8->x_offset;
      u8x8_cad_SendCmd(u8x8, 0x040);                       // line offset 0
      u8x8_cad_SendCmd(u8x8, 0x010 | (x >> 4));            // column high nibble
      u8x8_cad_SendArg(u8x8, 0x000 | (x & 15));            // column low nibble
      u8x8_cad_SendArg(u8x8, 0x0b0 | ((u8x8_tile_t *)arg_ptr)->y_pos);  // page
      do {
        c = ((u8x8_tile_t *)arg_ptr)->cnt;
        ptr = ((u8x8_tile_t *)arg_ptr)->tile_ptr;
        u8x8_cad_SendData(u8x8, c * 8, ptr);
        arg_int--;
      } while (arg_int > 0);
      u8x8_cad_EndTransfer(u8x8);
      break;
    default:
      return 0;
  }
  return 1;
}

// U8g2 display class for the SSD1317 128x64 panel over hardware I2C. Same constructor as
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C, so the calling code does not change.
class U8G2_SSD1317_128X64_NONAME_F_HW_I2C : public U8G2 {
  public: U8G2_SSD1317_128X64_NONAME_F_HW_I2C(const u8g2_cb_t *rotation, uint8_t reset = U8X8_PIN_NONE, uint8_t clock = U8X8_PIN_NONE, uint8_t data = U8X8_PIN_NONE) : U8G2() {
    uint8_t tile_buf_height;
    uint8_t *buf;
    u8g2_SetupDisplay(&u8g2, ssd1317_128x64_proc, u8x8_cad_ssd13xx_fast_i2c, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
    buf = u8g2_m_16_8_f(&tile_buf_height);
    u8g2_SetupBuffer(&u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, rotation);
    u8x8_SetPin_HW_I2C(getU8x8(), reset, clock, data);
  }
};
