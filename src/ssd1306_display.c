/**
 * @file ssd1306_display.c
 * @brief Implementation of SSD1306 OLED Display Driver
 * 
 * This implementation communicates with the SSD1306 via I2C using ESP-IDF's
 * I2C master driver. The display buffer is maintained in RAM and transferred
 * to the display GDDRAM when display_update() is called.
 * 
 * Memory Layout:
 * - Display buffer: 1024 bytes (128x64 pixels / 8 bits per byte)
 * - Organized as 8 pages of 128 bytes each
 * - Each byte represents 8 vertical pixels
 * 
 * Reference: SSD1306 Datasheet Rev 1.1, Solomon Systech
 * 
 */

#include "ssd1306_display.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Logging tag */
static const char *TAG = "SSD1306";

/* I2C configuration */
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_TIMEOUT_MS 1000

/* SSD1306 Commands (from datasheet section 8) */
#define SSD1306_CMD_SET_CONTRAST        0x81
#define SSD1306_CMD_DISPLAY_ALL_ON_RES  0xA4
#define SSD1306_CMD_DISPLAY_ALL_ON      0xA5
#define SSD1306_CMD_NORMAL_DISPLAY      0xA6
#define SSD1306_CMD_INVERT_DISPLAY      0xA7
#define SSD1306_CMD_DISPLAY_OFF         0xAE
#define SSD1306_CMD_DISPLAY_ON          0xAF
#define SSD1306_CMD_SET_DISPLAY_OFFSET  0xD3
#define SSD1306_CMD_SET_COM_PINS        0xDA
#define SSD1306_CMD_SET_VCOM_DETECT     0xDB
#define SSD1306_CMD_SET_DISPLAY_CLK_DIV 0xD5
#define SSD1306_CMD_SET_PRECHARGE       0xD9
#define SSD1306_CMD_SET_MULTIPLEX       0xA8
#define SSD1306_CMD_SET_LOW_COLUMN      0x00
#define SSD1306_CMD_SET_HIGH_COLUMN     0x10
#define SSD1306_CMD_SET_START_LINE      0x40
#define SSD1306_CMD_MEMORY_MODE         0x20
#define SSD1306_CMD_COLUMN_ADDR         0x21
#define SSD1306_CMD_PAGE_ADDR           0x22
#define SSD1306_CMD_COM_SCAN_INC        0xC0
#define SSD1306_CMD_COM_SCAN_DEC        0xC8
#define SSD1306_CMD_SEG_REMAP           0xA0
#define SSD1306_CMD_CHARGE_PUMP         0x8D
#define SSD1306_CMD_EXTERNAL_VCC        0x01
#define SSD1306_CMD_SWITCH_CAP_VCC      0x02

/* Control byte for I2C communication */
#define SSD1306_CONTROL_CMD_SINGLE  0x80
#define SSD1306_CONTROL_CMD_STREAM  0x00
#define SSD1306_CONTROL_DATA_STREAM 0x40

/* Display buffer size */
#define BUFFER_SIZE ((DISPLAY_WIDTH * DISPLAY_HEIGHT) / 8)

/**
 * @brief Basic 6x8 pixel font
 * 
 * Standard ASCII font for characters 0x20-0x7F.
 * Each character is 5 pixels wide with 1 pixel spacing = 6 pixels total.
 * Each byte represents a vertical column of 8 pixels.
 */
static const uint8_t font_6x8[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space (0x20)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // Backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x10, 0x08, 0x08, 0x10, 0x08}, // ~
};

/**
 * @brief Display driver context
 */
typedef struct {
    uint8_t buffer[BUFFER_SIZE];  
    display_config_t config;      
    bool initialized;             
} display_context_t;

/* Global display context */
static display_context_t g_display = {0};

/* Forward declarations */
static display_error_t i2c_write_cmd(uint8_t cmd);
static display_error_t i2c_write_cmd_arg(uint8_t cmd, uint8_t arg);
static display_error_t i2c_write_data(const uint8_t *data, size_t len);
static void draw_char(int16_t x, int16_t y, char c, display_text_size_t size, 
                     display_color_t color);

/**
 * @brief Write a command byte to SSD1306
 * 
 * Uses I2C control byte 0x00 (Co=0, D/C=0) to indicate command follows.
 * Reference: SSD1306 datasheet section 8.1.5
 */
