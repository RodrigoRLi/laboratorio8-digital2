#include "ili9341.h"
#include <string.h>

#define CS_LOW()   HAL_GPIO_WritePin(LCDcs_GPIO_Port,    LCDcs_Pin,    GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(LCDcs_GPIO_Port,    LCDcs_Pin,    GPIO_PIN_SET)
#define DC_CMD()   HAL_GPIO_WritePin(LCDdc_GPIO_Port,    LCDdc_Pin,    GPIO_PIN_RESET)
#define DC_DATA()  HAL_GPIO_WritePin(LCDdc_GPIO_Port,    LCDdc_Pin,    GPIO_PIN_SET)
#define RST_LOW()  HAL_GPIO_WritePin(LCDreset_GPIO_Port, LCDreset_Pin, GPIO_PIN_RESET)
#define RST_HIGH() HAL_GPIO_WritePin(LCDreset_GPIO_Port, LCDreset_Pin, GPIO_PIN_SET)

/* ---------- Fuente 5x7 (ASCII 32..90), columnas, bit0 = fila superior ---------- */
static const uint8_t font5x7[][5] = {
 {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
 {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0x00,0x08,0x07,0x03,0x00},
 {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x2A,0x1C,0x7F,0x1C,0x2A},{0x08,0x08,0x3E,0x08,0x08},
 {0x00,0x80,0x70,0x30,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x60,0x60,0x00},{0x20,0x10,0x08,0x04,0x02},
 {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},
 {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,0x09,0x07},
 {0x36,0x49,0x49,0x49,0x36},{0x46,0x49,0x49,0x29,0x1E},{0x00,0x00,0x14,0x00,0x00},{0x00,0x40,0x34,0x00,0x00},
 {0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x59,0x09,0x06},
 {0x3E,0x41,0x5D,0x59,0x4E},{0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
 {0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x41,0x51,0x73},
 {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
 {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x1C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
 {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},
 {0x03,0x01,0x7F,0x01,0x03},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
 {0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x59,0x49,0x4D,0x43}
};

/* ---------- Capa SPI ---------- */
static void write_cmd(uint8_t cmd)
{
    DC_CMD(); CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    CS_HIGH();
}

static void write_data(const uint8_t *d, uint16_t n)
{
    DC_DATA(); CS_LOW();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)d, n, HAL_MAX_DELAY);
    CS_HIGH();
}

static void cmd_data(uint8_t cmd, const uint8_t *d, uint16_t n)
{
    write_cmd(cmd);
    if (n) write_data(d, n);
}

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t c[4];
    c[0] = x0 >> 8; c[1] = x0 & 0xFF; c[2] = x1 >> 8; c[3] = x1 & 0xFF;
    cmd_data(0x2A, c, 4);
    c[0] = y0 >> 8; c[1] = y0 & 0xFF; c[2] = y1 >> 8; c[3] = y1 & 0xFF;
    cmd_data(0x2B, c, 4);
    write_cmd(0x2C); /* Memory write */
}

/* ---------- Inicializacion ---------- */
void ILI9341_Init(void)
{
    CS_HIGH();
    RST_LOW();  HAL_Delay(20);
    RST_HIGH(); HAL_Delay(120);

    cmd_data(0xCF, (uint8_t[]){0x00,0xC1,0x30}, 3);
    cmd_data(0xED, (uint8_t[]){0x64,0x03,0x12,0x81}, 4);
    cmd_data(0xE8, (uint8_t[]){0x85,0x00,0x78}, 3);
    cmd_data(0xCB, (uint8_t[]){0x39,0x2C,0x00,0x34,0x02}, 5);
    cmd_data(0xF7, (uint8_t[]){0x20}, 1);
    cmd_data(0xEA, (uint8_t[]){0x00,0x00}, 2);
    cmd_data(0xC0, (uint8_t[]){0x23}, 1);
    cmd_data(0xC1, (uint8_t[]){0x10}, 1);
    cmd_data(0xC5, (uint8_t[]){0x3E,0x28}, 2);
    cmd_data(0xC7, (uint8_t[]){0x86}, 1);
    cmd_data(0x36, (uint8_t[]){0x28}, 1);   /* MADCTL: landscape, BGR. Si sale espejo: 0xE8 / 0x68 / 0x88 */
    cmd_data(0x3A, (uint8_t[]){0x55}, 1);   /* 16 bits/pixel */
    cmd_data(0xB1, (uint8_t[]){0x00,0x18}, 2);
    cmd_data(0xB6, (uint8_t[]){0x08,0x82,0x27}, 3);
    cmd_data(0xF2, (uint8_t[]){0x00}, 1);
    cmd_data(0x26, (uint8_t[]){0x01}, 1);
    cmd_data(0xE0, (uint8_t[]){0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,0x37,0x07,0x10,0x03,0x0E,0x09,0x00}, 15);
    cmd_data(0xE1, (uint8_t[]){0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F}, 15);
    write_cmd(0x11); HAL_Delay(120);        /* Sleep out */
    write_cmd(0x29);                        /* Display on */
}

/* ---------- Parte 1: pixeles y figuras ---------- */
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_W || y >= LCD_H) return;
    uint8_t px[2] = { color >> 8, color & 0xFF };
    set_window(x, y, x, y);
    write_data(px, 2);
}

