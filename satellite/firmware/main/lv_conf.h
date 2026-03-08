/*
 * LVGL 8.3 configuration for Satellite Display firmware
 *
 * Minimal configuration: only the widgets and features needed
 * for the cockpit-style round display UI.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color settings */
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP    1   /* Byte-swap for SPI displays */

/* Memory */
#define LV_MEM_CUSTOM       1
#define LV_MEM_CUSTOM_INCLUDE   <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC     malloc
#define LV_MEM_CUSTOM_FREE      free
#define LV_MEM_CUSTOM_REALLOC   realloc

/* Display */
#define LV_DISP_DEF_REFR_PERIOD 16  /* ~60 fps */
#define LV_DPI_DEF               200

/* Logging */
#define LV_USE_LOG       0

/* Asserts */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/* GPU - none */
#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0
#define LV_USE_GPU_SDL          0

/* Fonts - only what we need */
#define LV_FONT_MONTSERRAT_10   1
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_8    0
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_20   0
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   0
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   0
#define LV_FONT_MONTSERRAT_34   0
#define LV_FONT_MONTSERRAT_36   0
#define LV_FONT_MONTSERRAT_38   0
#define LV_FONT_MONTSERRAT_40   0
#define LV_FONT_MONTSERRAT_42   0
#define LV_FONT_MONTSERRAT_44   0
#define LV_FONT_MONTSERRAT_46   0
#define LV_FONT_MONTSERRAT_48   0

#define LV_FONT_DEFAULT          &lv_font_montserrat_14

/* Symbols - we use a few for arrows and alerts */
#define LV_USE_FONT_COMPRESSED   0
#define LV_FONT_SUBPX_BGR       0

/* Widgets - only what we use */
#define LV_USE_ARC       1
#define LV_USE_LABEL     1
#define LV_USE_LINE      1
#define LV_USE_IMG       0
#define LV_USE_BTN       0
#define LV_USE_BTNMATRIX 0
#define LV_USE_BAR       0
#define LV_USE_SLIDER    0
#define LV_USE_SWITCH    0
#define LV_USE_CHECKBOX  0
#define LV_USE_DROPDOWN  0
#define LV_USE_ROLLER    0
#define LV_USE_TEXTAREA  0
#define LV_USE_TABLE     0
#define LV_USE_CHART     0
#define LV_USE_METER     0
#define LV_USE_CANVAS    0
#define LV_USE_LED       0
#define LV_USE_LIST      0
#define LV_USE_MENU      0
#define LV_USE_MSGBOX    0
#define LV_USE_SPINBOX   0
#define LV_USE_SPINNER   0
#define LV_USE_TABVIEW   0
#define LV_USE_TILEVIEW  0
#define LV_USE_WIN       0
#define LV_USE_SPAN      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_KEYBOARD  0
#define LV_USE_ANIMIMG   0
#define LV_USE_CALENDAR  0

/* Layout */
#define LV_USE_FLEX      0
#define LV_USE_GRID      0

/* Themes */
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_DARK   1
#define LV_USE_THEME_BASIC      0
#define LV_USE_THEME_MONO       0

/* Text */
#define LV_TXT_ENC                 LV_TXT_ENC_UTF8
#define LV_USE_BIDI                0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/* Other */
#define LV_USE_SNAPSHOT    0
#define LV_USE_MONKEY      0
#define LV_USE_GRIDNAV     0
#define LV_USE_FRAGMENT    0
#define LV_USE_IMGFONT     0
#define LV_USE_MSG         0
#define LV_USE_IME_PINYIN  0

/* File system - not needed */
#define LV_USE_FS_STDIO    0
#define LV_USE_FS_POSIX    0
#define LV_USE_FS_FATFS    0

/* Tick - custom via esp_timer */
#define LV_TICK_CUSTOM     0

#endif /* LV_CONF_H */
