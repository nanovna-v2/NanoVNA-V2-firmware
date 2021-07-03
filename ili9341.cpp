/*
 * Copyright (c) 2014-2015, TAKAHASHI Tomohiro (TTRFTECH) edy555@gmail.com
 * All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * The software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */
#include "ili9341.hpp"
#include "Font.h"
#include "plot.hpp"

// Display commands list
#define ILI9341_NOP                        0x00
#define ILI9341_SOFTWARE_RESET             0x01
#define ILI9341_READ_IDENTIFICATION        0x04
#define ILI9341_READ_STATUS                0x09
#define ILI9341_READ_POWER_MODE            0x0A
#define ILI9341_READ_MADCTL                0x0B
#define ILI9341_READ_PIXEL_FORMAT          0x0C
#define ILI9341_READ_IMAGE_FORMAT          0x0D
#define ILI9341_READ_SIGNAL_MODE           0x0E
#define ILI9341_READ_SELF_DIAGNOSTIC       0x0F
#define ILI9341_SLEEP_IN                   0x10
#define ILI9341_SLEEP_OUT                  0x11
#define ILI9341_PARTIAL_MODE_ON            0x12
#define ILI9341_NORMAL_DISPLAY_MODE_ON     0x13
#define ILI9341_INVERSION_OFF              0x20
#define ILI9341_INVERSION_ON               0x21
#define ILI9341_GAMMA_SET                  0x26
#define ILI9341_DISPLAY_OFF                0x28
#define ILI9341_DISPLAY_ON                 0x29
#define ILI9341_COLUMN_ADDRESS_SET         0x2A
#define ILI9341_PAGE_ADDRESS_SET           0x2B
#define ILI9341_MEMORY_WRITE               0x2C
#define ILI9341_COLOR_SET                  0x2D
#define ILI9341_MEMORY_READ                0x2E
#define ILI9341_PARTIAL_AREA               0x30
#define ILI9341_VERTICAL_SCROLLING_DEF     0x33
#define ILI9341_TEARING_LINE_OFF           0x34
#define ILI9341_TEARING_LINE_ON            0x35
#define ILI9341_MEMORY_ACCESS_CONTROL      0x36
#define ILI9341_VERTICAL_SCROLLING         0x37
#define ILI9341_IDLE_MODE_OFF              0x38
#define ILI9341_IDLE_MODE_ON               0x39
#define ILI9341_PIXEL_FORMAT_SET           0x3A
#define ILI9341_WRITE_MEMORY_CONTINUE      0x3C
#define ILI9341_READ_MEMORY_CONTINUE       0x3E
#define ILI9341_SET_TEAR_SCANLINE          0x44
#define ILI9341_GET_SCANLINE               0x45
#define ILI9341_WRITE_BRIGHTNESS           0x51
#define ILI9341_READ_BRIGHTNESS            0x52
#define ILI9341_WRITE_CTRL_DISPLAY         0x53
#define ILI9341_READ_CTRL_DISPLAY          0x54
#define ILI9341_WRITE_CA_BRIGHTNESS        0x55
#define ILI9341_READ_CA_BRIGHTNESS         0x56
#define ILI9341_WRITE_CA_MIN_BRIGHTNESS    0x5E
#define ILI9341_READ_CA_MIN_BRIGHTNESS     0x5F
#define ILI9341_READ_ID1                   0xDA
#define ILI9341_READ_ID2                   0xDB
#define ILI9341_READ_ID3                   0xDC
#define ILI9341_RGB_INTERFACE_CONTROL      0xB0
#define ILI9341_FRAME_RATE_CONTROL_1       0xB1
#define ILI9341_FRAME_RATE_CONTROL_2       0xB2
#define ILI9341_FRAME_RATE_CONTROL_3       0xB3
#define ILI9341_DISPLAY_INVERSION_CONTROL  0xB4
#define ILI9341_BLANKING_PORCH_CONTROL     0xB5
#define ILI9341_DISPLAY_FUNCTION_CONTROL   0xB6
#define ILI9341_ENTRY_MODE_SET             0xB7
#define ILI9341_BACKLIGHT_CONTROL_1        0xB8
#define ILI9341_BACKLIGHT_CONTROL_2        0xB9
#define ILI9341_BACKLIGHT_CONTROL_3        0xBA
#define ILI9341_BACKLIGHT_CONTROL_4        0xBB
#define ILI9341_BACKLIGHT_CONTROL_5        0xBC
#define ILI9341_BACKLIGHT_CONTROL_7        0xBE
#define ILI9341_BACKLIGHT_CONTROL_8        0xBF
#define ILI9341_POWER_CONTROL_1            0xC0
#define ILI9341_POWER_CONTROL_2            0xC1
#define ILI9341_POWER_CONTROL_3            0xC2
#define ILI9341_VCOM_CONTROL_1             0xC5
#define ILI9341_VCOM_CONTROL_2             0xC7
#define ILI9341_POWERA                     0xCB
#define ILI9341_POWERB                     0xCF
#define ILI9341_NV_MEMORY_WRITE            0xD0
#define ILI9341_NV_PROTECTION_KEY          0xD1
#define ILI9341_NV_STATUS_READ             0xD2
#define ILI9341_READ_ID4                   0xD3
#define ILI9341_POSITIVE_GAMMA_CORRECTION  0xE0
#define ILI9341_NEGATIVE_GAMMA_CORRECTION  0xE1
#define ILI9341_DIGITAL_GAMMA_CONTROL_1    0xE2
#define ILI9341_DIGITAL_GAMMA_CONTROL_2    0xE3
#define ILI9341_DTCA                       0xE8
#define ILI9341_DTCB                       0xEA
#define ILI9341_POWER_SEQ                  0xED
#define ILI9341_3GAMMA_EN                  0xF2
#define ILI9341_INTERFACE_CONTROL          0xF6
#define ILI9341_CSCON                      0xF0
#define ILI9341_PUMP_RATIO_CONTROL         0xF7

