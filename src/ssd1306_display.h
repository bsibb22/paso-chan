/**
 * @file ssd1306_display.h
 * @brief SSD1306 OLED Display Driver API
 * 
 * This module provides a simple interface for controlling the SSD1306
 * 128x64 OLED display via I2C. It offers text rendering, graphics primitives,
 * and display control functions suitable for embedded applications.
 * 
 * Hardware Configuration:
 * - Display: Adafruit SSD1306 0.96" OLED (128x64 pixels)
 * - Interface: I2C
 * - ESP32 Connections:
 *   * SDA -> GPIO 21 (configurable)
 *   * SCL -> GPIO 22 (configurable)
 *   * VCC -> 3.3V
 *   * GND -> GND
 * - I2C Address: 0x3C
 * 
 * Features:
 * - Simple text output with multiple sizes
 * - Basic graphics (pixels, lines, rectangles, circles)
 * - Full display buffer control
 * - Contrast adjustment
 * - Display on/off control
 * 
 * References:
 * - SSD1306 Datasheet: Solomon Systech SSD1306 Rev 1.1
 * - ESP-IDF I2C Driver: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
 * 
 */

#ifndef SSD1306_DISPLAY_H
#define SSD1306_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Display dimensions
 */
#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64

/**
 * @brief Default I2C configuration
 * Based on Freenove ESP32-WROOM board pinout
 */
#define DISPLAY_DEFAULT_SDA_PIN 21
#define DISPLAY_DEFAULT_SCL_PIN 22
#define DISPLAY_DEFAULT_I2C_ADDR 0x3C

/**
 * @brief Text size options
 * 
 * These multipliers scale the default 6x8 pixel font.
 * TEXT_SIZE_1 = 6x8 pixels per character
 * TEXT_SIZE_2 = 12x16 pixels per character
 * TEXT_SIZE_3 = 18x24 pixels per character
 */
typedef enum {
    TEXT_SIZE_1 = 1,
    TEXT_SIZE_2 = 2,
    TEXT_SIZE_3 = 3,
    TEXT_SIZE_4 = 4
} display_text_size_t;

/**
 * @brief Display colors
 */
typedef enum {
    COLOR_BLACK = 0,  /**< Pixel off */
    COLOR_WHITE = 1,  /**< Pixel on */
    COLOR_INVERT = 2  /**< Invert pixel state */
} display_color_t;

/**
 * @brief Display configuration structure
 */
typedef struct {
    uint8_t sda_pin;      
    uint8_t scl_pin;      
    uint8_t i2c_addr;   
    uint32_t i2c_freq_hz; 
} display_config_t;

/**
 * @brief Display error codes
 */
typedef enum {
    DISPLAY_OK = 0,
    DISPLAY_ERR_INIT_FAILED,
    DISPLAY_ERR_I2C_FAILED,
    DISPLAY_ERR_INVALID_PARAM,
    DISPLAY_ERR_NOT_INITIALIZED
} display_error_t;

/**
 * @brief Initialize the display
 * 
 * Configures I2C communication and initializes the controller
 * with appropriate settings for the 128x64 display.
 * 
 * Initialization sequence (per SSD1306 datasheet section 8.9):
 * 1. Set display OFF
 * 2. Configure display timing and driving scheme
 * 3. Configure charge pump regulator
 * 4. Clear display RAM
 * 5. Set display ON
 * 
 * @param config Pointer to display configuration, or NULL for defaults
 * 
 * @return DISPLAY_OK on success, error code otherwise
 * 
 * Example:
 * @code
 * display_config_t config = {
 *     .sda_pin = 21,
 *     .scl_pin = 22,
 *     .i2c_addr = 0x3C,
 *     .i2c_freq_hz = 400000
 * };
 * display_init(&config);
 * @endcode
 */
display_error_t display_init(const display_config_t *config);

/**
 * @brief Clear the entire display
 * 
 * Sets all pixels to black (off) and updates the display.
 * 
 * @return DISPLAY_OK on success, error code otherwise
 */
display_error_t display_clear(void);

/**
 * @brief Update the display with buffer contents
 * 
 * Transfers the internal display buffer to the SSD1306 GDDRAM.
 * This function must be called after drawing operations to make
 * changes visible.
 * 
 * @return DISPLAY_OK on success, error code otherwise
 * 
 * @note For performance, batch multiple drawing operations before
 *       calling display_update().
 */
display_error_t display_update(void);

/**
 * @brief Set a single pixel
 * 
 * @param x X coordinate (0 to DISPLAY_WIDTH-1)
 * @param y Y coordinate (0 to DISPLAY_HEIGHT-1)
 * @param color Pixel color (COLOR_WHITE, COLOR_BLACK, or COLOR_INVERT)
 * 
 * @return DISPLAY_OK on success, error code otherwise
 * 
 * @note Call display_update() to show changes on screen
 */
