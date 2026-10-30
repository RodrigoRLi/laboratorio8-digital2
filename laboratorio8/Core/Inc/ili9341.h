#ifndef ILI9341_H
#define ILI9341_H

#include "main.h"
#include <stdint.h>

extern SPI_HandleTypeDef hspi1;

/* Resolucion en orientacion horizontal (landscape) */
#define LCD_W 320
#define LCD_H 240

/* Colores RGB565 */
#define COL_BLACK    0x0000
#define COL_WHITE    0xFFFF
#define COL_RED      0xF800
#define COL_GREEN    0x07E0
#define COL_BLUE     0x001F
#define COL_CYAN     0x07FF
#define COL_YELLOW   0xFFE0
#define COL_NAVY     0x0010
#define COL_DARKBLUE 0x0299   /* azul de la barra de titulo */
#define COL_BG       0x0841   /* casi negro azulado */
#define COL_PINK     0xFBB5
#define COL_LAVENDER 0x949F

/* RGB888 -> RGB565 */
#define RGB565(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))

/* Parte 1: pixeles y figuras */
void ILI9341_Init(void);
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9341_FillScreen(uint16_t color);
void ILI9341_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t t, uint16_t color);
void ILI9341_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t t, uint16_t color);

/* Parte 2: texto (fuente 5x7 escalable) */
void ILI9341_DrawChar(uint16_t x, uint16_t y, char c, uint8_t scale, uint16_t fg, uint16_t bg);
void ILI9341_DrawString(uint16_t x, uint16_t y, const char *s, uint8_t scale, uint16_t fg, uint16_t bg);
uint16_t ILI9341_StringWidth(const char *s, uint8_t scale);

/* Parte 3: imagenes */
void ILI9341_DrawBitmap1bpp16(uint16_t x, uint16_t y, const uint16_t *bmp, uint8_t scale, uint16_t fg);
void ILI9341_DrawImageRGB565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);

#endif
