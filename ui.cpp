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

#include "common.hpp"
#include "main.hpp"
#include "flash.hpp"
#include "globals.hpp"
#include "ili9341.hpp"
#include "Font.h"
#include "plot.hpp"
#include "ui.hpp"
#include "board.hpp"
#include <stdlib.h>
#include <string.h>
#include <mculib/message_log.hpp>
#include <mculib/printf.hpp>
#include <mculib/printk.hpp>
#include "gitversion.hpp"

using UIHW::UIEvent;
using UIHW::UIEventButtons;
using UIHW::UIEventTypes;
using namespace UIActions;


#define TRUE true
#define FALSE false


int8_t previous_marker = MARKER_INVALID;

enum {
  UI_NORMAL, UI_MENU, UI_KEYPAD, UI_USB_MODE
};

enum {
  KM_START, KM_STOP, KM_CENTER, KM_SPAN, KM_POINTS, KM_CW, KM_SCALE, KM_REFPOS, KM_EDELAY, KM_VELOCITY_FACTOR, KM_SCALEDELAY
};

uint8_t ui_mode = UI_NORMAL;
uint8_t keypad_mode;
int8_t selection = 0;
bool ui_disabled = false;

// Button definition (used in MT_ADV_CALLBACK for custom)
#define BUTTON_ICON_NONE            -1
#define BUTTON_ICON_NOCHECK          0
#define BUTTON_ICON_CHECK            1
#define BUTTON_ICON_GROUP            2
#define BUTTON_ICON_GROUP_CHECKED    3

#define BUTTON_BORDER_NONE           0x00
#define BUTTON_BORDER_WIDTH_MASK     0x0F

// Define mask for draw border (if 1 use light color, if 0 dark)
#define BUTTON_BORDER_TYPE_MASK      0xF0
#define BUTTON_BORDER_TOP            0x10
#define BUTTON_BORDER_BOTTOM         0x20
#define BUTTON_BORDER_LEFT           0x40
#define BUTTON_BORDER_RIGHT          0x80

#define BUTTON_BORDER_FLAT           0x00
#define BUTTON_BORDER_RISE           (BUTTON_BORDER_TOP|BUTTON_BORDER_RIGHT)
#define BUTTON_BORDER_FALLING        (BUTTON_BORDER_BOTTOM|BUTTON_BORDER_LEFT)

typedef struct {
  uint16_t bg;
  uint16_t fg;
  uint8_t  border;
  int8_t   icon;
} button_t;

// Call back functions for MT_CALLBACK type
typedef void (*menuaction_cb_t)(UIEvent evt, int item, uint16_t data);
#define UI_FUNCTION_CALLBACK(ui_function_name) void ui_function_name(UIEvent evt, int item, uint16_t data)

typedef void (*menuaction_acb_t)(UIEvent evt, int item, uint16_t data, button_t *b);
#define UI_FUNCTION_ADV_CALLBACK(ui_function_name) void ui_function_name(UIEvent evt, int item, uint16_t data, button_t *b)

// Set structure align as WORD (save flash memory)
#pragma pack(push, 2)
typedef struct {
  uint8_t type;
  uint8_t data;
  const char *label;
  const void *reference;
} menuitem_t;
#pragma pack(pop)

//int awd_count;
//int touch_x, touch_y;

#define NUMINPUT_LEN 10

#define KP_CONTINUE 0
#define KP_DONE 1
#define KP_CANCEL 2

char kp_buf[11];
int8_t kp_index = 0;

bool uiEventsEnabled = true;
UIEvent lastUIEvent = {};

void ui_mode_menu(void);
void ui_mode_keypad(int _keypad_mode);
void draw_menu(void);
void leave_ui_mode(void);
void erase_menu_buttons(void);
void ui_process_keypad(UIEvent evt);

static void menu_push_submenu(const menuitem_t *submenu);
static int touch_pickup_marker(void);

void
touch_prepare_sense(void)
{
}

// Disable or enable ui_process_* callbacks in order to do synchronous event polling.
// New uses of these functions are heavily discouraged.
// Please use event driven UI processing instead.

void uiEnableProcessing(void) {
  uiEventsEnabled = true;
}
void uiDisableProcessing() {
  uiEventsEnabled = false;
}

// wait for an UI event when UI processing is disabled.
UIEvent uiWaitEvent() {
  while(lastUIEvent.type == UIEventTypes::None)
    application_doSingleEvent();

  UIEvent ret = lastUIEvent;
  lastUIEvent = {};
  return ret;
}



void
touch_cal_exec(void)
{
  uint16_t x1, x2, y1, y2, t;
  UIEvent evt;

  uiDisableProcessing();
  ili9341_set_foreground(DEFAULT_FG_COLOR);
  ili9341_set_background(DEFAULT_BG_COLOR);
  ili9341_clear_screen();
  ili9341_line(0, 0, 0, 32);
  ili9341_line(0, 0, 32, 0);
  ili9341_drawstring("TOUCH UPPER LEFT", 10, 10);

  do {
    evt = uiWaitEvent();
    if(evt.isTouchPress())
      UIHW::touchPosition(x1, y1);
  } while(!evt.isTouchRelease());


  ili9341_clear_screen();
  ili9341_line(LCD_WIDTH-1, LCD_HEIGHT-1, LCD_WIDTH-1, LCD_HEIGHT-32);
  ili9341_line(LCD_WIDTH-1, LCD_HEIGHT-1, LCD_WIDTH-32, LCD_HEIGHT-1);
  ili9341_drawstring("TOUCH LOWER RIGHT", LCD_WIDTH-17*(FONT_WIDTH)-10, LCD_HEIGHT-FONT_GET_HEIGHT-10);

  do {
    evt = uiWaitEvent();
     if(evt.isTouchPress())
      UIHW::touchPosition(x2, y2);
  } while(!evt.isTouchRelease());
  // Need swap data if display flip
  if(config.ui_options & UI_OPTIONS_FLIP){
    t=x1;x1=x2;x2=t;
    t=y1;y1=y2;y2=t;
  }
  config.touch_cal[0] = x1;
  config.touch_cal[1] = y1;
  config.touch_cal[2] = (x2 - x1) * 16 / LCD_WIDTH;
  config.touch_cal[3] = (y2 - y1) * 16 / LCD_HEIGHT;

  UIActions::printTouchCal();

  uiEnableProcessing();
}

void
touch_draw_test(void)
{
  UIEvent evt;
  int x0, y0;
  int x1, y1;

  uiDisableProcessing();
  ili9341_set_foreground(DEFAULT_FG_COLOR);
  ili9341_set_background(DEFAULT_BG_COLOR);
  ili9341_clear_screen();
  ili9341_drawstring("TOUCH TEST: DRAG PANEL", OFFSETX, LCD_HEIGHT - FONT_STR_HEIGHT);

  do {
    evt = uiWaitEvent();
  } while(!evt.isTouchPress());
  touch_position(&x0, &y0);

  while(true) {
    if(!touch_position(&x1, &y1))
      break;
    ili9341_line(x0, y0, x1, y1);
    x0 = x1;
    y0 = y1;
    delay(50);
  }

  uiEnableProcessing();
}


bool touch_position(int *x, int *y)
{
  uint16_t touchX, touchY;
  if(!UIHW::touchPosition(touchX, touchY))
    return false;
  *x = (int(touchX) - config.touch_cal[0]) * 16 / config.touch_cal[2];
  *y = (int(touchY) - config.touch_cal[1]) * 16 / config.touch_cal[3];
  if(config.ui_options & UI_OPTIONS_FLIP) {
    *x = LCD_WIDTH  - *x;
    *y = LCD_HEIGHT - *y;
  }
  return true;
}

bool touch_position(int *x, int *y, UIEvent evt) {
  return touch_position(x, y);
}


void
show_version(void)
{
  int x = 5, y = 5;
  const char *fpu;
  uint32_t* deviceID = (uint32_t*)0x1FFFF7E8;
  char snStr[64];
  chsnprintf(snStr, sizeof(snStr), "SN: %08x-%08x-%08x\n", deviceID[0], deviceID[1], deviceID[2]);

  ili9341_set_foreground(DEFAULT_FG_COLOR);
  ili9341_set_background(DEFAULT_BG_COLOR);
  uiDisableProcessing();
  ili9341_clear_screen();

  ili9341_drawstring_size(BOARD_NAME, x, y, 3);
  y += 3*FONT_GET_HEIGHT + 6;
  ili9341_drawstring_size(snStr, x, y, 2);
  y += 2*FONT_GET_HEIGHT;

  int step = FONT_STR_HEIGHT + 3;
  ili9341_drawstring("Software copyright @edy555 et al", x, y += step);
  ili9341_drawstring("Hardware designed by OwOComm", x, y += step);
  ili9341_drawstring("Licensed under GPL. ", x, y += step);
  ili9341_drawstring("https://github.com/ttrftech/NanoVNA", x + 10, y += step);
  ili9341_drawstring("https://github.com/nanovna/NanoVNA-V2-firmware", x + 10, y += step);
  ili9341_drawstring("Version: " GITVERSION, x, y += step);
  ili9341_drawstring("Build Time: " __DATE__ " - " __TIME__, x, y += step);
  y += 5;
  ili9341_drawstring("Compiler: " PORT_COMPILER_NAME, x, y += step);
  ili9341_drawstring("Port Info: " PORT_INFO, x, y += step);
  ili9341_drawstring("Board: " BOARD_NAME, x, y += step);

  while (true) {
    UIEvent evt = uiWaitEvent();
    if(evt.isTouchPress() || evt.isLeverClick())
      break;
  }

  uiEnableProcessing();
}