static display_error_t i2c_write_cmd(uint8_t cmd)
{
    uint8_t data[2] = {SSD1306_CONTROL_CMD_STREAM, cmd};
    
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM, 
                                               g_display.config.i2c_addr,
                                               data, sizeof(data),
                                               pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C write command failed: %s", esp_err_to_name(err));
        return DISPLAY_ERR_I2C_FAILED;
    }
    
    return DISPLAY_OK;
}

/**
 * @brief Write a command with one argument byte
 */
static display_error_t i2c_write_cmd_arg(uint8_t cmd, uint8_t arg)
{
    uint8_t data[3] = {SSD1306_CONTROL_CMD_STREAM, cmd, arg};
    
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM,
                                               g_display.config.i2c_addr,
                                               data, sizeof(data),
                                               pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C write command with arg failed: %s", esp_err_to_name(err));
        return DISPLAY_ERR_I2C_FAILED;
    }
    
    return DISPLAY_OK;
}

/**
 * @brief Write data bytes to SSD1306 GDDRAM
 * 
 * Uses I2C control byte 0x40 (Co=0, D/C=1) to indicate data follows.
 * Reference: SSD1306 datasheet section 8.1.5
 */
static display_error_t i2c_write_data(const uint8_t *data, size_t len)
{
    /* Allocate buffer for control byte + data */
    uint8_t *buffer = malloc(len + 1);
    if (!buffer) {
        return DISPLAY_ERR_INVALID_PARAM;
    }
    
    buffer[0] = SSD1306_CONTROL_DATA_STREAM;
    memcpy(buffer + 1, data, len);
    
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM,
                                               g_display.config.i2c_addr,
                                               buffer, len + 1,
                                               pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    
    free(buffer);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C write data failed: %s", esp_err_to_name(err));
        return DISPLAY_ERR_I2C_FAILED;
    }
    
    return DISPLAY_OK;
}

display_error_t display_init(const display_config_t *config)
{
    /* Use default configuration if not provided */
    if (config) {
        memcpy(&g_display.config, config, sizeof(display_config_t));
    } else {
        g_display.config.sda_pin = DISPLAY_DEFAULT_SDA_PIN;
        g_display.config.scl_pin = DISPLAY_DEFAULT_SCL_PIN;
        g_display.config.i2c_addr = DISPLAY_DEFAULT_I2C_ADDR;
        g_display.config.i2c_freq_hz = 400000;  /* 400 kHz I2C fast mode */
    }
    
    /* Configure I2C master */
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = g_display.config.sda_pin,
        .scl_io_num = g_display.config.scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = g_display.config.i2c_freq_hz,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &i2c_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C parameter config failed: %s", esp_err_to_name(err));
        return DISPLAY_ERR_INIT_FAILED;
    }
    
    err = i2c_driver_install(I2C_MASTER_NUM, i2c_conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return DISPLAY_ERR_INIT_FAILED;
    }
    
    /* Small delay for display power-up */
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* SSD1306 initialization sequence (per datasheet section 8.9) */
    
    /* Display OFF */
    i2c_write_cmd(SSD1306_CMD_DISPLAY_OFF);
    
    /* Set display clock divide ratio/oscillator frequency */
    i2c_write_cmd_arg(SSD1306_CMD_SET_DISPLAY_CLK_DIV, 0x80);
    
    /* Set multiplex ratio (1 to 64) */
    i2c_write_cmd_arg(SSD1306_CMD_SET_MULTIPLEX, DISPLAY_HEIGHT - 1);
    
    /* Set display offset to 0 */
    i2c_write_cmd_arg(SSD1306_CMD_SET_DISPLAY_OFFSET, 0x00);
    
    /* Set display start line to 0 */
    i2c_write_cmd(SSD1306_CMD_SET_START_LINE | 0x00);
    
    /* Enable charge pump regulator (required for OLED operation from 3.3V) */
    /* Reference: SSD1306 Application Note, Charge Pump Setting */
    i2c_write_cmd_arg(SSD1306_CMD_CHARGE_PUMP, 0x14);
    
    /* Set memory addressing mode to horizontal */
    i2c_write_cmd_arg(SSD1306_CMD_MEMORY_MODE, 0x00);
    
    /* Set segment re-map (column address 127 mapped to SEG0) */
    i2c_write_cmd(SSD1306_CMD_SEG_REMAP | 0x01);
    
    /* Set COM output scan direction (scan from COM[N-1] to COM0) */
    i2c_write_cmd(SSD1306_CMD_COM_SCAN_DEC);
    
    /* Set COM pins hardware configuration */
    i2c_write_cmd_arg(SSD1306_CMD_SET_COM_PINS, 0x12);
    
    /* Set contrast control */
    i2c_write_cmd_arg(SSD1306_CMD_SET_CONTRAST, 0x7F);
    
    /* Set pre-charge period */
    i2c_write_cmd_arg(SSD1306_CMD_SET_PRECHARGE, 0xF1);
    
    /* Set VCOMH deselect level */
    i2c_write_cmd_arg(SSD1306_CMD_SET_VCOM_DETECT, 0x40);
    
    /* Enable display output from RAM */
    i2c_write_cmd(SSD1306_CMD_DISPLAY_ALL_ON_RES);
    
    /* Set normal display (not inverted) */
    i2c_write_cmd(SSD1306_CMD_NORMAL_DISPLAY);
    
    /* Clear display buffer */
    memset(g_display.buffer, 0, BUFFER_SIZE);
    display_update();
    
    /* Turn on display */
    i2c_write_cmd(SSD1306_CMD_DISPLAY_ON);
    
    g_display.initialized = true;
    ESP_LOGI(TAG, "Display initialized successfully");
    
    return DISPLAY_OK;
}

