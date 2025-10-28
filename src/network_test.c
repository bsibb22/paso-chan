/**
 * @file network_test.c
 * @brief Example application using Network API and SSD1306 Display
 * 
 * This example demonstrates a complete networked application with display output.
 * It replicates the functionality of the original Arduino sketch but uses the
 * new C/C++ APIs built on ESP-IDF.
 * 
 * Features demonstrated:
 * - WiFi connection and automatic reconnection
 * - TCP client communication with server
 * - Display status updates on OLED
 * - Button input handling
 * - Message transmission and reception
 * 
 * Hardware Requirements:
 * - Freenove ESP32-WROOM Development Board
 * - Adafruit SSD1306 0.96" OLED Display (128x64)
 * - Button connected to GPIO 0 (boot button on board)
 * 
 * Connections:
 * - OLED SDA -> GPIO 21
 * - OLED SCL -> GPIO 22
 * - OLED VCC -> 3.3V
 * - OLED GND -> GND
 * - Button -> GPIO 0 (with internal pull-up)
 * 
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "network_api.h"
#include "ssd1306_display.h"

/* ========== Configuration ========== */

/* Network configuration - Update these for your network */
#define WIFI_SSID           "RTKeeny_2ghz"
#define WIFI_PASSWORD       "Banananutmuffin1120"
#define SERVER_IP           "192.168.1.106"
#define SERVER_PORT         8888
#define DEVICE_NAME         "Device1"

/* Button configuration */
#define BUTTON_PIN          GPIO_NUM_0
#define BUTTON_DEBOUNCE_MS  50

/* Message sending interval */
#define HEARTBEAT_INTERVAL_MS 15000  /* 15 seconds */

/* ========== Global Variables ========== */

static const char *TAG = "MAIN";
static uint32_t message_count = 0;
static bool last_button_state = true;  /* Pull-up makes idle state HIGH */

/* ========== Helper Functions ========== */

/**
 * @brief Update display with current status
 * 
 * Shows device name, network state, signal strength, and statistics
 * on the OLED display in a formatted layout.
 */
static void update_display_status(void)
{
    network_state_t state = network_get_state();
    network_stats_t stats;
    int8_t rssi = 0;
    
    network_get_stats(&stats);
    network_get_rssi(&rssi);
    
    /* Clear display */
    display_clear();
    
    /* Title and device name */
    display_print_text(0, 0, DEVICE_NAME, TEXT_SIZE_1, COLOR_WHITE);
    display_draw_line(0, 9, DISPLAY_WIDTH - 1, 9, COLOR_WHITE);
    
    /* Connection status */
    const char *status_text;
    switch (state) {
        case NETWORK_STATE_SERVER_CONNECTED:
            status_text = "CONNECTED";
            break;
        case NETWORK_STATE_WIFI_CONNECTED:
            status_text = "WiFi Only";
            break;
        case NETWORK_STATE_DISCONNECTED:
            status_text = "Disconnected";
            break;
        default:
            status_text = "Error";
            break;
    }
    
    display_print_text(0, 12, "Status:", TEXT_SIZE_1, COLOR_WHITE);
    display_print_text(42, 12, (char *)status_text, TEXT_SIZE_1, COLOR_WHITE);
    
    /* Signal strength */
    if (state >= NETWORK_STATE_WIFI_CONNECTED) {
        display_printf(0, 22, TEXT_SIZE_1, COLOR_WHITE, "RSSI: %d dBm", rssi);
    }
    
    /* Statistics */
    display_printf(0, 32, TEXT_SIZE_1, COLOR_WHITE, "TX: %lu", stats.messages_sent);
    display_printf(0, 42, TEXT_SIZE_1, COLOR_WHITE, "RX: %lu", stats.messages_received);
    
    /* Error count if any */
    if (stats.send_errors > 0) {
        display_printf(0, 52, TEXT_SIZE_1, COLOR_WHITE, "Err: %lu", stats.send_errors);
    }
    
    /* Update display */
    display_update();
}

/**
 * @brief Display a received message
 * 
 * Shows incoming messages on the display.
 * 
 * @param message Message to display
 */
static void display_message(const char *message)
{
    display_clear();
    
    /* Header */
    display_print_text(0, 0, "RECEIVED:", TEXT_SIZE_1, COLOR_WHITE);
    display_draw_line(0, 9, DISPLAY_WIDTH - 1, 9, COLOR_WHITE);
    
    /* Message content - word wrap if needed */
    /* For simplicity, just display first line */
    display_print_text(0, 16, (char *)message, TEXT_SIZE_2, COLOR_WHITE);
    
    display_update();
    
    /* Keep message on screen for 3 seconds, then show status */
    vTaskDelay(pdMS_TO_TICKS(3000));
    update_display_status();
}

/**
 * @brief Network message callback
 * 
 * This function is called by the network task when a message is received
 * from the server. It logs the message and updates the display.
 * 
 * @param message Received message string
 * @param length Message length
 * @param user_data User context (not used in this example)
 */
static void on_message_received(const char *message, size_t length, void *user_data)
{
    ESP_LOGI(TAG, "Message received: %s", message);
    display_message(message);
}

/**
 * @brief Button handling task
 * 
 * Monitors button state and sends a message when pressed.
 * Uses debouncing to avoid multiple triggers from a single press.
 * 
 * @param pvParameters Task parameters (unused)
 */