void
show_dmesg(void)
{
  int x = 5, y = 5;
  ili9341_set_foreground(DEFAULT_FG_COLOR);
  ili9341_set_background(DEFAULT_BG_COLOR);
  uiDisableProcessing();
  ili9341_clear_screen();

  int maxLines = 23;

  const char* msg = dmesg();
  int len = strlen(msg);
  const char* end = msg + len;

  // leave only last maxLines lines
  const char* pos = end - 1;
  const char* lines[maxLines];
  int nextLineIndex = maxLines - 1;
  while(pos >= msg && pos < end) {
    if((*pos) == '\n') {
      lines[nextLineIndex] = pos + 1;
      nextLineIndex--;
      if(nextLineIndex < 0) break;
    }
    pos--;
  }

  if(nextLineIndex >= 0) {
    lines[nextLineIndex] = msg;
    nextLineIndex--;
  }

  for(int i = nextLineIndex+1; i<maxLines; i++) {
    int len = (i < (maxLines-1)) ? (lines[i+1] - lines[i]) : (end - lines[i]);
    if(len > 0) len--;
    ili9341_drawstring(lines[i], len, x, y);
    y += 10;
  }

  while (true) {
    UIEvent evt = uiWaitEvent();
    if(evt.isTouchPress() || evt.isLeverClick())
      break;
  }

  uiEnableProcessing();
}


void ui_mode_usb(void) {
  int x = 5, y = 5;
  ili9341_set_foreground(DEFAULT_FG_COLOR);
  ili9341_set_background(DEFAULT_BG_COLOR);
  ili9341_clear_screen();

  ili9341_drawstring_size(BOARD_NAME, x, y, 3);
  y += 3 * FONT_GET_HEIGHT + 25;

  ili9341_drawstring_size("USB MODE", x, y, 3);
  ui_mode = UI_USB_MODE;
}


void
show_message(const char* title, const char* message, int fg, int bg)
{
  int x = 5, y = 5;
  ili9341_set_foreground(fg);
  ili9341_set_background(bg);
  ili9341_clear_screen();

  ili9341_drawstring_size(title, x, y, 3);
  y += 3 * FONT_GET_HEIGHT + 25;

  ili9341_drawstring_size(message, x, y, 1);
}

void
ui_enter_bootload(void)
{
  uiDisableProcessing();

  int x = 5, y = 5;
  ili9341_set_foreground(DEFAULT_FG_COLOR);
  ili9341_set_background(DEFAULT_BG_COLOR);
  // leave a last message
  ili9341_clear_screen();
  ili9341_drawstring("BOOTLOAD: Device Firmware Update Mode", x, y += FONT_STR_HEIGHT);
  ili9341_drawstring("To exit Bootload mode, please reset device yourself.", x, y += FONT_STR_HEIGHT);

  enterBootload();
}


// type of menu item
enum {
  MT_NONE,
  MT_BLANK,
  MT_SUBMENU,
  MT_CALLBACK,
  MT_ADV_CALLBACK,
  MT_CANCEL,
  MT_CLOSE
};

static void menu_move_back(bool leave_ui);


static UI_FUNCTION_ADV_CALLBACK(menu_calop_acb)
{
  if (b){
     if ((data == CAL_OPEN  && (cal_status & CALSTAT_OPEN))
      || (data == CAL_SHORT && (cal_status & CALSTAT_SHORT))
      || (data == CAL_LOAD  && (cal_status & CALSTAT_LOAD))
      || (data == CAL_THRU  && (cal_status & CALSTAT_THRU)))
          b->icon = BUTTON_ICON_CHECK;
    return;
  }
  cal_collect(data);
//  selection = item+1;
  ui_disabled = true;
//  draw_cal_status();
//  draw_menu();
}

void ui_cal_collected() {
  ui_disabled = false;
  draw_cal_status();
  draw_menu();
}

extern const menuitem_t menu_save[];

static UI_FUNCTION_CALLBACK(menu_caldone_cb)
{
  (void)item;
  (void)data;
  cal_done();
  draw_cal_status();
  menu_move_back(false);
  menu_push_submenu(menu_save);
}

static UI_FUNCTION_ADV_CALLBACK(menu_cal2_acb)
{
  (void)data;
  if (b){
    if (item == 4) b->icon = (cal_status&CALSTAT_APPLY) ? BUTTON_ICON_CHECK : BUTTON_ICON_NOCHECK;
    if (item == 5) b->icon = (cal_status&CALSTAT_ENHANCED_RESPONSE) ? BUTTON_ICON_CHECK : BUTTON_ICON_NOCHECK;
    return;
  }
  switch (item) {
  case 2: // RESET
    cal_reset();
    break;
  case 3: // RESET ALL
    cal_reset_all();
    break;
  case 4: // CORRECTION
    // toggle applying correction
    if (cal_status)
      cal_status ^= CALSTAT_APPLY;
    break;
  case 5: // ENHANCED RESPONSE
    cal_status ^= CALSTAT_ENHANCED_RESPONSE;
    break;
  }
  draw_menu();
  draw_cal_status();
}

static UI_FUNCTION_CALLBACK(menu_recall_cb)
{
  if (caldata_recall(data) == 0) {
    menu_move_back(true);
    draw_cal_status();
  } else {
    show_dmesg();
    redraw_frame();
    request_to_redraw_grid();
    draw_menu();
  }
}

static UI_FUNCTION_CALLBACK(menu_config_cb)
{
  switch (item) {
  case 0:
      touch_cal_exec();
      redraw_frame();
      request_to_redraw_grid();
      draw_menu();
      uiEnableProcessing();
      break;
  case 1:
      touch_draw_test();
      redraw_frame();
      request_to_redraw_grid();
      draw_menu();
      break;
  case 3:
      show_version();
      redraw_frame();
      request_to_redraw_grid();
      draw_menu();
      break;
  case 4:
      show_dmesg();
      redraw_frame();
      request_to_redraw_grid();
      draw_menu();
      break;
  }
}

static UI_FUNCTION_CALLBACK(menu_config_save_cb)
{
  (void)item;
  (void)data;
  config_save();
  menu_move_back(true);
}

static UI_FUNCTION_CALLBACK(menu_bootload_cb)
{
  (void)item;
  (void)data;
  ui_enter_bootload();
}

static UI_FUNCTION_CALLBACK(menu_save_cb)
{
  (void)item;
  if (caldata_save(data) == 0) {
    menu_move_back(true);
    draw_cal_status();
  } else {
    show_dmesg();
    redraw_frame();
    request_to_redraw_grid();
    draw_menu();
  }
}

static void
choose_active_trace(void)
{
  int i;
  if (trace[uistat.current_trace].enabled)
    // do nothing
    return;
  for (i = 0; i < TRACES_MAX; i++)
    if (trace[i].enabled) {
      uistat.current_trace = i;
      return;
    }
}

static UI_FUNCTION_ADV_CALLBACK(menu_trace_acb)
{
  (void)item;
  if (b){
    if (trace[data].enabled){
      b->bg = config.trace_color[data];
      if (data == selection) b->fg = ~config.trace_color[data];
      if (uistat.current_trace == data) b->icon = BUTTON_ICON_CHECK;
    }
    return;
  }

  if (trace[data].enabled) {
    if (data == uistat.current_trace) {
      // disable if active trace is selected
      trace[data].enabled = FALSE;
      choose_active_trace();
    } else {
      // make active selected trace
      uistat.current_trace = data;
    }
  } else {
    trace[data].enabled = TRUE;
    uistat.current_trace = data;
  }
  request_to_redraw_grid();
  draw_menu();
}

static UI_FUNCTION_ADV_CALLBACK(menu_format_acb)
{
  (void)item;
  if (b){
    if (uistat.current_trace >=0 && trace[uistat.current_trace].type == data)
      b->icon = BUTTON_ICON_CHECK;
    return;
  }
  set_trace_type(uistat.current_trace, data);
  request_to_redraw_grid();
  ui_mode_normal();
  //redraw_all();
}

static UI_FUNCTION_ADV_CALLBACK(menu_channel_acb)
{
  (void)item;
  if (b){
    if (uistat.current_trace >=0 && trace[uistat.current_trace].channel == data)
      b->icon = BUTTON_ICON_CHECK;
    return;
  }
  set_trace_channel(uistat.current_trace, data);
  menu_move_back(true);
}

static UI_FUNCTION_ADV_CALLBACK(menu_transform_window_acb)
{
  (void)item;
  // TODO
  if(b){
    b->icon = (domain_mode & TD_WINDOW) == data ? BUTTON_ICON_GROUP_CHECKED : BUTTON_ICON_GROUP;
    return;
  }
  domain_mode = (domain_mode & ~TD_WINDOW) | data;
  ui_mode_normal();
}

static UI_FUNCTION_ADV_CALLBACK(menu_transform_acb)
{
  (void)item;
  (void)data;
  if(b){
    if (domain_mode & DOMAIN_TIME) b->icon = BUTTON_ICON_CHECK;
    return;
  }
  domain_mode ^= DOMAIN_TIME;
//  uistat.lever_mode = LM_MARKER;
  ui_mode_normal();
}