void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= LCD_W || y >= LCD_H || w == 0 || h == 0) return;
    if (x + w > LCD_W) w = LCD_W - x;
    if (y + h > LCD_H) h = LCD_H - y;

    static uint8_t buf[512];
    uint8_t hi = color >> 8, lo = color & 0xFF;
    for (uint16_t i = 0; i < sizeof(buf); i += 2) { buf[i] = hi; buf[i + 1] = lo; }

    set_window(x, y, x + w - 1, y + h - 1);
    uint32_t total = (uint32_t)w * h * 2;
    DC_DATA(); CS_LOW();
    while (total) {
        uint16_t n = (total > sizeof(buf)) ? sizeof(buf) : total;
        HAL_SPI_Transmit(&hspi1, buf, n, HAL_MAX_DELAY);
        total -= n;
    }
    CS_HIGH();
}

void ILI9341_FillScreen(uint16_t color) { ILI9341_FillRect(0, 0, LCD_W, LCD_H, color); }

void ILI9341_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t t, uint16_t color)
{
    ILI9341_FillRect(x, y, w, t, color);
    ILI9341_FillRect(x, y + h - t, w, t, color);
    ILI9341_FillRect(x, y, t, h, color);
    ILI9341_FillRect(x + w - t, y, t, h, color);
}

/* Borde con esquinas "cortadas" estilo pixel-art */
void ILI9341_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t t, uint16_t color)
{
    uint8_t r = t * 2;
    ILI9341_FillRect(x + r, y, w - 2 * r, t, color);
    ILI9341_FillRect(x + r, y + h - t, w - 2 * r, t, color);
    ILI9341_FillRect(x, y + r, t, h - 2 * r, color);
    ILI9341_FillRect(x + w - t, y + r, t, h - 2 * r, color);
    /* esquinas diagonales */
    ILI9341_FillRect(x + t, y + t, t, t, color);
    ILI9341_FillRect(x + w - 2 * t, y + t, t, t, color);
    ILI9341_FillRect(x + t, y + h - 2 * t, t, t, color);
    ILI9341_FillRect(x + w - 2 * t, y + h - 2 * t, t, t, color);
}

/* ---------- Parte 2: texto ---------- */
void ILI9341_DrawChar(uint16_t x, uint16_t y, char c, uint8_t scale, uint16_t fg, uint16_t bg)
{
    if (c >= 'a' && c <= 'z') c -= 32;           /* solo mayusculas */
    if (c < 32 || c > 90) c = '?';
    const uint8_t *g = font5x7[c - 32];

    ILI9341_FillRect(x, y, 6 * scale, 8 * scale, bg);   /* celda con fondo */
    for (uint8_t col = 0; col < 5; col++)
        for (uint8_t row = 0; row < 7; row++)
            if (g[col] & (1 << row))
                ILI9341_FillRect(x + col * scale, y + row * scale, scale, scale, fg);
}

void ILI9341_DrawString(uint16_t x, uint16_t y, const char *s, uint8_t scale, uint16_t fg, uint16_t bg)
{
    while (*s) {
        ILI9341_DrawChar(x, y, *s++, scale, fg, bg);
        x += 6 * scale;
    }
}

uint16_t ILI9341_StringWidth(const char *s, uint8_t scale) { return strlen(s) * 6 * scale; }

/* ---------- Parte 3: imagenes ---------- */
/* Bitmap monocromo 16x16 (16 palabras, bit15 = columna izquierda) escalable */
void ILI9341_DrawBitmap1bpp16(uint16_t x, uint16_t y, const uint16_t *bmp, uint8_t scale, uint16_t fg)
{
    for (uint8_t row = 0; row < 16; row++)
        for (uint8_t col = 0; col < 16; col++)
            if (bmp[row] & (0x8000 >> col))
                ILI9341_FillRect(x + col * scale, y + row * scale, scale, scale, fg);
}

/* Imagen a color RGB565 (por ejemplo exportada con Image2LCD / LCD Image Converter) */
void ILI9341_DrawImageRGB565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
    static uint8_t line[LCD_W * 2];
    set_window(x, y, x + w - 1, y + h - 1);
    DC_DATA(); CS_LOW();
    for (uint16_t r = 0; r < h; r++) {
        for (uint16_t c = 0; c < w; c++) {
            uint16_t p = data[r * w + c];
            line[2 * c] = p >> 8; line[2 * c + 1] = p & 0xFF;
        }
        HAL_SPI_Transmit(&hspi1, line, w * 2, HAL_MAX_DELAY);
    }
    CS_HIGH();
}