//
// ILI9341_MEMORY_ACCESS_CONTROL registers
//
#define ILI9341_MADCTL_MY  0x80
#define ILI9341_MADCTL_MX  0x40
#define ILI9341_MADCTL_MV  0x20
#define ILI9341_MADCTL_ML  0x10
#define ILI9341_MADCTL_BGR 0x08
#define ILI9341_MADCTL_MH  0x04
#define ILI9341_MADCTL_RGB 0x00

#define DISPLAY_ROTATION_270   (ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR)
#define DISPLAY_ROTATION_90    (ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR)
#define DISPLAY_ROTATION_0     (ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)
#define DISPLAY_ROTATION_180   (ILI9341_MADCTL_MX | ILI9341_MADCTL_MY  \
                              | ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)


#define RESET_ASSERT	;
#define RESET_NEGATE	;
#define CS_LOW			ili9341_spi_set_cs(true)
#define CS_HIGH			ili9341_spi_set_cs(false)
#define DC_CMD			digitalWrite(ili9341_conf_dc, LOW)
#define DC_DATA			digitalWrite(ili9341_conf_dc, HIGH)




uint16_t ili9341_spi_buffers[SPI_BUFFER_SIZE * 2];

static uint16_t* const ili9341_spi_bufferA = ili9341_spi_buffers;
static uint16_t* const ili9341_spi_bufferB = &ili9341_spi_buffers[SPI_BUFFER_SIZE];

uint16_t* ili9341_spi_buffer = ili9341_spi_bufferA;

// Default foreground & background colors
uint16_t foreground_color = 0;
uint16_t background_color = 0;

Pad ili9341_conf_dc;
small_function<void(bool selected)> ili9341_spi_set_cs;
small_function<uint32_t(uint32_t sdi, int bits)> ili9341_spi_transfer;
small_function<void(uint32_t words)> ili9341_spi_transfer_bulk;
small_function<void()> ili9341_spi_wait_bulk;
small_function<void(uint8_t *buf, uint32_t bytes)> ili9341_spi_read;

static inline void ssp_senddata(uint8_t x)
{
  ili9341_spi_transfer(x, 8);
}

static inline uint8_t ssp_sendrecvdata(uint8_t x)
{
	return (uint8_t) ili9341_spi_transfer(x, 8);
}

static inline void ssp_senddata16(uint16_t x)
{
  ili9341_spi_transfer(x, 16);
}

static void send_command(uint8_t cmd, int len, const uint8_t *data)
{
	CS_LOW;
	DC_CMD;
//    delayMicroseconds(1);
	ssp_senddata(cmd);
	DC_DATA;
//    delayMicroseconds(1);
	while (len-- > 0) {
	  ssp_senddata(*data++);
	}
	//CS_HIGH;
}

