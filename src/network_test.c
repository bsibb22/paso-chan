/*
 * Test application that demonstrates how to use the network API together
 * with an SSD1306 display. This is a simple example that shows:
 * - Bringing up WiFi and the network client
 * - Sending periodic heartbeats to the server
 * - Sending a message when the user presses the BOOT button
 * - Updating an OLED display with connection status and stats
 *
 * This is intended to be helpful for understanding the
 * normal initialization flow and task structure used with the network
 * module and an RTOS-based application.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "network_api.h"
#include "ssd1306_display.h"


/* Network configuration - hardcoded for right now */
#define WIFI_SSID           "RTKeeny2ghz"
#define WIFI_PASSWORD       "Banananutmuffin1120"
#define SERVER_IP           "192.168.1.106"
#define SERVER_PORT         8888
#define DEVICE_NAME         "Device2"

/* Button configuration */
#define BUTTON_PIN          GPIO_NUM_0
#define BUTTON_DEBOUNCE_MS  50

/* Message sending interval */
#define HEARTBEAT_INTERVAL_MS 15000  /* 15 seconds */

/* Global vars */

static const char *TAG = "MAIN";
static uint32_t message_count = 0;
static bool last_button_state = true;  /* Pull-up for idle state HIGH */

/* ========== Helper Functions ========== */

/*
 * Render current status on the OLED.
 *
 * What this does:
 * - Queries the network module for state, RSSI and stats
 * - Clears and redraws the display with formatted information
 *
 * Notes for readers:
 * - This function is safe to call from a task context. It performs
 *   only short blocking operations (display primitives) and is not
 *   timing-critical.
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

/*
 * Show an incoming message on the display for a short time.
 *
 * This replaces the normal status view while the message is shown, waits
 * three seconds, then restores the status screen. The display routines are
 * simple and blocking, intended for user feedback rather than high-speed IO.
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

/*
 * Callback invoked by the network module whenever a newline-delimited
 * message arrives from the server.
 *
 * Important details:
 * - This callback runs in the context of the network task. Keep it quick.
 * - Here we log the message and show it on the display. For heavier work
 *   (parsing, storing), hand off to another task or queue.
 */
static void on_message_received(const char *message, size_t length, void *user_data)
{
    ESP_LOGI(TAG, "Message received: %s", message);
    display_message(message);
}

/*
 * Task that polls the BOOT button and sends a message on press.
 *
 * Implementation notes:
 * - Uses simple polling with a 10 ms loop and a short debounce delay.
 * - On a detected falling edge we wait BUTTON_DEBOUNCE_MS and re-check
 *   to filter bounces.
 * - The task calls network_send_message() which queues the message.
 *   network_send_message() is non-blocking; it will return an error if
 *   the transmit queue is full.
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

/*
 * Periodically send a heartbeat message to the server.
 *
 * Notes:
 * - Sleeps for HEARTBEAT_INTERVAL_MS between attempts.
 * - Only sends if the network reports that we're connected to the server.
 * - network_send_message() is used; the function logs a warning if queuing
 *   the heartbeat fails (queue full or not initialized).
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

/*
 * Periodically refresh the OLED with current status.
 *
 * This runs every 2s and calls update_display_status().
 * Keep the work here small so updates don't block other tasks.
 */
static void status_update_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Status update task started");
    
    while (1) {
        update_display_status();
        vTaskDelay(pdMS_TO_TICKS(2000));  /* Update every 2 seconds */
    }
}

/*
 * Main application.
 *
 * Initialization order:
 * 1. Configure hardware (GPIO, display)
 * 2. Initialize display so we can show status/errors
 * 3. Initialize the network module (network_init)
 * 4. Start the network (network_start) which will attempt WiFi connect
 * 5. Create application tasks (button, heartbeat, status)
 *
 * If initialization fails, update the display and return so the error
 * is visible. After creating tasks the main function returns. FreeRTOS
 * keeps the other tasks running.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Network Test Starting");
    ESP_LOGI(TAG, "Device: %s", DEVICE_NAME);
    ESP_LOGI(TAG, "Compiled: %s %s", __DATE__, __TIME__);
        
    /* Config button GPIO */
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&button_conf);
    
    /* Init display */
    ESP_LOGI(TAG, "Initializing display...");
    display_config_t display_conf = {
        .sda_pin = DISPLAY_DEFAULT_SDA_PIN,
        .scl_pin = DISPLAY_DEFAULT_SCL_PIN,
        .i2c_addr = DISPLAY_DEFAULT_I2C_ADDR,
        .i2c_freq_hz = 400000
    };
    
    display_error_t disp_err = display_init(&display_conf);
    if (disp_err != DISPLAY_OK) {
        ESP_LOGE(TAG, "Display init failed: %d", disp_err);
        return;
    }
    
    /* Show startup screen */
    display_clear();
    display_print_text(0, 0, "Paso Network", TEXT_SIZE_1, COLOR_WHITE);
    display_print_text(0, 10, "Application", TEXT_SIZE_1, COLOR_WHITE);
    display_draw_line(0, 20, DISPLAY_WIDTH - 1, 20, COLOR_WHITE);
    display_print_text(0, 24, DEVICE_NAME, TEXT_SIZE_2, COLOR_WHITE);
    display_print_text(0, 48, "Starting...", TEXT_SIZE_1, COLOR_WHITE);
    display_update();
    
    vTaskDelay(pdMS_TO_TICKS(2000));
        
    ESP_LOGI(TAG, "Init network...");
    
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
        ESP_LOGE(TAG, "Network init failed: %d", net_err);
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