static void button_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Button task started");
    
    while (1) {
        bool button_state = gpio_get_level(BUTTON_PIN);
        
        /* Detect falling edge (button press) with debouncing */
        if (button_state == 0 && last_button_state == 1) {
            /* Button pressed */
            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
            
            /* Verify button still pressed after debounce */
            if (gpio_get_level(BUTTON_PIN) == 0) {
                ESP_LOGI(TAG, "Button pressed");
                
                /* Send message to server */
                char msg[64];
                snprintf(msg, sizeof(msg), "Button pressed! Count: %lu", message_count++);
                
                network_error_t err = network_send_message(msg);
                if (err == NETWORK_OK) {
                    ESP_LOGI(TAG, "Message queued: %s", msg);
                    
                    /* Brief visual feedback on display */
                    display_clear();
                    display_print_text(10, 24, "Button!", TEXT_SIZE_3, COLOR_WHITE);
                    display_update();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    update_display_status();
                } else {
                    ESP_LOGE(TAG, "Failed to queue message: %d", err);
                }
            }
        }
        
        last_button_state = button_state;
        vTaskDelay(pdMS_TO_TICKS(10));  /* Poll every 10ms */
    }
}

/**
 * @brief Heartbeat task
 * 
 * Periodically sends a heartbeat message to keep connection alive and
 * provides a way to monitor device health from the server side.
 * 
 * @param pvParameters Task parameters (unused)
 */
static void heartbeat_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Heartbeat task started");
    
    uint32_t heartbeat_count = 0;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        
        if (network_get_state() == NETWORK_STATE_SERVER_CONNECTED) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Heartbeat #%lu", heartbeat_count++);
            
            network_error_t err = network_send_message(msg);
            if (err == NETWORK_OK) {
                ESP_LOGI(TAG, "Heartbeat sent: %lu", heartbeat_count);
            } else {
                ESP_LOGW(TAG, "Heartbeat send failed");
            }
        }
    }
}

/**
 * @brief Status update task
 * 
 * Periodically refreshes the display with current status information.
 * 
 * @param pvParameters Task parameters (unused)
 */
static void status_update_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Status update task started");
    
    while (1) {
        update_display_status();
        vTaskDelay(pdMS_TO_TICKS(2000));  /* Update every 2 seconds */
    }
}

/* ========== Main Application ========== */

/**
 * @brief Application entry point
 * 
 * Initializes all subsystems and starts the application tasks.
 * The function demonstrates proper initialization order and error handling.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32 Network + Display Application ===");
    ESP_LOGI(TAG, "Device: %s", DEVICE_NAME);
    ESP_LOGI(TAG, "Compiled: %s %s", __DATE__, __TIME__);
    
    /* ========== Hardware Initialization ========== */
    
    /* Configure button GPIO */
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&button_conf);
    
    /* Initialize display */
    ESP_LOGI(TAG, "Initializing display...");
    display_config_t display_conf = {
        .sda_pin = DISPLAY_DEFAULT_SDA_PIN,
        .scl_pin = DISPLAY_DEFAULT_SCL_PIN,
        .i2c_addr = DISPLAY_DEFAULT_I2C_ADDR,
        .i2c_freq_hz = 400000
    };
    
    display_error_t disp_err = display_init(&display_conf);
    if (disp_err != DISPLAY_OK) {
        ESP_LOGE(TAG, "Display initialization failed: %d", disp_err);
        return;
    }
    
    /* Show startup screen */
    display_clear();
    display_print_text(0, 0, "ESP32 Network", TEXT_SIZE_1, COLOR_WHITE);
    display_print_text(0, 10, "Application", TEXT_SIZE_1, COLOR_WHITE);
    display_draw_line(0, 20, DISPLAY_WIDTH - 1, 20, COLOR_WHITE);
    display_print_text(0, 24, DEVICE_NAME, TEXT_SIZE_2, COLOR_WHITE);
    display_print_text(0, 48, "Starting...", TEXT_SIZE_1, COLOR_WHITE);
    display_update();
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    /* ========== Network Initialization ========== */
    
    ESP_LOGI(TAG, "Initializing network...");
    
    network_config_t net_config = {
        .wifi_ssid = WIFI_SSID,
        .wifi_password = WIFI_PASSWORD,
        .server_ip = SERVER_IP,
        .server_port = SERVER_PORT,
        .device_name = DEVICE_NAME,
        .reconnect_interval_ms = 5000
    };
    
    network_error_t net_err = network_init(&net_config, on_message_received, NULL);
    if (net_err != NETWORK_OK) {
        ESP_LOGE(TAG, "Network initialization failed: %d", net_err);
        display_clear();
        display_print_text(0, 24, "Init Failed!", TEXT_SIZE_2, COLOR_WHITE);
        display_update();
        return;
    }
    
    /* Show connecting status */
    display_clear();
    display_print_text(0, 20, "Connecting", TEXT_SIZE_2, COLOR_WHITE);
    display_print_text(0, 40, "to WiFi...", TEXT_SIZE_2, COLOR_WHITE);
    display_update();
    
    /* Start network */
    net_err = network_start();
    if (net_err != NETWORK_OK) {
        ESP_LOGE(TAG, "Network start failed: %d", net_err);
        display_clear();
        display_print_text(0, 24, "Connect Failed", TEXT_SIZE_1, COLOR_WHITE);
        display_update();
        return;
    }
    
    ESP_LOGI(TAG, "Network started successfully");
    
    /* ========== Task Creation ========== */
    
    /* Create button handling task */
    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
    
    /* Create heartbeat task */
    xTaskCreate(heartbeat_task, "heartbeat_task", 2048, NULL, 4, NULL);
    
    /* Create status update task */
    xTaskCreate(status_update_task, "status_update_task", 3072, NULL, 3, NULL);
    
    ESP_LOGI(TAG, "Application started successfully");
    ESP_LOGI(TAG, "Press the BOOT button to send a test message");
    
    /* Main task can now terminate as all work is done in other tasks */
    /* In ESP-IDF, app_main can return and the system continues running */
}