#ifndef DISPLAY_ST7796
static const uint8_t ili_init_seq[] = {
  // cmd, len, data...,
  // SW reset
  ILI9341_SOFTWARE_RESET, 0,
  // display off
  ILI9341_DISPLAY_OFF, 0,
  // Power control B
  ILI9341_POWERB, 3, 0x00, 0xC1, 0x30,
  // Power on sequence control
  ILI9341_POWER_SEQ, 4, 0x64, 0x03, 0x12, 0x81,
  // Driver timing control A
  ILI9341_DTCA, 3, 0x85, 0x00, 0x78,
  // Power control A
  ILI9341_POWERA, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  // Pump ratio control
  ILI9341_PUMP_RATIO_CONTROL, 1, 0x20,
  // Driver timing control B
  ILI9341_DTCB, 2, 0x00, 0x00,
  // POWER_CONTROL_1
  ILI9341_POWER_CONTROL_1, 1, 0x23,
  // POWER_CONTROL_2
  ILI9341_POWER_CONTROL_2, 1, 0x10,
  // VCOM_CONTROL_1
  ILI9341_VCOM_CONTROL_1, 2, 0x3e, 0x28,
  // VCOM_CONTROL_2
  ILI9341_VCOM_CONTROL_2, 1, 0xBE,
  // MEMORY_ACCESS_CONTROL
  //ILI9341_MEMORY_ACCESS_CONTROL, 1, 0x48, // portlait
  ILI9341_MEMORY_ACCESS_CONTROL, 1, DISPLAY_ROTATION_0, // landscape
  // COLMOD_PIXEL_FORMAT_SET : 16 bit pixel
  ILI9341_PIXEL_FORMAT_SET, 1, 0x55,
  // Frame Rate
  ILI9341_FRAME_RATE_CONTROL_1, 2, 0x00, 0x18,
  // Gamma Function Disable
  ILI9341_3GAMMA_EN, 1, 0x00,
  // gamma set for curve 01/2/04/08
  ILI9341_GAMMA_SET, 1, 0x01,
  // positive gamma correction
  ILI9341_POSITIVE_GAMMA_CORRECTION, 15, 0x0F,  0x31,  0x2B,  0x0C,  0x0E,  0x08,  0x4E,  0xF1,  0x37,  0x07,  0x10,  0x03,  0x0E, 0x09,  0x00,
  // negativ gamma correction
  ILI9341_NEGATIVE_GAMMA_CORRECTION, 15, 0x00,  0x0E,  0x14,  0x03,  0x11,  0x07,  0x31,  0xC1,  0x48,  0x08,  0x0F,  0x0C,  0x31, 0x36,  0x0F,
  // Column Address Set
//ILI9341_COLUMN_ADDRESS_SET, 4, 0x00, 0x00, 0x01, 0x3f, // width 320
  // Page Address Set
//ILI9341_PAGE_ADDRESS_SET, 4, 0x00, 0x00, 0x00, 0xef,   // height 240
  // entry mode
  ILI9341_ENTRY_MODE_SET, 1, 0x06,
  // display function control
  ILI9341_DISPLAY_FUNCTION_CONTROL, 3, 0x08, 0x82, 0x27,
  // Interface Control (set WEMODE=0)
  ILI9341_INTERFACE_CONTROL, 3, 0x00, 0x00, 0x00,
  // sleep out
  ILI9341_SLEEP_OUT, 0,
  // display on
  ILI9341_DISPLAY_ON, 0,
  0 // sentinel
};
#else
static const uint8_t ili_init_seq[] = {
  // SW reset
  ILI9341_SOFTWARE_RESET, 0,
  // display off
  ILI9341_DISPLAY_OFF, 0,

  // Interface Mode Control
  ILI9341_RGB_INTERFACE_CONTROL, 1, 0x00,
  // Frame Rate
  ILI9341_FRAME_RATE_CONTROL_1, 2, 0x50, 0x10,
  // Display Inversion Control , 2 Dot
  ILI9341_DISPLAY_INVERSION_CONTROL, 1, 0x00,
  // RGB/MCU Interface Control
  ILI9341_DISPLAY_FUNCTION_CONTROL, 3, 0x02, 0x02, 0x3B,
  // EntryMode
  ILI9341_ENTRY_MODE_SET, 1, 0xC6,
  // Power Control 1
  ILI9341_POWER_CONTROL_1, 2, 0x17, 0x15,
  // Power Control 2
  ILI9341_POWER_CONTROL_2, 1, 0x41,
  // VCOM Control
//ILI9341_VCOM_CONTROL_1, 3, 0x00, 0x4D, 0x90,
  ILI9341_VCOM_CONTROL_1, 3, 0x00, 0x12, 0x80,
  // Memory Access
  ILI9341_MEMORY_ACCESS_CONTROL, 1, 0x28,  // landscape, BGR
//ILI9341_MEMORY_ACCESS_CONTROL, 1, 0x20,  // landscape, RGB
  // Interface Pixel Format,	16bpp DPI and DBI and
  ILI9341_PIXEL_FORMAT_SET, 1, 0x55,
  // P-Gamma
  ILI9341_POSITIVE_GAMMA_CORRECTION, 15, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F,
  // N-Gamma
  ILI9341_NEGATIVE_GAMMA_CORRECTION, 15, 0x00, 0X16, 0X19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F,
  //Set Image Func
//  0xE9, 1, 0x00,
  // Set Brightness to Max
//  ILI9341_WRITE_BRIGHTNESS, 1, 0xFF,
  // Adjust Control
//  ILI9341_PUMP_RATIO_CONTROL, 4, 0xA9, 0x51, 0x2C, 0x82,
  //Exit Sleep
  ILI9341_SLEEP_OUT, 0x00,
  // display on
  ILI9341_DISPLAY_ON, 0,
  0 // sentinel
};
#endif