display_error_t display_set_pixel(int16_t x, int16_t y, display_color_t color);

/**
 * @brief Draw a line
 * 
 * Uses Bresenham's line algorithm for efficient line drawing.
 * 
 * @param x0 Start X coordinate
 * @param y0 Start Y coordinate
 * @param x1 End X coordinate
 * @param y1 End Y coordinate
 * @param color Line color
 * 
 * @return DISPLAY_OK on success, error code otherwise
 */
display_error_t display_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, 
                                 display_color_t color);

/**
 * @brief Draw a rectangle
 * 
 * @param x Top-left X coordinate
 * @param y Top-left Y coordinate
 * @param w Width in pixels
 * @param h Height in pixels
 * @param color Rectangle color
 * @param filled true to fill rectangle, false for outline only
 * 
 * @return DISPLAY_OK on success, error code otherwise
 */
display_error_t display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                                 display_color_t color, bool filled);

/**
 * @brief Draw a circle
 * 
 * Uses midpoint circle algorithm.
 * 
 * @param x0 Center X coordinate
 * @param y0 Center Y coordinate
 * @param r Radius in pixels
 * @param color Circle color
 * @param filled true to fill circle, false for outline only
 * 
 * @return DISPLAY_OK on success, error code otherwise
 */
display_error_t display_draw_circle(int16_t x0, int16_t y0, int16_t r,
                                   display_color_t color, bool filled);

/**
 * @brief Print text at specified position
 * 
 * @param x Starting X coordinate
 * @param y Starting Y coordinate
 * @param text Null-terminated string to display
 * @param size Text size (TEXT_SIZE_1 to TEXT_SIZE_4)
 * @param color Text color
 * 
 * @return DISPLAY_OK on success, error code otherwise
 * 
 * @note Text wrapping is not automatic. Text exceeding screen width is clipped.
 * 
 * Example:
 * @code
 * display_clear();
 * display_print_text(0, 0, "Hello", TEXT_SIZE_2, COLOR_WHITE);
 * display_print_text(0, 20, "World!", TEXT_SIZE_1, COLOR_WHITE);
 * display_update();
 * @endcode
 */
display_error_t display_print_text(int16_t x, int16_t y, const char *text,
                                   display_text_size_t size, display_color_t color);

/**
 * @brief Print formatted text
 * 
 * Similar to printf, but outputs to the display at specified coordinates.
 * 
 * @param x Starting X coordinate
 * @param y Starting Y coordinate
 * @param size Text size
 * @param color Text color
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 * 
 * @return DISPLAY_OK on success, error code otherwise
 * 
 * Example:
 * @code
 * int count = 42;
 * display_printf(0, 0, TEXT_SIZE_1, COLOR_WHITE, "Count: %d", count);
 * display_update();
 * @endcode
 */
display_error_t display_printf(int16_t x, int16_t y, display_text_size_t size,
                              display_color_t color, const char *format, ...);

/**
 * @brief Set display contrast
 * 
 * Adjusts the display brightness by configuring the SSD1306 contrast control.
 * 
 * @param contrast Contrast value (0-255, where 255 is maximum brightness)
 * 
 * @return DISPLAY_OK on success, error code otherwise
 * 
 * @note Default contrast is 0x7F (mid-range). Per SSD1306 datasheet section 8.1.7.
 */
display_error_t display_set_contrast(uint8_t contrast);

/**
 * @brief Turn display on or off
 * 
 * Controls the SSD1306 display on/off state. When off, the display is blank
 * but the GDDRAM contents are preserved.
 * 
 * @param on true to turn display on, false to turn off
 * 
 * @return DISPLAY_OK on success, error code otherwise
 * 
 * @note Useful for power saving. Per SSD1306 datasheet commands 0xAE/0xAF.
 */
display_error_t display_set_power(bool on);

/**
 * @brief Invert display colors
 * 
 * When inverted, all pixels are XORed (white becomes black, black becomes white).
 * 
 * @param invert true to invert display, false for normal
 * 
 * @return DISPLAY_OK on success, error code otherwise
 */
display_error_t display_invert(bool invert);

/**
 * @brief Get display dimensions
 * 
 * @param width Pointer to store width (can be NULL)
 * @param height Pointer to store height (can be NULL)
 */
void display_get_dimensions(uint16_t *width, uint16_t *height);

/**
 * @brief Deinitialize the display
 * 
 * Turns off display and releases I2C resources.
 * 
 * @return DISPLAY_OK on success, error code otherwise
 */
display_error_t display_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_DISPLAY_H */