display_error_t display_clear(void)
{
    if (!g_display.initialized) {
        return DISPLAY_ERR_NOT_INITIALIZED;
    }
    
    memset(g_display.buffer, 0, BUFFER_SIZE);
    return display_update();
}

display_error_t display_update(void)
{
    if (!g_display.initialized) {
        return DISPLAY_ERR_NOT_INITIALIZED;
    }
    
    /* Set column address range (0 to 127) */
    i2c_write_cmd(SSD1306_CMD_COLUMN_ADDR);
    i2c_write_cmd(0);
    i2c_write_cmd(DISPLAY_WIDTH - 1);
    
    /* Set page address range (0 to 7) */
    i2c_write_cmd(SSD1306_CMD_PAGE_ADDR);
    i2c_write_cmd(0);
    i2c_write_cmd((DISPLAY_HEIGHT / 8) - 1);
    
    /* Transfer entire buffer to GDDRAM */
    return i2c_write_data(g_display.buffer, BUFFER_SIZE);
}

display_error_t display_set_pixel(int16_t x, int16_t y, display_color_t color)
{
    if (!g_display.initialized) {
        return DISPLAY_ERR_NOT_INITIALIZED;
    }
    
    /* Check bounds */
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
        return DISPLAY_OK;  /* Silently clip out-of-bounds pixels */
    }
    
    /* Calculate buffer position
     * Display is organized as 8 pages (rows) of 128 bytes
     * Each byte represents 8 vertical pixels
     */
    uint16_t index = x + (y / 8) * DISPLAY_WIDTH;
    uint8_t bit = 1 << (y % 8);
    
    switch (color) {
        case COLOR_WHITE:
            g_display.buffer[index] |= bit;
            break;
        case COLOR_BLACK:
            g_display.buffer[index] &= ~bit;
            break;
        case COLOR_INVERT:
            g_display.buffer[index] ^= bit;
            break;
    }
    
    return DISPLAY_OK;
}

display_error_t display_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                 display_color_t color)
{
    /* Bresenham's line algorithm */
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    while (1) {
        display_set_pixel(x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
    
    return DISPLAY_OK;
}

display_error_t display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                                 display_color_t color, bool filled)
{
    if (filled) {
        for (int16_t i = x; i < x + w; i++) {
            for (int16_t j = y; j < y + h; j++) {
                display_set_pixel(i, j, color);
            }
        }
    } else {
        /* Draw four sides */
        display_draw_line(x, y, x + w - 1, y, color);           /* Top */
        display_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color); /* Bottom */
        display_draw_line(x, y, x, y + h - 1, color);           /* Left */
        display_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color); /* Right */
    }
    
    return DISPLAY_OK;
}