void
ili9341_init(void)
{
  DC_DATA;
  RESET_ASSERT;
  delay(10);
  RESET_NEGATE;

  ili9341_spi_wait_bulk();

  const uint8_t *p;
  for (p = ili_init_seq; *p; ) {
	send_command(p[0], p[1], &p[2]);
	p += 2 + p[1];
	delay(5);
  }
}

// Reverses the byte order within each halfword of a word. For example, 0x12345678 becomes 0x34127856.
#if 0
#define __REV16(v) (((((uint32_t)(v) & 0xFF000000) >> 8) | (((uint32_t)(v) & 0x00FF0000) << 8) | (((uint32_t)(v) & 0x0000FF00) >> 8) | (((uint32_t)(v) & 0x0000FF) << 8)))
#else
static inline uint32_t __REV16(uint32_t value)
{
  uint32_t result;
  __asm volatile("rev16 %0, %1" : "=r" (result) : "r" (value));
  return result;
}
#endif

#if 0
void ili9341_pixel(int x, int y, uint16_t color)
{
	uint8_t xx[4] = { x >> 8, x, (x+1) >> 8, (x+1) };
	uint8_t yy[4] = { y >> 8, y, (y+1) >> 8, (y+1) };
	uint8_t cc[2] = { color >> 8, color };
	ili9341_spi_wait_bulk();
	send_command(0x2A, 4, xx);
	send_command(0x2B, 4, yy);
	send_command(0x2C, 2, cc);
	//send_command16(0x2C, color);
}
#endif

void ili9341_fill(int x, int y, int w, int h, uint16_t color)
{
	uint32_t len = w * h;
	uint32_t xx = __REV16(x | ((x + w - 1) << 16));
	uint32_t yy = __REV16(y | ((y + h - 1) << 16));
	ili9341_spi_wait_bulk();
	send_command(ILI9341_COLUMN_ADDRESS_SET, 4, (uint8_t*)&xx);
	send_command(ILI9341_PAGE_ADDRESS_SET, 4, (uint8_t*)&yy);
	send_command(ILI9341_MEMORY_WRITE, 0, NULL);

	constexpr int chunkSize = 512;
	static_assert(chunkSize <= SPI_BUFFER_SIZE);

	uint32_t fill = len > chunkSize ? chunkSize : len;
	for(uint32_t i=0; i< fill; i++)
		ili9341_spi_buffer[i] = color;

	while(len > 0) {
		uint32_t bulk = len > fill ? fill : len;
		ili9341_spi_transfer_bulk(bulk);
		len -= bulk;
	}
	if(ili9341_spi_buffer == ili9341_spi_bufferA)
		ili9341_spi_buffer = ili9341_spi_bufferB;
	else ili9341_spi_buffer = ili9341_spi_bufferA;
}

