/**
 * @file network_api.h
 * @brief ESP32 TCP Client Networking API
 * 
 * This module provides a simple, thread-safe networking interface for ESP32-WROOM
 * devices. It abstracts WiFi connection management and TCP client operations,
 * making it easy for team members to integrate network communication into their
 * applications without dealing with low-level ESP-IDF networking components.
 * 
 * Key Features:
 * - Automatic WiFi connection and reconnection
 * - Non-blocking TCP client operations
 * - Thread-safe message queue for sending/receiving
 * - Built-in error handling and logging
 * - Simple callback-based message reception
 * 
 * Hardware: Freenove ESP32-WROOM Development Board
 * 
 * References:
 * - ESP-IDF WiFi API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html
 * - ESP-IDF TCP/IP Adapter: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_netif.html
 * 
 */

#ifndef NETWORK_API_H
#define NETWORK_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum length for network messages
 */
#define NETWORK_MAX_MESSAGE_LEN 512

/**
 * @brief Network connection states
 * 
 * These states represent the current status of the network connection,
 * allowing the application to respond appropriately to connection changes.
 */
typedef enum {
    NETWORK_STATE_DISCONNECTED = 0,  /**< Not connected to WiFi or server */
    NETWORK_STATE_WIFI_CONNECTED,    /**< Connected to WiFi, not to server */
    NETWORK_STATE_SERVER_CONNECTED,  /**< Fully connected to server */
    NETWORK_STATE_ERROR              /**< Error state requiring reset */
} network_state_t;

/**
 * @brief Network error codes
 */
typedef enum {
    NETWORK_OK = 0,                  /**< Operation successful */
    NETWORK_ERR_NOT_INITIALIZED,     /**< Network module not initialized */
    NETWORK_ERR_WIFI_FAILED,         /**< WiFi connection failed */
    NETWORK_ERR_SERVER_FAILED,       /**< Server connection failed */
    NETWORK_ERR_SEND_FAILED,         /**< Message send failed */
    NETWORK_ERR_INVALID_PARAM,       /**< Invalid parameter provided */
    NETWORK_ERR_QUEUE_FULL,          /**< Message queue is full */
    NETWORK_ERR_TIMEOUT              /**< Operation timed out */
} network_error_t;

/**
 * @brief Network configuration structure
 * 
 * Contains all necessary parameters for WiFi and server connection.
 * Initialize this structure and pass it to network_init().
 */
typedef struct {
    const char *wifi_ssid;           /**< WiFi network SSID */
    const char *wifi_password;       /**< WiFi network password */
    const char *server_ip;           /**< Server IP address (e.g., "192.168.1.100") */
    uint16_t server_port;            /**< Server TCP port number */
    const char *device_name;         /**< Unique device identifier */
    uint32_t reconnect_interval_ms;  /**< Milliseconds between reconnection attempts */
} network_config_t;

/**
 * @brief Callback function type for received messages
 * 
 * This callback is invoked when a message is received from the server.
 * The callback executes in the context of the network task, so keep
 * processing minimal or delegate to another task.
 * 
 * @param message Null-terminated received message string
 * @param length Length of the message (excluding null terminator)
 * @param user_data User-provided context pointer (set in network_init)
 * 
 * @note The message buffer is only valid during the callback.
 *       Copy data if needed for later use.
 */
typedef void (*network_message_callback_t)(const char *message, 
                                           size_t length, 
                                           void *user_data);

/**
 * @brief Initialize the networking subsystem
 * 
 * This function must be called before any other network API functions.
 * It initializes the WiFi stack, creates internal tasks, and prepares
 * the system for connection.
 * 
 * @param config Pointer to network configuration structure
 * @param callback Message reception callback function (can be NULL)
 * @param user_data User context pointer passed to callback (can be NULL)
 * 
 * @return NETWORK_OK on success, error code otherwise
 * 
 * @note This function initializes NVS (Non-Volatile Storage) flash,
 *       which is required by the WiFi stack.
 * 
 * Example:
 * @code
 * network_config_t config = {
 *     .wifi_ssid = "MyWiFi",
 *     .wifi_password = "password123",
 *     .server_ip = "192.168.1.100",
 *     .server_port = 8888,
 *     .device_name = "ESP32_Device1",
 *     .reconnect_interval_ms = 5000
 * };
 * network_init(&config, my_message_handler, NULL);
 * @endcode
 */
network_error_t network_init(const network_config_t *config,
                            network_message_callback_t callback,
                            void *user_data);

/**
 * @brief Start network connection
 * 
 * Initiates WiFi connection and subsequent server connection.
 * This function is non-blocking; connection status can be monitored
 * via network_get_state() or the status will be logged.
 * 
 * @return NETWORK_OK on success, error code otherwise
 * 
 * @note Automatic reconnection is handled internally if connection drops.
 */
network_error_t network_start(void);

/**
 * @brief Stop network connection
 * 
 * Gracefully disconnects from server and WiFi.
 * 
 * @return NETWORK_OK on success, error code otherwise
 */
network_error_t network_stop(void);

/**
 * @brief Send a message to the server
 * 
 * Queues a message for transmission to the server. The message is sent
 * asynchronously by the network task. A newline character is automatically
 * appended if not present.
 * 
 * @param message Null-terminated message string
 * 
 * @return NETWORK_OK if queued successfully, error code otherwise
 * 
 * @note Maximum message length is NETWORK_MAX_MESSAGE_LEN bytes.
 * @note Function returns immediately; message is sent asynchronously.
 * 
 * Example:
 * @code
 * network_send_message("Button pressed!");
 * @endcode
 */
network_error_t network_send_message(const char *message);

/**
 * @brief Get current network connection state
 * 
 * @return Current network_state_t value
 * 
 * Example:
 * @code
 * if (network_get_state() == NETWORK_STATE_SERVER_CONNECTED) {
 *     network_send_message("Hello Server!");
 * }
 * @endcode
 */
network_state_t network_get_state(void);

/**
 * @brief Get the current WiFi RSSI (signal strength)
 * 
 * @param rssi Pointer to store RSSI value in dBm
 * 
 * @return NETWORK_OK on success, error code otherwise
 * 
 * @note RSSI values typically range from -30 (excellent) to -90 (poor) dBm
 */
network_error_t network_get_rssi(int8_t *rssi);

/**
 * @brief Get network statistics
 * 
 * Provides transmission statistics for monitoring and debugging.
 */
typedef struct {
    uint32_t messages_sent;          /**< Total messages successfully sent */
    uint32_t messages_received;      /**< Total messages received */
    uint32_t send_errors;            /**< Number of send failures */
    uint32_t reconnect_count;        /**< Number of reconnection attempts */
} network_stats_t;

/**
 * @brief Get network statistics
 * 
 * @param stats Pointer to structure to fill with statistics
 * 
 * @return NETWORK_OK on success, error code otherwise
 */
network_error_t network_get_stats(network_stats_t *stats);

/**
 * @brief Deinitialize the networking subsystem
 * 
 * Cleans up all network resources. Call this before application shutdown.
 * 
 * @return NETWORK_OK on success, error code otherwise
 */
network_error_t network_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_API_H */