static UI_FUNCTION_ADV_CALLBACK(menu_transform_filter_acb)
{
  (void)item;
  if(b){
    b->icon = (domain_mode & TD_FUNC) == data ? BUTTON_ICON_GROUP_CHECKED : BUTTON_ICON_GROUP;
    return;
  }
  domain_mode = (domain_mode & ~TD_FUNC) | data;
  ui_mode_normal();
}


static UI_FUNCTION_ADV_CALLBACK(menu_avg_acb)
{
  (void)item;
  if(b) {
    if (current_props._avg == data)
      b->icon = BUTTON_ICON_CHECK;
    return;
  }
  set_averaging(data);
  draw_frequencies();
  ui_mode_normal();
}


static UI_FUNCTION_ADV_CALLBACK(menu_power_acb)
{
  (void)item;
  if(b) {
    if (current_props._adf4350_txPower == data)
      b->icon = BUTTON_ICON_CHECK;
    return;
  }
  set_adf4350_txPower(data);
  ui_mode_normal();
}


static UI_FUNCTION_ADV_CALLBACK(menu_display_acb)
{
  if(b){
    b->icon = config.ui_options & UI_OPTIONS_FLIP ? BUTTON_ICON_CHECK : BUTTON_ICON_NOCHECK;
    return;
  }
  config.ui_options^= UI_OPTIONS_FLIP;
  if(config.ui_options & UI_OPTIONS_FLIP)
    ili9341_set_flip(true, true);

  else ili9341_set_flip(false, false);
  redraw_request |= 0xff;
  force_set_markmap();
  ili9341_set_background(DEFAULT_BG_COLOR);
  ili9341_clear_screen();
  draw_all(true);
  draw_menu();
  plot_cancel();
}

static UI_FUNCTION_CALLBACK(menu_keyboard_cb)
{
  (void)item;
  if (data == KM_SCALE && trace[uistat.current_trace].type == TRC_DELAY) {
    data = KM_SCALEDELAY;
  }
  ui_mode_keypad(data);
}

static UI_FUNCTION_ADV_CALLBACK(measurement_mode)
{
    (void) item;
    enum MeasurementMode mode = (enum MeasurementMode) data;
    enum MeasurementMode cur_mode = (enum MeasurementMode) current_props._measurement_mode;

    if (b){
        b->icon = (mode == cur_mode) ? BUTTON_ICON_GROUP_CHECKED : BUTTON_ICON_GROUP ;
        return;
    }
    // CW work also as checkbox
    if (cur_mode == mode && mode == MEASURE_MODE_REFL_THRU) mode = MEASURE_MODE_FULL;
    set_measurement_mode(mode);
    draw_menu();
}

static UI_FUNCTION_ADV_CALLBACK(menu_pause_acb)
{
  (void)item;
  (void)data;
  if (b){
    b->icon = sweep_enabled ? BUTTON_ICON_NOCHECK : BUTTON_ICON_CHECK;
    return;
  }
  toggle_sweep();
  //menu_move_back(true);
  draw_menu();
}

static freqHz_t
get_marker_frequency(int marker)
{
  if (marker < 0 || marker >= MARKERS_MAX)
    return -1;
  if (!markers[marker].enabled)
    return -1;
  return frequencyAt(markers[marker].index);
}

static UI_FUNCTION_CALLBACK(menu_marker_op_cb)
{
  freqHz_t freq = get_marker_frequency(active_marker);
  if (freq < 0)
    return; // no active marker

  switch (item) {
  case 0: /* MARKER->START */
  case 1: /* MARKER->STOP */
  case 2: /* MARKER->CENTER */
    set_sweep_frequency((SweepParameter)data, freq);
    break;
  case 3: /* MARKERS->SPAN */
    {
      if (previous_marker == MARKER_INVALID || active_marker == previous_marker) {
        // if only 1 marker is active, keep center freq and make span the marker comes to the edge
        freqHz_t center = get_sweep_frequency(ST_CENTER);
        freqHz_t span = center - freq;
       if (span < 0) span = -span;
        set_sweep_frequency(ST_SPAN, span * 2);
      } else {
        // if 2 or more marker active, set start and stop freq to each marker
        freqHz_t freq2 = get_marker_frequency(previous_marker);
        if (freq2 < 0)
          return;
        if (freq > freq2) {
          freq2 = freq;
          freq = get_marker_frequency(previous_marker);
        }
        set_sweep_frequency(ST_START, freq);
        set_sweep_frequency(ST_STOP, freq2);
      }
    }
    break;
  case 4: /* MARKERS->EDELAY */
    {
      if (uistat.current_trace == -1)
        break;
      complexf* array = measured[trace[uistat.current_trace].channel];
      float v = groupdelay_from_array(markers[active_marker].index, array);
      set_electrical_delay(electrical_delay + (v / 1e-12));
    }
    break;
  }
  ui_mode_normal();
  draw_cal_status();
  //redraw_all();
}

#ifdef __USE_LC_MATCHING__
static UI_FUNCTION_ADV_CALLBACK(menu_marker_lc_match_acb)
{
  (void)data;
  if (b){
    b->icon = domain_mode & TD_LC_MATH ? BUTTON_ICON_CHECK : BUTTON_ICON_NOCHECK;
    return;
  }
  domain_mode^=TD_LC_MATH;
  ui_mode_normal();
}
#endif

static UI_FUNCTION_CALLBACK(menu_marker_search_cb)
{
  int i = -1;
  if (active_marker == MARKER_INVALID)
    return;

  switch (item) {
  case 0: /* maximum */
  case 1: /* minimum */
    uistat.marker_search_mode = (item == 0) ? MarkerSearchModes::Max : MarkerSearchModes::Min;
    i = marker_search(uistat.marker_search_mode);
    break;
  case 2: /* search Left */
    i = marker_search_dir(uistat.marker_search_mode, markers[active_marker].index, MK_SEARCH_LEFT);
    break;
  case 3: /* search right */
    i = marker_search_dir(uistat.marker_search_mode, markers[active_marker].index, MK_SEARCH_RIGHT);
    break;
  case 4: /* tracking */
    uistat.marker_tracking = !uistat.marker_tracking;
    break;
  }
  if (i >= 0){
    markers[active_marker].index = i;
    redraw_marker(active_marker);
  }
  draw_menu();
//  uistat.lever_mode = LM_SEARCH;
}

void ui_marker_track() {
  if (uistat.marker_tracking) {
    int i = marker_search(uistat.marker_search_mode);
    if (i != -1 && active_marker != MARKER_INVALID) {
      markers[active_marker].index = i;
      redraw_request |= REDRAW_MARKER;
    }
  }
}

static UI_FUNCTION_ADV_CALLBACK(menu_marker_tracking_acb)
{
  (void)item;
  (void)data;
  if (b){
    b->icon = uistat.marker_tracking ? BUTTON_ICON_CHECK : BUTTON_ICON_NOCHECK;
    return;
  }
  uistat.marker_tracking = !uistat.marker_tracking;
  draw_menu();
}

static UI_FUNCTION_ADV_CALLBACK(menu_marker_smith_acb)
{
  (void)item;
  if (b){
    b->icon = marker_smith_format == data ? BUTTON_ICON_GROUP_CHECKED : BUTTON_ICON_GROUP;
    return;
  }
  marker_smith_format = data;
  redraw_marker(active_marker);
  draw_menu();
}

static void
active_marker_check(void)
{
  int i;
  // Auto select active marker if disabled
  if (active_marker == MARKER_INVALID)
    for (i = 0; i < MARKERS_MAX; i++)
      if (markers[i].enabled) active_marker = i;
  // Auto select previous marker if disabled
  if (previous_marker == active_marker) previous_marker = MARKER_INVALID;
  if (previous_marker == MARKER_INVALID){
    for (i = 0; i < MARKERS_MAX; i++)
      if (markers[i].enabled && i != active_marker) previous_marker = i;
  }
}

#define UI_MARKER_OFF   (MARKERS_MAX  )
#define UI_MARKER_DELTA (MARKERS_MAX+1)
static UI_FUNCTION_ADV_CALLBACK(menu_marker_sel_acb)
{
  int i;
  if (b){
    if (data < MARKERS_MAX && markers[data].enabled) b->icon = BUTTON_ICON_CHECK;
    else if (data == UI_MARKER_DELTA) b->icon = uistat.marker_delta ? BUTTON_ICON_CHECK : BUTTON_ICON_NOCHECK;
    return;
  }
  // Marker select click
  if (data < MARKERS_MAX) {
    int mk = data;
    if (markers[mk].enabled) {            // Marker enabled
      if (mk == active_marker) {          // If active marker:
        markers[mk].enabled = FALSE;      //  disable it
        mk = previous_marker;             //  set select from previous marker
        active_marker = MARKER_INVALID;   //  invalidate active
      }
    } else {
      markers[mk].enabled = TRUE;         // Enable marker
    }
    previous_marker = active_marker;      // set previous marker as current active
    active_marker = mk;                   // set new active marker
    active_marker_check();
  } else if (data == UI_MARKER_OFF) {     // all off
      for (i = 0; i < MARKERS_MAX; i++)
        markers[i].enabled = FALSE;
      previous_marker = MARKER_INVALID;
      active_marker = MARKER_INVALID;
  } else if (data == UI_MARKER_DELTA) { // marker delta
    uistat.marker_delta = !uistat.marker_delta;
  }
  redraw_marker(active_marker);
  draw_menu();
}