void ili9341_bulk(int x, int y, int w, int h)
{
	uint32_t len = w * h;
	uint32_t xx = __REV16(x | ((x + w - 1) << 16));
	uint32_t yy = __REV16(y | ((y + h - 1) << 16));
	ili9341_spi_wait_bulk();
	send_command(ILI9341_COLUMN_ADDRESS_SET, 4, (uint8_t*)&xx);
	send_command(ILI9341_PAGE_ADDRESS_SET, 4, (uint8_t*)&yy);
	send_command(ILI9341_MEMORY_WRITE, 0, NULL);

	ili9341_spi_transfer_bulk(len);

	// switch buffers so that the user can continue to render while
	// the bulk transfer is happening.
	if(ili9341_spi_buffer == ili9341_spi_bufferA)
		ili9341_spi_buffer = ili9341_spi_bufferB;
	else ili9341_spi_buffer = ili9341_spi_bufferA;
}

void
ili9341_read_memory(int x, int y, int w, int h, uint16_t *out)
{
	uint32_t xx = __REV16(x | ((x + w - 1) << 16));
	uint32_t yy = __REV16(y | ((y + h - 1) << 16));
	ili9341_spi_wait_bulk();
	send_command(ILI9341_COLUMN_ADDRESS_SET, 4, (uint8_t *)&xx);
	send_command(ILI9341_PAGE_ADDRESS_SET, 4, (uint8_t*)&yy);

	ili9341_spi_wait_bulk();
	send_command(0x2E, 0, NULL);

	int len = w * h;
#ifndef DISPLAY_ST7796
	// require 8bit dummy clock
	ssp_sendrecvdata(0);
	do {
		// read data is always 24bit RGB888
		uint8_t r, g, b;
		r = ssp_sendrecvdata(0);
		g = ssp_sendrecvdata(0);
		b = ssp_sendrecvdata(0);
		*out++ = RGB565(r,g,b);
	} while(--len);
#else
	// require 8bit dummy clock
	ssp_sendrecvdata(0);
	// read data is always 16bit RGB565
	ili9341_spi_read((uint8_t *)out, len * 2);
#endif
	CS_HIGH;
}

void
ili9341_set_flip(bool flipX, bool flipY) {
	ili9341_spi_wait_bulk();
	uint8_t memAcc = ILI9341_MADCTL_BGR | ILI9341_MADCTL_MV;
	if(flipX) memAcc |= ILI9341_MADCTL_MX;
	if(flipY) memAcc |= ILI9341_MADCTL_MY;
	send_command(ILI9341_MEMORY_ACCESS_CONTROL, 1, &memAcc);
}

//********************************************************************
void
ili9341_clear_screen(void)
{
	ili9341_fill(0, 0, LCD_WIDTH, LCD_HEIGHT, background_color);
}

void
ili9341_set_foreground(uint16_t fg)
{
  foreground_color = fg;
}

void
ili9341_set_background(uint16_t bg)
{
  background_color = bg;
}

//static uint8_t bit_align = 0;
void ili9341_blitBitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *b)
{
  uint16_t *buf = ili9341_spi_buffer;
  uint8_t bits = 0;
  for (uint32_t c = 0; c < height; c++) {
    for (uint32_t r = 0; r < width; r++) {
      if ((r&7) == 0) bits = *b++;
      *buf++ = (0x80 & bits) ? foreground_color : background_color;
      bits <<= 1;
    }
//    if (bit_align) b+=bit_align;
  }
  ili9341_bulk(x, y, width, height);
}

void
ili9341_drawchar(uint8_t ch, int x, int y)
{
  ili9341_blitBitmap(x, y, FONT_GET_WIDTH(ch), FONT_GET_HEIGHT, FONT_GET_DATA(ch));
}

void ili9341_drawstring(const char *str, int x, int y)
{
  int x_pos = x;
  while (*str) {
    uint8_t ch = *str++;
    if (ch == '\n') {x = x_pos; y+=FONT_STR_HEIGHT; continue;}
    const uint8_t *char_buf = FONT_GET_DATA(ch);
    uint16_t w = FONT_GET_WIDTH(ch);
    ili9341_blitBitmap(x, y, w, FONT_GET_HEIGHT, char_buf);
    x += w;
  }
}

