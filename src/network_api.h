/*
 * Simple network communication library for Paso-chan
 * 
 * This library allows easy connection to WiFi and send/receive 
 * messages to/from the relay server. It handles all the complex networking stuff
 * automatically, including:
 * - Connecting to WiFi and reconnecting if the connection drops
 * - Connecting to a server and maintaining the connection
 * - Sending messages reliably
 * - Receiving messages through a simple callback function. Callbacks are functions that run when an event happens
 * 
 * To use this library:
 * 1. Call network_init() with your WiFi and server details
 * 2. Call network_start() to connect
 * 3. Use network_send_message() to send data
 * 4. Receive data through your callback function
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

/* 
 * The possible connection states your device can be in.
 * Check this to see if you're fully connected and ready to send messages.
 */
typedef enum {
    NETWORK_STATE_DISCONNECTED = 0,  /**< Not connected to WiFi or server */
    NETWORK_STATE_WIFI_CONNECTED,    /**< Connected to WiFi, not to server */
    NETWORK_STATE_SERVER_CONNECTED,  /**< Fully connected to server */
    NETWORK_STATE_ERROR              /**< Error state requiring reset */
} network_state_t;

/*
 * Error codes that functions can return. Use these to check if operations succeeded.
 * - NETWORK_OK: Everything worked fine
 * - NETWORK_ERR_WIFI_FAILED: Couldn't connect to WiFi
 * - NETWORK_ERR_SERVER_FAILED: Couldn't connect to the server
 * - NETWORK_ERR_SEND_FAILED: Message couldn't be sent
 * - NETWORK_ERR_QUEUE_FULL: Too many messages waiting to be sent
 */
typedef enum {
    NETWORK_OK = 0,                  /* Everything worked */
    NETWORK_ERR_NOT_INITIALIZED,     /* Call network_init() first */
    NETWORK_ERR_WIFI_FAILED,         /* WiFi connection failed */
    NETWORK_ERR_SERVER_FAILED,       /* Server connection failed */
    NETWORK_ERR_SEND_FAILED,         /* Couldn't send message */
    NETWORK_ERR_INVALID_PARAM,       /* You provided invalid settings */
    NETWORK_ERR_QUEUE_FULL,          /* Too many messages queued */
    NETWORK_ERR_TIMEOUT              /* Operation took too long */
} network_error_t;

/*
 * Settings needed to connect to your WiFi network and server.
 * Fill this out and pass it to network_init() to start using the network.
 */
typedef struct {
    const char *wifi_ssid;           /* Name of your WiFi network */
    const char *wifi_password;       /* Password for your WiFi network */
    const char *server_ip;          /* IP address of your server (e.g., "192.168.1.100") */
    uint16_t server_port;           /* Port number your server is listening on */
    const char *device_name;        /* Name to identify this device to the server */
    uint32_t reconnect_interval_ms; /* How often to retry if connection is lost (in ms) */
} network_config_t;

/*
 * This is the type of function you need to write to handle incoming messages.
 * Your function will be called automatically whenever a message arrives.
 * 
 * Your function should look like this:
 * void my_message_handler(const char *message, size_t length, void *user_data) {
 *     // Handle the message here
 *     printf("Got message: %s\n", message);
 * }
 * 
 * Important:
 * - message: The text that was received (ends with \0)
 * - length: How many characters are in the message
 * - user_data: Any extra data you provided to network_init()
 * 
 * Note: If you need to save the message for later, make a copy of it!
 * The message buffer will be reused after your function returns.
 */
typedef void (*network_message_callback_t)(const char *message, 
                                           size_t length, 
                                           void *user_data);

/*
 * Sets up the network system for the device. Call this first!
 *
 * Example use:
 * network_config_t config = {
 *     .wifi_ssid = "MyWiFi",
 *     .wifi_password = "password123",
 *     .server_ip = "192.168.1.100",
 *     .server_port = 8888,
 *     .device_name = "ESP32_Device1",
 *     .reconnect_interval_ms = 5000
 * };
 * 
 * void handle_message(const char *msg, size_t len, void *data) {
 *     printf("Got message: %s\n", msg);
 * }
 * 
 * network_init(&config, handle_message, NULL);
 * 
 * Returns NETWORK_OK if setup was successful, or an error code if something went wrong.
 */
network_error_t network_init(const network_config_t *config,
                            network_message_callback_t callback,
                            void *user_data);

/*
 * Start the network connection. Call this after network_init().
 * The function returns right away and connection happens in the background.
 * Use network_get_state() to check when you're fully connected.
 * 
 * If the connection drops, it will automatically try to reconnect.
 * Returns NETWORK_OK if started successfully.
 */
network_error_t network_start(void);

/*
 * Disconnect from the network cleanly. 
 * Use this when you're done or need to restart the connection.
 * Returns NETWORK_OK when successfully disconnected.
 */
network_error_t network_stop(void);

/*
 * Send a text message to the server. Message will be delivered reliably
 * and a newline is automatically added. Returns immediately after queuing
 * the message (doesn't wait for it to be sent).
 * 
 * Example:
 *   network_send_message("Button pressed!");
 * 
 * Note: Maximum message length is NETWORK_MAX_MESSAGE_LEN characters.
 * Returns NETWORK_OK if the message was queued successfully.
 */
network_error_t network_send_message(const char *message);

/*
 * Check if connected and ready to send messages.
 * 
 * Example:
 * if (network_get_state() == NETWORK_STATE_SERVER_CONNECTED) {
 *     // We're connected! Safe to send messages
 *     network_send_message("Hello!");
 * } else {
 *     // Not ready yet, wait for connection
 * }
 */
network_state_t network_get_state(void);

/*
 * Check the WiFi signal strength.
 * Writes the signal strength to the rssi pointer provided.
 * 
 * The number will be between -30 (perfect signal) and -90 (very weak).
 * Anything better than -70 is usually good enough.
 * 
 * Example:
 * int8_t signal_strength;
 * if (network_get_rssi(&signal_strength) == NETWORK_OK) {
 *     printf("WiFi signal strength: %d\n", signal_strength);
 * }
 */
network_error_t network_get_rssi(int8_t *rssi);

/*
 * Statistics about network activity - useful for debugging.
 * These counters track how many messages have been sent/received
 * and if there were any problems.
 */
typedef struct {
    uint32_t messages_sent;      /* How many messages sent successfully */
    uint32_t messages_received;  /* How many messages  received */
    uint32_t send_errors;        /* Number of failed send attempts */
    uint32_t reconnect_count;    /* How many times had to reconnect */
} network_stats_t;

/*
 * Get statistics about network activity.
 * Pass in a network_stats_t variable and it will be filled with the current counts.
 * 
 * Example:
 * network_stats_t stats;
 * if (network_get_stats(&stats) == NETWORK_OK) {
 *     printf("Sent %d messages, received %d\n", 
 *            stats.messages_sent, stats.messages_received);
 * }
 */
network_error_t network_get_stats(network_stats_t *stats);

/*
 * Clean up the network system.
 * Call this when your program is shutting down to release all resources.
 * After calling this, you'll need to call network_init() again if you
 * want to use the network.
 */
network_error_t network_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_API_H */