display_error_t display_draw_circle(int16_t x0, int16_t y0, int16_t r,
                                   display_color_t color, bool filled)
{
    /* Midpoint circle algorithm */
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    
    while (x >= y) {
        if (filled) {
            display_draw_line(x0 - x, y0 + y, x0 + x, y0 + y, color);
            display_draw_line(x0 - x, y0 - y, x0 + x, y0 - y, color);
            display_draw_line(x0 - y, y0 + x, x0 + y, y0 + x, color);
            display_draw_line(x0 - y, y0 - x, x0 + y, y0 - x, color);
        } else {
            display_set_pixel(x0 + x, y0 + y, color);
            display_set_pixel(x0 + y, y0 + x, color);
            display_set_pixel(x0 - y, y0 + x, color);
            display_set_pixel(x0 - x, y0 + y, color);
            display_set_pixel(x0 - x, y0 - y, color);
            display_set_pixel(x0 - y, y0 - x, color);
            display_set_pixel(x0 + y, y0 - x, color);
            display_set_pixel(x0 + x, y0 - y, color);
        }
        
        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }
    
    return DISPLAY_OK;
}

/**
 * @brief Draw a single character
 */
static void draw_char(int16_t x, int16_t y, char c, display_text_size_t size,
                     display_color_t color)
{
    if (c < 0x20 || c > 0x7E) {
        return;  /* Character not in font */
    }
    
    const uint8_t *glyph = font_6x8[c - 0x20];
    
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t line = glyph[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (line & 0x01) {
                /* Draw scaled pixel */
                if (size == 1) {
                    display_set_pixel(x + i, y + j, color);
                } else {
                    display_draw_rect(x + (i * size), y + (j * size), 
                                    size, size, color, true);
                }
            }
            line >>= 1;
        }
    }
}

display_error_t display_print_text(int16_t x, int16_t y, const char *text,
                                   display_text_size_t size, display_color_t color)
{
    if (!g_display.initialized) {
        return DISPLAY_ERR_NOT_INITIALIZED;
    }
    
    if (!text) {
        return DISPLAY_ERR_INVALID_PARAM;
    }
    
    int16_t cursor_x = x;
    
    while (*text) {
        draw_char(cursor_x, y, *text, size, color);
        cursor_x += 6 * size;  /* 6 pixels per character (5 + 1 spacing) */
        text++;
    }
    
    return DISPLAY_OK;
}

display_error_t display_printf(int16_t x, int16_t y, display_text_size_t size,
                              display_color_t color, const char *format, ...)
{
    char buffer[128];
    va_list args;
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    return display_print_text(x, y, buffer, size, color);
}

display_error_t display_set_contrast(uint8_t contrast)
{
    if (!g_display.initialized) {
        return DISPLAY_ERR_NOT_INITIALIZED;
    }
    
    return i2c_write_cmd_arg(SSD1306_CMD_SET_CONTRAST, contrast);
}

display_error_t display_set_power(bool on)
{
    if (!g_display.initialized) {
        return DISPLAY_ERR_NOT_INITIALIZED;
    }
    
    return i2c_write_cmd(on ? SSD1306_CMD_DISPLAY_ON : SSD1306_CMD_DISPLAY_OFF);
}

display_error_t display_invert(bool invert)
{
    if (!g_display.initialized) {
        return DISPLAY_ERR_NOT_INITIALIZED;
    }
    
    return i2c_write_cmd(invert ? SSD1306_CMD_INVERT_DISPLAY : 
                                  SSD1306_CMD_NORMAL_DISPLAY);
}

void display_get_dimensions(uint16_t *width, uint16_t *height)
{
    if (width) *width = DISPLAY_WIDTH;
    if (height) *height = DISPLAY_HEIGHT;
}

display_error_t display_deinit(void)
{
    if (!g_display.initialized) {
        return DISPLAY_OK;
    }
    
    /* Turn off display */
    display_set_power(false);
    
    /* Uninstall I2C driver */
    i2c_driver_delete(I2C_MASTER_NUM);
    
    g_display.initialized = false;
    ESP_LOGI(TAG, "Display deinitialized");
    
    return DISPLAY_OK;
}