void
ili9341_drawstring(const char *str, int len, int x, int y)
{
	const char* end = str + len;
	while (str < end) {
		uint8_t ch = *str++;
		const uint8_t *char_buf = FONT_GET_DATA(ch);
		uint16_t w = FONT_GET_WIDTH(ch);
		ili9341_blitBitmap(x, y, w, FONT_GET_HEIGHT, char_buf);
		x += w;
	}
}

int
ili9341_drawchar_size(uint8_t ch, int x, int y, uint8_t size)
{
  uint16_t *buf = ili9341_spi_buffer;
  const uint8_t *char_buf = FONT_GET_DATA(ch);
  uint16_t w = FONT_GET_WIDTH(ch);
  for (int c = 0; c < FONT_GET_HEIGHT; c++, char_buf++) {
    for (int i = 0; i < size; i++) {
      uint8_t bits = *char_buf;
      for (int r = 0; r < w; r++, bits <<= 1)
        for (int j = 0; j < size; j++)
          *buf++ = (0x80 & bits) ? foreground_color : background_color;
    }
  }
  ili9341_bulk(x, y, w * size, FONT_GET_HEIGHT * size);
  return w*size;
}
//********************************************************************

void
ili9341_drawstring_size(const char *str, int x, int y, uint8_t size)
{
  int origX = x;
  while (*str){
    uint8_t c =*str++;
    if(c == '\n'){
        x = origX;
        y += FONT_STR_HEIGHT * size;
    	continue;
    }
    x += ili9341_drawchar_size(c, x, y, size);
  }
}

#define SWAP(x,y) do { int z=x; x = y; y = z; } while(0)

void
ili9341_line(int x0, int y0, int x1, int y1)
{
  if (x0 > x1) {
	SWAP(x0, x1);
	SWAP(y0, y1);
  }

  while (x0 <= x1) {
	int dx = x1 - x0 + 1;
	int dy = y1 - y0;
	if (dy >= 0) {
	  dy++;
	  if (dy > dx) {
		dy /= dx; dx = 1;
	  } else {
		dx /= dy; dy = 1;
	  }
	} else {
	  dy--;
	  if (-dy > dx) {
		dy /= dx; dx = 1;
	  } else {
		dx /= -dy; dy = -1;
	  }
	}
	if (dy > 0)
	  ili9341_fill(x0, y0, dx, dy, foreground_color);
	else
	  ili9341_fill(x0, y0+dy, dx, -dy, foreground_color);
	x0 += dx;
	y0 += dy;
  }
}


void
ili9341_drawfont(uint8_t ch, int x, int y)
{
  ili9341_blitBitmap(x, y, NUM_FONT_GET_WIDTH, NUM_FONT_GET_HEIGHT, NUM_FONT_GET_DATA(ch));
}

#if 0
const uint16_t colormap[] = {
  RGB565(255,0,0), RGB565(0,255,0), RGB565(0,0,255),
  RGB565(255,255,0), RGB565(0,255,255), RGB565(255,0,255)
};

void
ili9341_test(int mode)
{
  int x, y;
  int i;
  switch (mode) {
  default:
#if 1
	ili9341_fill(0, 0, 320, 240, 0);
	for (y = 0; y < 240; y++) {
	  ili9341_fill(0, y, 320, 1, RGB565(y, (y + 120) % 256, 240-y));
	}
	break;
  case 1:
	ili9341_fill(0, 0, 320, 240, 0);
	for (y = 0; y < 240; y++) {
	  for (x = 0; x < 320; x++) {
		ili9341_pixel(x, y, (y<<8)|x);
	  }
	}
	break;
  case 2:
	//send_command16(0x55, 0xff00);
	ili9341_pixel(64, 64, 0xaa55);
	break;
#endif
#if 1
  case 3:
	for (i = 0; i < 10; i++)
	  ili9341_drawfont(i, &NF20x22, i*20, 120, colormap[i%6], 0x0000);
	break;
#endif
#if 0
  case 4:
	draw_grid(10, 8, 29, 29, 15, 0, 0xffff, 0);
	break;
#endif
  case 4:
	ili9341_line(0, 0, 15, 100, 0xffff);
	ili9341_line(0, 0, 100, 100, 0xffff);
	ili9341_line(0, 15, 100, 0, 0xffff);
	ili9341_line(0, 100, 100, 0, 0xffff);
	break;
  }
}
#endif