static const menuitem_t menu_calop[] = {
  { MT_ADV_CALLBACK, CAL_OPEN,  "OPEN",  (const void *)menu_calop_acb },
  { MT_ADV_CALLBACK, CAL_SHORT, "SHORT", (const void *)menu_calop_acb },
  { MT_ADV_CALLBACK, CAL_LOAD,  "LOAD",  (const void *)menu_calop_acb },
  { MT_ADV_CALLBACK, CAL_THRU,  "THRU",  (const void *)menu_calop_acb },
  { MT_CALLBACK, 0,         "DONE",  (const void *)menu_caldone_cb },
  { MT_CANCEL,   0, S_LARROW" BACK", NULL },
  { MT_NONE,     0, NULL, NULL } // sentinel
};

const menuitem_t menu_save[] = {
  { MT_CALLBACK, 0, "SAVE 0", (const void *)menu_save_cb },
  { MT_CALLBACK, 1, "SAVE 1", (const void *)menu_save_cb },
  { MT_CALLBACK, 2, "SAVE 2", (const void *)menu_save_cb },
  { MT_CALLBACK, 3, "SAVE 3", (const void *)menu_save_cb },
  { MT_CALLBACK, 4, "SAVE 4", (const void *)menu_save_cb },
#if SAVEAREA_MAX > 5
  { MT_CALLBACK, 5, "SAVE 5", (const void *)menu_save_cb },
#endif
#if SAVEAREA_MAX > 6
  { MT_CALLBACK, 6, "SAVE 6", (const void *)menu_save_cb },
#endif
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_cal[] = {
  { MT_SUBMENU,  0, "CALIBRATE", (const void *)menu_calop },
  { MT_SUBMENU,  0, "SAVE",  (const void *)menu_save },
  { MT_ADV_CALLBACK, 0, "RESET", (const void *)menu_cal2_acb },
  { MT_ADV_CALLBACK, 0, "RESET\nALL", (const void *)menu_cal2_acb },
  { MT_ADV_CALLBACK, 0, "APPLY", (const void *)menu_cal2_acb },
  { MT_ADV_CALLBACK, 0, "ENHANCED\nRESPONSE", (const void *)menu_cal2_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_trace[] = {
  { MT_ADV_CALLBACK, 0, "TRACE 0", (const void *)menu_trace_acb },
  { MT_ADV_CALLBACK, 1, "TRACE 1", (const void *)menu_trace_acb },
  { MT_ADV_CALLBACK, 2, "TRACE 2", (const void *)menu_trace_acb },
  { MT_ADV_CALLBACK, 3, "TRACE 3", (const void *)menu_trace_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_format2[] = {
  { MT_ADV_CALLBACK, TRC_POLAR, "POLAR", (const void *)menu_format_acb },
  { MT_ADV_CALLBACK, TRC_LINEAR, "LINEAR", (const void *)menu_format_acb },
  { MT_ADV_CALLBACK, TRC_REAL, "REAL", (const void *)menu_format_acb },
  { MT_ADV_CALLBACK, TRC_IMAG, "IMAG", (const void *)menu_format_acb },
  { MT_ADV_CALLBACK, TRC_R, "RESISTANCE", (const void *)menu_format_acb },
  { MT_ADV_CALLBACK, TRC_X, "REACTANCE", (const void *)menu_format_acb },
  { MT_ADV_CALLBACK, TRC_Q, "Q FACTOR", (const void *)menu_format_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_format[] = {
  { MT_ADV_CALLBACK, TRC_LOGMAG, "LOGMAG", (const void *)menu_format_acb },
  { MT_ADV_CALLBACK, TRC_PHASE, "PHASE", (const void *)menu_format_acb },
  { MT_ADV_CALLBACK, TRC_DELAY, "DELAY", (const void *)menu_format_acb },
  { MT_ADV_CALLBACK, TRC_SMITH, "SMITH", (const void *)menu_format_acb },
  { MT_ADV_CALLBACK, TRC_SWR, "SWR", (const void *)menu_format_acb },
  { MT_SUBMENU, 0, S_RARROW" MORE", (const void *)menu_format2 },
  //{ MT_CALLBACK, TRC_LINEAR, "LINEAR", (const void *)menu_format_cb },
  //{ MT_CALLBACK, TRC_SWR, "SWR", (const void *)menu_format_cb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_scale[] = {
  { MT_CALLBACK, KM_SCALE, "SCALE/DIV", (const void *)menu_keyboard_cb },
  { MT_CALLBACK, KM_REFPOS, "REFERENCE\nPOSITION", (const void *)menu_keyboard_cb },
  { MT_CALLBACK, KM_EDELAY, "ELECTRICAL\nDELAY", (const void *)menu_keyboard_cb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};


const menuitem_t menu_channel[] = {
  { MT_ADV_CALLBACK, 0, "CH0\nREFLECT", (const void *)menu_channel_acb },
  { MT_ADV_CALLBACK, 1, "CH1\nTHROUGH", (const void *)menu_channel_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_transform_window[] = {
  { MT_ADV_CALLBACK, TD_WINDOW_MINIMUM, "MINIMUM", (const void *)menu_transform_window_acb },
  { MT_ADV_CALLBACK, TD_WINDOW_NORMAL,   "NORMAL", (const void *)menu_transform_window_acb },
  { MT_ADV_CALLBACK, TD_WINDOW_MAXIMUM, "MAXIMUM", (const void *)menu_transform_window_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_transform[] = {
  { MT_ADV_CALLBACK, 0, "TRANSFORM\nON", (const void *)menu_transform_acb },
  { MT_ADV_CALLBACK, TD_FUNC_LOWPASS_IMPULSE, "LOW PASS\nIMPULSE", (const void *)menu_transform_filter_acb },
  { MT_ADV_CALLBACK, TD_FUNC_LOWPASS_STEP, "LOW PASS\nSTEP", (const void *)menu_transform_filter_acb },
  { MT_ADV_CALLBACK, TD_FUNC_BANDPASS, "BANDPASS", (const void *)menu_transform_filter_acb },
  { MT_SUBMENU, 0, "WINDOW", (const void *)menu_transform_window },
  { MT_CALLBACK, KM_VELOCITY_FACTOR, "VELOCITY\nFACTOR", (const void *)menu_keyboard_cb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};


const menuitem_t menu_avg[] = {
  { MT_ADV_CALLBACK, 1, "NONE", (const void *)menu_avg_acb },
  { MT_ADV_CALLBACK, 2, "2x", (const void *)menu_avg_acb },
  { MT_ADV_CALLBACK, 5, "5x", (const void *)menu_avg_acb },
  { MT_ADV_CALLBACK, 10, "10x", (const void *)menu_avg_acb },
  { MT_ADV_CALLBACK, 20, "20x", (const void *)menu_avg_acb },
  { MT_ADV_CALLBACK, 40, "40x", (const void *)menu_avg_acb },
//  { MT_ADV_CALLBACK, 60, "60x", (const void *)menu_avg_acb },
  { MT_ADV_CALLBACK, 80, "80x", (const void *)menu_avg_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_display[] = {
  { MT_SUBMENU, 0, "TRACE", (const void *)menu_trace },
  { MT_SUBMENU, 0, "FORMAT", (const void *)menu_format },
  { MT_SUBMENU, 0, "SCALE", (const void *)menu_scale },
  { MT_SUBMENU, 0, "CHANNEL", (const void *)menu_channel },
  { MT_SUBMENU, 0, "AVG", (const void *)menu_avg },
  { MT_SUBMENU, 0, "TRANSFORM", (const void *)menu_transform },
  { MT_ADV_CALLBACK, 0, "FLIP\nDISPLAY", (const void *)menu_display_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};


const menuitem_t menu_power[] = {
  { MT_ADV_CALLBACK, 0, "0", (const void *)menu_power_acb },
  { MT_ADV_CALLBACK, 1, "1", (const void *)menu_power_acb },
  { MT_ADV_CALLBACK, 2, "2", (const void *)menu_power_acb },
  { MT_ADV_CALLBACK, 3, "3", (const void *)menu_power_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

static const menuitem_t menu_sweep_config[] = {
  { MT_CALLBACK, KM_POINTS, "SWEEP\nPOINTS", (const void *)menu_keyboard_cb },
  { MT_ADV_CALLBACK, (uint8_t)MEASURE_MODE_REFL_THRU, "CW", (const void *)measurement_mode },
#ifndef BOARD_DISABLE_ECAL
  { MT_ADV_CALLBACK, (uint8_t)MEASURE_MODE_REFL_THRU_REFRENCE, "No ECAL", (const void *)measurement_mode  },
  { MT_ADV_CALLBACK, (uint8_t)MEASURE_MODE_FULL, "ECAL", (const void *)measurement_mode  },
#endif
  { MT_SUBMENU,  0, "ADF4350\nTX POWER", (const void *)menu_power },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_stimulus[] = {
  { MT_CALLBACK, KM_START, "START", (const void *)menu_keyboard_cb },
  { MT_CALLBACK, KM_STOP, "STOP",   (const void *)menu_keyboard_cb },
  { MT_CALLBACK, KM_CENTER, "CENTER", (const void *)menu_keyboard_cb },
  { MT_CALLBACK, KM_SPAN, "SPAN",  (const void *)menu_keyboard_cb },
  { MT_CALLBACK, KM_CW, "CW FREQ", (const void *)menu_keyboard_cb },
  { MT_SUBMENU, 0, "CFG SWEEP", (const void *)menu_sweep_config },
//  { MT_ADV_CALLBACK, 0, "PAUSE\nSWEEP", (const void *)menu_pause_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_marker_sel[] = {
  { MT_ADV_CALLBACK, 0, "MARKER 1", (const void *)menu_marker_sel_acb },
  { MT_ADV_CALLBACK, 1, "MARKER 2", (const void *)menu_marker_sel_acb },
  { MT_ADV_CALLBACK, 2, "MARKER 3", (const void *)menu_marker_sel_acb },
  { MT_ADV_CALLBACK, 3, "MARKER 4", (const void *)menu_marker_sel_acb },
  { MT_ADV_CALLBACK, UI_MARKER_OFF, "ALL OFF", (const void *)menu_marker_sel_acb },
  { MT_ADV_CALLBACK, UI_MARKER_DELTA, "DELTA", (const void *)menu_marker_sel_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_marker_ops[] = {
  { MT_CALLBACK, ST_START, S_RARROW"START", (const void *)menu_marker_op_cb },
  { MT_CALLBACK, ST_STOP, S_RARROW"STOP", (const void *)menu_marker_op_cb },
  { MT_CALLBACK, ST_CENTER, S_RARROW"CENTER", (const void *)menu_marker_op_cb },
  { MT_CALLBACK, ST_SPAN, S_RARROW"SPAN", (const void *)menu_marker_op_cb },
  { MT_CALLBACK, 0, S_RARROW"EDELAY", (const void *)menu_marker_op_cb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_marker_search[] = {
  //{ MT_CALLBACK, "OFF", menu_marker_search_cb },
  { MT_CALLBACK, 0, "MAXIMUM", (const void *)menu_marker_search_cb },
  { MT_CALLBACK, 0, "MINIMUM", (const void *)menu_marker_search_cb },
  { MT_CALLBACK, 0, "SEARCH\n" S_LARROW" LEFT", (const void *)menu_marker_search_cb },
  { MT_CALLBACK, 0, "SEARCH\n" S_RARROW" RIGHT", (const void *)menu_marker_search_cb },
  { MT_ADV_CALLBACK, 0, "TRACKING", (const void *)menu_marker_tracking_acb },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_marker_smith[] = {
  { MT_ADV_CALLBACK, MS_LIN, "LIN", (const void *)menu_marker_smith_acb },
  { MT_ADV_CALLBACK, MS_LOG, "LOG", (const void *)menu_marker_smith_acb },
  { MT_ADV_CALLBACK, MS_REIM,"Re+Im", (const void *)menu_marker_smith_acb },
  { MT_ADV_CALLBACK, MS_RX,  "R+Xj", (const void *)menu_marker_smith_acb },
  { MT_ADV_CALLBACK, MS_RLC, "R+L/C", (const void *)menu_marker_smith_acb },
#ifdef __USE_LC_MATCHING__
  { MT_ADV_CALLBACK,      0, "L/C MATCH", (const void *)menu_marker_lc_match_acb },
#endif
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_marker[] = {
  { MT_SUBMENU, 0, "SELECT\nMARKER", (const void *)menu_marker_sel },
  { MT_SUBMENU, 0, "SEARCH", (const void *)menu_marker_search },
  { MT_SUBMENU, 0, "OPERATIONS", (const void *)menu_marker_ops },
  { MT_SUBMENU, 0, "SMITH\nVALUE", (const void *)menu_marker_smith },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_recall[] = {
  { MT_CALLBACK, 0, "RECALL 0", (const void *)menu_recall_cb },
  { MT_CALLBACK, 1, "RECALL 1", (const void *)menu_recall_cb },
  { MT_CALLBACK, 2, "RECALL 2", (const void *)menu_recall_cb },
  { MT_CALLBACK, 3, "RECALL 3", (const void *)menu_recall_cb },
  { MT_CALLBACK, 4, "RECALL 4", (const void *)menu_recall_cb },
#if SAVEAREA_MAX > 5
  { MT_CALLBACK, 5, "RECALL 5", (const void *)menu_recall_cb },
#endif
#if SAVEAREA_MAX > 6
  { MT_CALLBACK, 6, "RECALL 6", (const void *)menu_recall_cb },
#endif
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_bootload[] = {
  { MT_CALLBACK, 0, "RESET AND\nENTER", (const void *)menu_bootload_cb },
  { MT_CANCEL, 0, S_LARROW"CANCEL", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_config[] = {
  { MT_CALLBACK, 0, "TOUCH CAL", (const void *)menu_config_cb },
  { MT_CALLBACK, 0, "TOUCH TEST", (const void *)menu_config_cb },
  { MT_CALLBACK, 0, "SAVE", (const void *)menu_config_save_cb },
  { MT_CALLBACK, 0, "VERSION", (const void *)menu_config_cb },
  { MT_CALLBACK, 0, "DMESG", (const void *)menu_config_cb },
  { MT_SUBMENU, 0, S_RARROW"BOOTLOAD", (const void *)menu_bootload },
  { MT_CANCEL, 0, S_LARROW" BACK", NULL },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

const menuitem_t menu_top[] = {
  { MT_SUBMENU, 0, "DISPLAY", (const void *)menu_display },
  { MT_SUBMENU, 0, "MARKER", (const void *)menu_marker },
  { MT_SUBMENU, 0, "STIMULUS", (const void *)menu_stimulus },
  { MT_SUBMENU, 0, "CALIBRATE", (const void *)menu_cal },
  { MT_SUBMENU, 0, "RECALL", (const void *)menu_recall },
  { MT_SUBMENU, 0, "CONFIG", (const void *)menu_config },
  { MT_ADV_CALLBACK, 0, "PAUSE\nSWEEP", (const void *)menu_pause_acb },
  { MT_NONE, 0, NULL, NULL } // sentinel
};

#define MENU_STACK_DEPTH_MAX 4
uint8_t menu_current_level = 0;
const menuitem_t *menu_stack[4] = {
  menu_top, NULL, NULL, NULL
};

static void
ensure_selection(void)
{
  const menuitem_t *menu = menu_stack[menu_current_level];
  int i;
  for (i = 0; menu[i].type != MT_NONE; i++)
    ;
  if (selection >= i)
    selection = i-1;
}

static void
menu_move_back(bool leave_ui)
{
  if (menu_current_level == 0)
    return;
  menu_current_level--;
  ensure_selection();
  erase_menu_buttons();
  if (leave_ui)
    ui_mode_normal();
  else
    draw_menu();
}

static void menu_push_submenu(const menuitem_t *submenu)
{
  if (menu_current_level < MENU_STACK_DEPTH_MAX-1)
    menu_current_level++;
  menu_stack[menu_current_level] = submenu;
  ensure_selection();
  erase_menu_buttons();
  draw_menu();
}

/*
static void menu_move_top(void)
{
  if (menu_current_level == 0)
    return;
  menu_current_level = 0;
  ensure_selection();
  erase_menu_buttons();
  draw_menu();
}
*/

void menu_invoke(UIEvent evt, int item)
{
  const menuitem_t *menu = menu_stack[menu_current_level];
  menu = &menu[item];

  switch (menu->type) {
  case MT_NONE:
  case MT_BLANK:
  case MT_CLOSE:
    ui_mode_normal();
    break;

  case MT_CANCEL:
    menu_move_back(false);
    break;

  case MT_CALLBACK: {
    menuaction_cb_t cb = (menuaction_cb_t)menu->reference;
    if (cb) (*cb)(evt, item, menu->data);
    break;
  }
  case MT_ADV_CALLBACK: {
    menuaction_acb_t cb = (menuaction_acb_t)menu->reference;
    if (cb) (*cb)(evt, item, menu->data, NULL);
    break;
  }

  case MT_SUBMENU:
    menu_push_submenu((const menuitem_t*)menu->reference);
    break;
  }
}

// Key names
#define KP_0          0
#define KP_1          1
#define KP_2          2
#define KP_3          3
#define KP_4          4
#define KP_5          5
#define KP_6          6
#define KP_7          7
#define KP_8          8
#define KP_9          9
#define KP_PERIOD    10
#define KP_MINUS     11
#define KP_X1        12
#define KP_K         13
#define KP_M         14
#define KP_G         15
#define KP_BS        16
#define KP_INF       17
#define KP_DB        18
#define KP_PLUSMINUS 19
#define KP_KEYPAD    20
#define KP_N         21
#define KP_P         22
// Stop
#define KP_NONE      -1

typedef struct {
  uint8_t x:4;
  uint8_t y:4;
  int8_t  c;
} keypads_t;

static const keypads_t *keypads;

uint8_t keypads_last_index;

static const keypads_t keypads_freq[] = {
  { 1, 3, KP_PERIOD },
  { 0, 3, KP_0 },
  { 0, 2, KP_1 },
  { 1, 2, KP_2 },
  { 2, 2, KP_3 },
  { 0, 1, KP_4 },
  { 1, 1, KP_5 },
  { 2, 1, KP_6 },
  { 0, 0, KP_7 },
  { 1, 0, KP_8 },
  { 2, 0, KP_9 },
  { 3, 0, KP_G },
  { 3, 1, KP_M },
  { 3, 2, KP_K },
  { 3, 3, KP_X1 },
  { 2, 3, KP_BS },
  { 0, 0, KP_NONE }
};

static const keypads_t keypads_scale[] = {
  { 1, 3, KP_PERIOD },
  { 0, 3, KP_0 },
  { 0, 2, KP_1 },
  { 1, 2, KP_2 },
  { 2, 2, KP_3 },
  { 0, 1, KP_4 },
  { 1, 1, KP_5 },
  { 2, 1, KP_6 },
  { 0, 0, KP_7 },
  { 1, 0, KP_8 },
  { 2, 0, KP_9 },
  { 3, 3, KP_X1 },
  { 2, 3, KP_BS },
  { 0, 0, KP_NONE }
};

static const keypads_t keypads_time[] = {
  { 1, 3, KP_PERIOD },
  { 0, 3, KP_0 },
  { 0, 2, KP_1 },
  { 1, 2, KP_2 },
  { 2, 2, KP_3 },
  { 0, 1, KP_4 },
  { 1, 1, KP_5 },
  { 2, 1, KP_6 },
  { 0, 0, KP_7 },
  { 1, 0, KP_8 },
  { 2, 0, KP_9 },
  { 3, 1, KP_N },
  { 3, 2, KP_P },
  { 3, 3, KP_MINUS },
  { 2, 3, KP_BS },
  { 0, 0, KP_NONE }
};

static const keypads_t * const keypads_mode_tbl[] = {
  keypads_freq, // start
  keypads_freq, // stop
  keypads_freq, // center
  keypads_freq, // span
  keypads_scale, // sweep points
  keypads_freq, // cw freq
  keypads_scale, // scale
  keypads_scale, // refpos
  keypads_time, // electrical delay
  keypads_scale, // velocity factor
  keypads_time // scale of delay
};

const char * const keypad_mode_label[] = {
  "START", "STOP", "CENTER", "SPAN", "POINTS", "CW FREQ", "SCALE", "REFPOS", "EDELAY", "VELOCITY%", "DELAY"
};

static void
draw_button(uint16_t x, uint16_t y, uint16_t w, uint16_t h, button_t *b)
{
  uint16_t bw = b->border&BUTTON_BORDER_WIDTH_MASK;
  ili9341_fill(x + bw, y + bw, w - (bw * 2), h - (bw * 2), b->bg);
  if (bw==0) return;
  uint16_t br = DEFAULT_RISE_EDGE_COLOR;
  uint16_t bd = DEFAULT_FALLEN_EDGE_COLOR;
  uint16_t type = b->border;
  ili9341_fill(x,          y,           w, bw, type&BUTTON_BORDER_TOP    ? br : bd); // top
  ili9341_fill(x + w - bw, y,          bw,  h, type&BUTTON_BORDER_RIGHT  ? br : bd); // right
  ili9341_fill(x,          y,          bw,  h, type&BUTTON_BORDER_LEFT   ? br : bd); // left
  ili9341_fill(x,          y + h - bw,  w, bw, type&BUTTON_BORDER_BOTTOM ? br : bd); // bottom
}

static void
draw_keypad(void)
{
  int i = 0;
  button_t button;
  button.fg = DEFAULT_MENU_TEXT_COLOR;
  while (keypads[i].c != KP_NONE) {
    button.bg = config.menu_normal_color;
    if (i == selection){
      button.bg = config.menu_active_color;
      button.border = KEYBOARD_BUTTON_BORDER|BUTTON_BORDER_FALLING;
    }
    else
      button.border = KEYBOARD_BUTTON_BORDER|BUTTON_BORDER_RISE;
    ili9341_set_foreground(button.fg);
    ili9341_set_background(button.bg);
    int x = KP_GET_X(keypads[i].x);
    int y = KP_GET_Y(keypads[i].y);
    draw_button(x, y, KP_WIDTH, KP_HEIGHT, &button);
    ili9341_drawfont(keypads[i].c,
                     x + (KP_WIDTH - NUM_FONT_GET_WIDTH) / 2,
                     y + (KP_HEIGHT - NUM_FONT_GET_HEIGHT) / 2);
    i++;
  }
}

void
draw_numeric_area_frame(void)
{
  ili9341_fill(0, LCD_HEIGHT-NUM_INPUT_HEIGHT, LCD_WIDTH, NUM_INPUT_HEIGHT, DEFAULT_FG_COLOR);
  ili9341_set_foreground(DEFAULT_MENU_TEXT_COLOR);
  ili9341_set_background(DEFAULT_FG_COLOR);
  ili9341_drawstring(keypad_mode_label[keypad_mode], 10, LCD_HEIGHT-(FONT_GET_HEIGHT+NUM_INPUT_HEIGHT)/2);
//  ili9341_drawfont(KP_KEYPAD, 300, 216);
}

void
draw_numeric_input(const char *buf)
{
  int i;
  int x;
  int focused = FALSE;
  uint16_t xsim = 0b0010010000000000;

  for (i = 0, x = 10 + 10 * FONT_WIDTH + 4; i < 10 && buf[i]; i++, xsim<<=1) {
    uint16_t fg = DEFAULT_MENU_TEXT_COLOR;
    uint16_t bg = DEFAULT_FG_COLOR;
    int c = buf[i];
    if (c == '.')
      c = KP_PERIOD;
    else if (c == '-')
      c = KP_MINUS;
    else// if (c >= '0' && c <= '9')
      c = c - '0';
    ili9341_set_foreground(fg);
    ili9341_set_background(bg);
    if (c < 0 && focused) c = 0;
    if (c >= 0) // c is number
      ili9341_drawfont(c, x, LCD_HEIGHT-NUM_INPUT_HEIGHT+4);
    else        // erase
      ili9341_fill(x, LCD_HEIGHT-NUM_INPUT_HEIGHT+4, NUM_FONT_GET_HEIGHT, NUM_FONT_GET_WIDTH+2+8, bg);

    x += xsim&0x8000 ? NUM_FONT_GET_WIDTH+2+8 : NUM_FONT_GET_WIDTH+2;
  }
  // erase last
  ili9341_fill(x, LCD_HEIGHT-NUM_INPUT_HEIGHT+4, NUM_FONT_GET_WIDTH+2+8, NUM_FONT_GET_WIDTH+2+8, DEFAULT_FG_COLOR);
}

static int
menu_is_multiline(const char *label)
{
  int n = 1;
  while (*label)
    if (*label++ == '\n')
      n++;
  return n;
}

#if 0
static void
menu_item_modify_attribute(const menuitem_t *menu, int item,
                           uint16_t *fg, uint16_t *bg)
{
  if (menu == menu_trace && item < TRACES_MAX) {
    if (trace[item].enabled)
      *bg = config.trace_color[item];
  } else if (menu == menu_marker_sel) {
    if (item < MARKERS_MAX) {
      if (markers[item].enabled) {
        *bg = 0x0000;
        *fg = 0xffff;
      }
    } else if (item == 5) {
      if (uistat.marker_delta) {
        *bg = 0x0000;
        *fg = 0xffff;
      }
    }
  } else if (menu == menu_marker_search) {
    if (item == 4 && uistat.marker_tracking) {
      *bg = 0x0000;
      *fg = 0xffff;
    }
  } else if (menu == menu_marker_smith) {
    if (marker_smith_format == item) {
      *bg = 0x0000;
      *fg = 0xffff;
    }
  } else if (menu == menu_calop) {
    if ((item == 0 && (cal_status & CALSTAT_OPEN))
        || (item == 1 && (cal_status & CALSTAT_SHORT))
        || (item == 2 && (cal_status & CALSTAT_LOAD))
        || (item == 3 && (cal_status & CALSTAT_THRU))) {
      domain_mode = (domain_mode & ~DOMAIN_MODE) | DOMAIN_FREQ;
      *bg = 0x0000;
      *fg = 0xffff;
    }
  } else if (menu == menu_top) {
    if (item == 6 /* PAUSE */ && !sweep_enabled) {
      *bg = 0x0000;
      *fg = 0xffff;
    }
  } else if (menu == menu_cal) {
    if (item == 3 /* CORRECTION */ && (cal_status & CALSTAT_APPLY)) {
      *bg = 0x0000;
      *fg = 0xffff;
    }
  } else if (menu == menu_transform) {
      if ((item == 0 && (domain_mode & DOMAIN_MODE) == DOMAIN_TIME)
       || (item == 1 && (domain_mode & TD_FUNC) == TD_FUNC_LOWPASS_IMPULSE)
       || (item == 2 && (domain_mode & TD_FUNC) == TD_FUNC_LOWPASS_STEP)
       || (item == 3 && (domain_mode & TD_FUNC) == TD_FUNC_BANDPASS)
       ) {
        *bg = 0x0000;
        *fg = 0xffff;
      }
  } else if (menu == menu_transform_window) {
      if ((item == 0 && (domain_mode & TD_WINDOW) == TD_WINDOW_MINIMUM)
       || (item == 1 && (domain_mode & TD_WINDOW) == TD_WINDOW_NORMAL)
       || (item == 2 && (domain_mode & TD_WINDOW) == TD_WINDOW_MAXIMUM)
       ) {
        *bg = 0x0000;
        *fg = 0xffff;
      }
  }
  if(*bg == 0x0000 && ui_disabled)
    *bg = 0x6666;
  if(*fg == 0x0000 && ui_disabled)
    *fg = 0x6666;
}
#endif

#define ICON_WIDTH        16
#define ICON_HEIGHT       11
static const uint8_t check_box[] = {
  _BMP16(0b0011111111110000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0011111111110000),

  _BMP16(0b0011111111110000),
  _BMP16(0b0010000000001000),
  _BMP16(0b0010000000011000),
  _BMP16(0b0010000000110000),
  _BMP16(0b0010000001100000),
  _BMP16(0b0010100011010000),
  _BMP16(0b0010110110010000),
  _BMP16(0b0010011100010000),
  _BMP16(0b0010001000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0011111111110000),

  _BMP16(0b0000000000000000),
  _BMP16(0b0000011110000000),
  _BMP16(0b0000100001000000),
  _BMP16(0b0001000000100000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0010000000010000),
  _BMP16(0b0001000000100000),
  _BMP16(0b0000100001000000),
  _BMP16(0b0000011110000000),

  _BMP16(0b0000000000000000),
  _BMP16(0b0000011110000000),
  _BMP16(0b0000100001000000),
  _BMP16(0b0001001100100000),
  _BMP16(0b0010011110010000),
  _BMP16(0b0010111111010000),
  _BMP16(0b0010111111010000),
  _BMP16(0b0010011110010000),
  _BMP16(0b0001001100100000),
  _BMP16(0b0000100001000000),
  _BMP16(0b0000011110000000),
};

static void
draw_menu_buttons(const menuitem_t *menu)
{
  int i = 0, y = 0;
  for (i = 0; i < MENU_BUTTON_MAX; i++, y+=MENU_BUTTON_HEIGHT) {
    if (menu[i].type == MT_NONE)
      break;
    if (menu[i].type == MT_BLANK)
      continue;

    button_t button;
    button.bg = config.menu_normal_color;
    button.fg = DEFAULT_MENU_TEXT_COLOR;
    button.icon = BUTTON_ICON_NONE;
    // focus only in MENU mode but not in KEYPAD mode
    if (ui_mode == UI_MENU && i == selection){
      button.bg = config.menu_active_color;
      button.border = MENU_BUTTON_BORDER|BUTTON_BORDER_FALLING;
    }
    else
      button.border = MENU_BUTTON_BORDER|BUTTON_BORDER_RISE;

    if (menu[i].type == MT_ADV_CALLBACK){
      menuaction_acb_t cb = (menuaction_acb_t)menu[i].reference;
      if (cb) (*cb)({}, i, menu[i].data, &button);
    }
    draw_button(LCD_WIDTH-MENU_BUTTON_WIDTH, y, MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT, &button);

    ili9341_set_foreground(button.fg);
    ili9341_set_background(button.bg);
    uint16_t text_offs = LCD_WIDTH-MENU_BUTTON_WIDTH+MENU_BUTTON_BORDER + 5;


    if (button.icon >=0){
      ili9341_blitBitmap(LCD_WIDTH-MENU_BUTTON_WIDTH+MENU_BUTTON_BORDER + 1, y+(MENU_BUTTON_HEIGHT-ICON_HEIGHT)/2, ICON_WIDTH, ICON_HEIGHT, &check_box[button.icon*2*ICON_HEIGHT]);
      text_offs=LCD_WIDTH-MENU_BUTTON_WIDTH+MENU_BUTTON_BORDER+1+ICON_WIDTH;
    }
    int lines = menu_is_multiline(menu[i].label);
    ili9341_drawstring(menu[i].label, text_offs, y+(MENU_BUTTON_HEIGHT-lines*FONT_GET_HEIGHT)/2);
  }
  for (; i < MENU_BUTTON_MAX; i++, y+=MENU_BUTTON_HEIGHT) {
    ili9341_fill(LCD_WIDTH-MENU_BUTTON_WIDTH, y, MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT, DEFAULT_BG_COLOR);
  }
}

void
menu_select_touch(UIEvent evt, int i)
{
  selection = i;
  draw_menu();
  uiDisableProcessing();
  while(uiWaitEvent().type != UIEventTypes::Up);
  uiEnableProcessing();
  selection = -1;
  menu_invoke(evt, i);
}

void
menu_apply_touch(UIEvent evt)
{
  int touch_x, touch_y;
  const menuitem_t *menu = menu_stack[menu_current_level];
  int i;

  if(!touch_position(&touch_x, &touch_y, evt))
    return;
  for (i = 0; i < MENU_BUTTON_MAX; i++) {
    if (menu[i].type == MT_NONE)
      break;
    if (menu[i].type == MT_BLANK)
      continue;
    int y = MENU_BUTTON_HEIGHT*i;
    if (y < touch_y && touch_y < y+MENU_BUTTON_HEIGHT && LCD_WIDTH-MENU_BUTTON_WIDTH < touch_x) {
      menu_select_touch(evt, i);
      return;
    }
  }

  ui_mode_normal();
}

void
draw_menu(void)
{
  draw_menu_buttons(menu_stack[menu_current_level]);
}

void
erase_menu_buttons(void)
{
//  ili9341_fill(LCD_WIDTH-MENU_BUTTON_WIDTH, 0, MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT*MENU_BUTTON_MAX, DEFAULT_BG_COLOR);
}

void
erase_numeric_input(void)
{
  ili9341_fill(0, LCD_HEIGHT-NUM_INPUT_HEIGHT, LCD_WIDTH, NUM_INPUT_HEIGHT, DEFAULT_BG_COLOR);
}

void
leave_ui_mode()
{
  if (ui_mode == UI_MENU) {
    request_to_draw_cells_behind_menu();
    erase_menu_buttons();
  }
  draw_frequencies();
}

void set_numeric_value(void)
{
  switch (keypad_mode) {
  case KM_START:
    set_sweep_frequency(ST_START, uistat.value);
    break;
  case KM_STOP:
    set_sweep_frequency(ST_STOP, uistat.value);
    break;
  case KM_CENTER:
    set_sweep_frequency(ST_CENTER, uistat.value);
    break;
  case KM_SPAN:
    set_sweep_frequency(ST_SPAN, uistat.value);
    break;
  case KM_POINTS:
    set_sweep_points(uistat.value);
    break;
  case KM_CW:
    set_sweep_frequency(ST_CW, uistat.value);
    break;
  case KM_SCALE:
    set_trace_scale(uistat.current_trace, uistat.value / 1000.0);
    break;
  case KM_REFPOS:
    set_trace_refpos(uistat.current_trace, uistat.value / 1000.0);
    break;
  case KM_EDELAY:
    set_electrical_delay(uistat.value);
    break;
  case KM_VELOCITY_FACTOR:
    velocity_factor = uistat.value / 100.f;
    break;
  }
}

void
draw_numeric_area(void)
{
  char buf[NUMERIC_INPUT_DIGITS+1];

  // if NUMERIC_INPUT_DIGITS is changed, the below format string
  // must be changed as well.
  static_assert(NUMERIC_INPUT_DIGITS == 10);
  chsnprintf(buf, sizeof buf, "%10ld", uistat.value);

  draw_numeric_input(buf);
}


void
ui_mode_menu(void)
{
  if (ui_mode == UI_MENU)
    return;

  ui_mode = UI_MENU;
  /* narrowen plotting area */
  area_width = AREA_WIDTH_NORMAL - MENU_BUTTON_WIDTH;
  area_height = AREA_HEIGHT_NORMAL;
  ensure_selection();
  draw_menu();
}

void
ui_mode_keypad(int _keypad_mode)
{
  if (ui_mode == UI_KEYPAD)
    return;

  kp_index = 0;

  // keypads array
  keypad_mode = _keypad_mode;
  keypads = keypads_mode_tbl[_keypad_mode];
  int i;
  for (i = 0; keypads[i+1].c != KP_NONE; i++)
    ;
  keypads_last_index = i;

  ui_mode = UI_KEYPAD;
  area_width = AREA_WIDTH_NORMAL - MENU_BUTTON_WIDTH;
  area_height = HEIGHT - NUM_INPUT_HEIGHT;
  draw_menu();
  draw_keypad();
  draw_numeric_area_frame();
  enable_redraw(false);
}

void
ui_mode_normal(void)
{
  if (ui_mode == UI_NORMAL)
    return;

  area_width = AREA_WIDTH_NORMAL;
  area_height = AREA_HEIGHT_NORMAL;
  leave_ui_mode();
  ui_mode = UI_NORMAL;
}

static void
lever_move_marker(UIEvent evt)
{
  if (active_marker != MARKER_INVALID && markers[active_marker].enabled) {
    auto& am = markers[active_marker];
    int step = evt.isTick() ? 3 : 1;
    if (evt.isJogLeft()) {
      am.index -= step;
      if(am.index < 0)
        am.index = 0;
    }
    if (evt.isJogRight()) {
      am.index += step;
      if(am.index >= current_props._sweep_points)
        am.index = current_props._sweep_points - 1;
    }
    am.frequency = frequencyAt(am.index);
    request_to_redraw_marker(active_marker);
  }
}

static void
lever_search_marker(UIEvent evt)
{
  if (active_marker != MARKER_INVALID) {
    if (evt.isJogLeft()) {
      int i = marker_search_dir(uistat.marker_search_mode, markers[active_marker].index, MK_SEARCH_LEFT);
      if (i != -1)
        markers[active_marker].index = i;
    } else if (evt.isJogRight()) {
      int i = marker_search_dir(uistat.marker_search_mode, markers[active_marker].index, MK_SEARCH_RIGHT);
      if (i != -1)
        markers[active_marker].index = i;
    }
    redraw_marker(active_marker);
  }
}

// ex. 10942 -> 10000
//      6791 ->  5000
//       341 ->   200
static freqHz_t
step_round(freqHz_t v)
{
  // decade step
  freqHz_t x = 1;
  for (x = 1; x*10 < v; x *= 10)
    ;

  // 1-2-5 step
  if (x * 2 > v)
    return x;
  else if (x * 5 > v)
    return x * 2;
  else
    return x * 5;
}

static void
lever_zoom_span(UIEvent evt)
{
  freqHz_t span = get_sweep_frequency(ST_SPAN);
  if (evt.isJogLeft()) {
    span = step_round(span - 1);
    set_sweep_frequency(ST_SPAN, span);
  } else if (evt.isJogRight()) {
    span = step_round(span + 1);
    span = step_round(span * 3);
    set_sweep_frequency(ST_SPAN, span);
  }
}

static void
lever_move_center(UIEvent evt)
{
  freqHz_t center = get_sweep_frequency(ST_CENTER);
  freqHz_t span = get_sweep_frequency(ST_SPAN);
  span = step_round(span / 3);
  if (evt.isJogRight()) {
    set_sweep_frequency(ST_CENTER, center + span);
  } else if (evt.isJogLeft()) {
    set_sweep_frequency(ST_CENTER, center - span);
  }
}


static void
ui_process_normal(UIEvent evt)
{
  if (evt.isLeverClick()) {
    ui_mode_menu();
  }
  if(evt.isJog()) {
#ifdef ENABLE_LEVER_MODES
    switch (uistat.lever_mode) {
      case LM_MARKER: lever_move_marker(evt);   break;
      case LM_SEARCH: lever_search_marker(evt); break;
      case LM_CENTER: lever_move_center(evt);   break;
      case LM_SPAN:   lever_zoom_span(evt);     break;
    }
#else
    lever_move_marker(evt);
#endif
  }
  if(evt.isJogEnd()) {
    if (active_marker != MARKER_INVALID)
      request_to_redraw_marker(active_marker);
  }
  if(evt.isTouchPress()) {
    if (touch_pickup_marker()) {
      return;
    }
    // switch menu mode
    selection = -1;
    ui_mode_menu();
  }
}

static void
ui_process_menu(UIEvent evt)
{
  if (evt.isLeverClick() || evt.isLeverLongPress()) {
    if(selection < 0)
      goto menuclose;
    menu_invoke(evt, selection);
    return;
  }
  if (evt.isJogRight()) {
    if(menu_stack[menu_current_level][selection+1].type == MT_NONE)
      goto menuclose;
    selection++;
    draw_menu();
  }
  if (evt.isJogLeft()) {
    if (selection == 0)
      goto menuclose;
    if(selection < 0)
      return;
    selection--;
    draw_menu();
  }
  if(evt.isTouchPress()) {
    menu_apply_touch(evt);
  }
  return;
menuclose:
  ui_mode_normal();
}

static int
keypad_click(int key)
{
  int c = keypads[key].c;
  if ((c >= KP_X1 && c <= KP_G) || c == KP_N || c == KP_P) {
    float scale = 1;
    if (c >= KP_X1 && c <= KP_G) {
      int n = c - KP_X1;
      while (n-- > 0)
        scale *= 1000;
    } else if (c == KP_N) {
      scale *= 1000;
    }
    /* numeric input done */
    float value = my_atof(kp_buf) * scale;
    switch (keypad_mode) {
    case KM_START:
      set_sweep_frequency(ST_START, value);
      break;
    case KM_STOP:
      set_sweep_frequency(ST_STOP, value);
      break;
    case KM_CENTER:
      set_sweep_frequency(ST_CENTER, value);
      break;
    case KM_SPAN:
      set_sweep_frequency(ST_SPAN, value);
      break;
    case KM_POINTS:
      set_sweep_points(value);
      break;
    case KM_CW:
      set_sweep_frequency(ST_CW, value);
      break;
    case KM_SCALE:
      set_trace_scale(uistat.current_trace, value);
      break;
    case KM_REFPOS:
      set_trace_refpos(uistat.current_trace, value);
      break;
    case KM_EDELAY:
      set_electrical_delay(value); // pico seconds
      break;
    case KM_VELOCITY_FACTOR:
      velocity_factor = value / 100.f;
      break;
    case KM_SCALEDELAY:
      set_trace_scale(uistat.current_trace, value * 1e-12); // pico second
      break;
    }

    return KP_DONE;
  } else if (c <= 9 && kp_index < NUMINPUT_LEN)
    kp_buf[kp_index++] = '0' + c;
  else if (c == KP_PERIOD && kp_index < NUMINPUT_LEN) {
    // check period in former input
    int j;
    for (j = 0; j < kp_index && kp_buf[j] != '.'; j++)
      ;
    // append period if there are no period
    if (kp_index == j)
      kp_buf[kp_index++] = '.';
  } else if (c == KP_MINUS) {
    if (kp_index == 0)
      kp_buf[kp_index++] = '-';
  } else if (c == KP_BS) {
    if (kp_index == 0) {
      return KP_CANCEL;
    }
    --kp_index;
  }
  kp_buf[kp_index] = '\0';
  draw_numeric_input(kp_buf);
  return KP_CONTINUE;
}

static int
keypad_apply_touch(UIEvent evt)
{
  int touch_x, touch_y;
  int i = 0;

  if(!touch_position(&touch_x, &touch_y, evt))
    return -1;

  while (keypads[i].c != KP_NONE) {
    int x = KP_GET_X(keypads[i].x);
    int y = KP_GET_Y(keypads[i].y);
    if (x < touch_x && touch_x < x+KP_WIDTH && y < touch_y && touch_y < y+KP_HEIGHT) {
      // draw focus
      selection = i;
      draw_keypad();
      uiDisableProcessing();
      uiWaitEvent();
      uiEnableProcessing();
      // erase focus
      selection = -1;
      draw_keypad();
      return i;
    }
    i++;
  }
//  if (touch_y > 48 * 4) {
    // exit keypad mode
//    return -2;
//  }
  return -1;
}

void
ui_process_keypad(UIEvent evt)
{
  if (evt.isJogLeft()) {
    if (--selection < 0)
      selection = keypads_last_index;
    draw_keypad();
    return;
  }
  if (evt.isJogRight()) {
    if (++selection > keypads_last_index) {
      // reaches to tail
      selection = 0;
    }
    draw_keypad();
    return;
  }

  if (evt.isLeverClick()) {
    if (keypad_click(selection))
      /* exit loop on done or cancel */
      goto return_to_normal;
  }

  if (evt.isTouchPress()) {
    int key = keypad_apply_touch(evt);
    if (key >= 0 && keypad_click(key))
      /* exit loop on done or cancel */
      goto return_to_normal;
    else if (key == -2) {
      //xxx;
      return;
    }
  }
  return;

return_to_normal:
  redraw_frame();
  request_to_redraw_grid();
  ui_mode_normal();
  //redraw_all();
  uiEnableProcessing();
  enable_redraw(true);
}


static void
drag_marker(int t, int m)
{
  /* wait touch release */
  while(true) {
    int touch_x, touch_y;
    int index;
    if(!touch_position(&touch_x, &touch_y))
      break;
    touch_x -= OFFSETX;
    touch_y -= OFFSETY;
    index = search_nearest_index(touch_x, touch_y, t);
    if (index >= 0) {
      markers[m].index = index;
      markers[m].frequency = frequencyAt(index);
      redraw_marker(m);
    }
  }
}

static int
touch_pickup_marker(void)
{
  int touch_x, touch_y;
  if(!touch_position(&touch_x, &touch_y))
    return FALSE;
  touch_x -= OFFSETX;
  touch_y -= OFFSETY;
  int8_t i = MARKER_INVALID, mt, m, t;
  int min_dist = MARKER_PICKUP_DISTANCE * MARKER_PICKUP_DISTANCE;
  // Search closest marker to touch position
  for (t = 0; t < TRACES_MAX; t++) {
    if (!trace[t].enabled)
      continue;
    for (m = 0; m < MARKERS_MAX; m++) {
      if (!markers[m].enabled)
        continue;
      // Get distance to marker from touch point
      int dist = distance_to_index(t, markers[m].index, touch_x, touch_y);
      if (dist < min_dist) {
        min_dist = dist;
        i  = m;
        mt = t;
      }
    }
  }
  // Marker not found
  if (i == MARKER_INVALID)
    return FALSE;
  // Marker found, set as active and start drag it
  if (active_marker != i) {
    previous_marker = active_marker;
    active_marker = i;
  }

  // select trace
  uistat.current_trace = mt;
  // drag marker until release
  drag_marker(mt, i);
  return TRUE;
}

void
ui_process(UIEvent evt)
{
  // if the display is flipped, flip jog left/right too
  if(config.ui_options & UI_OPTIONS_FLIP) {
    if(evt.button == UIEventButtons::LeverLeft)
      evt.button = UIEventButtons::LeverRight;
    else if(evt.button == UIEventButtons::LeverRight)
      evt.button = UIEventButtons::LeverLeft;
  }

  if(uiEventsEnabled)
    lastUIEvent = {};
  else {
    lastUIEvent = evt;
    return;
  }

  if(ui_disabled) return;

//  if(evt.isTouchPress())
//    awd_count++;

  switch (ui_mode) {
  case UI_NORMAL:
    ui_process_normal(evt);
    break;
  case UI_MENU:
    ui_process_menu(evt);
    break;
  case UI_KEYPAD:
    ui_process_keypad(evt);
    break;
  case UI_USB_MODE:
    if(evt.isLeverLongPress()) {
      UIActions::reconnectUSB();
    }
    break;
  }
}
