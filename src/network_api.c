/*
 * This is the core networking implementation for Paso-chan.
 * It runs a background task that handles all network operations:
 * - Manages WiFi connection with auto-reconnect
 * - Maintains TCP connection to the server
 * - Sends/receives messages using queues to be thread-safe
 *
 * The basic architecture:
 * 1. A main network task runs in the background
 * 2. Messages to send go through a fifo (tx_queue)
 * 3. WiFi status is tracked with event bits
 * 4. Everything is protected with mutexes
 */

#include "network_api.h"

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

/* Logging tag for ESP-IDF logging system */
static const char *TAG = "NETWORK_API";

/* WiFi event bits for synchronization using FreeRTOS event groups */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* Network task configuration */
#define NETWORK_TASK_STACK_SIZE 4096
#define NETWORK_TASK_PRIORITY   5
#define NETWORK_QUEUE_SIZE      10
#define NETWORK_RECV_BUFFER_SIZE 1024

/* Maximum number of WiFi connection retry attempts before giving up */
#define WIFI_MAX_RETRY_COUNT 5

/* 
 * Message structure used in the tx_queue.
 * When someone calls network_send_message(), their message goes into
 * one of these and gets queued for sending by the network task.
 */
typedef struct {
    char data[NETWORK_MAX_MESSAGE_LEN];  /* The actual message content */
    size_t length;                       /* Real length of the message */
} network_message_t;

/*
 * Main context structure that holds our network state.
 * Uses a global instance (g_network_ctx) since there's only
 * one network connection to manage.
 */
typedef struct {
    /* Config */
    network_config_t config;
    network_message_callback_t message_callback;
    void *user_data;
    
    /* State */
    network_state_t state;
    int sock_fd;                    /* TCP socket file descriptor */
    bool running;                   /* Task running flag */
    int wifi_retry_count;
    
    /* Stats */
    network_stats_t stats;
    
    /* FreeRTOS primitives */
    TaskHandle_t network_task_handle;
    QueueHandle_t tx_queue;         /* Transmit message fifo */
    EventGroupHandle_t wifi_event_group;
    
    /* Synchronization */
    SemaphoreHandle_t state_mutex;
} network_context_t;

/* Global network context (singleton pattern) */
static network_context_t g_network_ctx = {0};

/* Forward declarations of internal functions */
static void network_task(void *pvParameters);
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
static network_error_t wifi_init_sta(void);
static network_error_t connect_to_server(void);
static void close_socket(void);
static void set_state(network_state_t new_state);

/*
 * This function gets called by the ESP32's WiFi system whenever something happens:
 * - When WiFi starts up (WIFI_EVENT_STA_START)
 * - If we get disconnected (WIFI_EVENT_STA_DISCONNECTED)
 * - When we get an IP address (IP_EVENT_STA_GOT_IP)
 * 
 * Use event bits to signal the main code about WiFi status changes,
 * and retry connections up to WIFI_MAX_RETRY_COUNT times.
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi station started, attempting connection...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (g_network_ctx.wifi_retry_count < WIFI_MAX_RETRY_COUNT) {
            esp_wifi_connect();
            g_network_ctx.wifi_retry_count++;
            ESP_LOGI(TAG, "Retry WiFi connection (%d/%d)", 
                     g_network_ctx.wifi_retry_count, WIFI_MAX_RETRY_COUNT);
        } else {
            xEventGroupSetBits(g_network_ctx.wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "WiFi connection failed after %d attempts", WIFI_MAX_RETRY_COUNT);
        }
        set_state(NETWORK_STATE_DISCONNECTED);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        g_network_ctx.wifi_retry_count = 0;
        xEventGroupSetBits(g_network_ctx.wifi_event_group, WIFI_CONNECTED_BIT);
        set_state(NETWORK_STATE_WIFI_CONNECTED);
    }
}

/*
 * Sets up the ESP32's WiFi in station (client) mode.
 * Steps:
 * 1. Initialize the TCP/IP stack and event system
 * 2. Setup the WiFi driver with our SSID/password
 * 3. Register our event handler to know when we connect/disconnect
 * 4. Start the WiFi and wait for connection
 * 
 * Use an event group to wait for either successful connection
 * or a failure after max retries.
 */
static network_error_t wifi_init_sta(void)
{
    /* Init TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    /* Init WiFi with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Setup event handlers for WiFi and IP events */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    /* Config WiFi credentials */
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    /* Copy SSID and password safely (literally OS p1)*/
    strncpy((char *)wifi_config.sta.ssid, g_network_ctx.config.wifi_ssid, 
            sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, g_network_ctx.config.wifi_password, 
            sizeof(wifi_config.sta.password) - 1);

    /* Set WiFi mode to station and apply config */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization complete. Connecting to SSID: %s", 
             g_network_ctx.config.wifi_ssid);

    /* Wait for connection or failure */
    EventBits_t bits = xEventGroupWaitBits(g_network_ctx.wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi SSID: %s", g_network_ctx.config.wifi_ssid);
        return NETWORK_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi SSID: %s", g_network_ctx.config.wifi_ssid);
        return NETWORK_ERR_WIFI_FAILED;
    }

    return NETWORK_ERR_WIFI_FAILED;
}

/*
 * Establishes the TCP connection to our server after WiFi is connected.
 * Standard socket stuff here:
 * 1. Create a TCP socket
 * 2. Convert server IP from string to binary
 * 3. Connect to server:port
 * 4. Send our device name so server knows who we are
 * 
 * If anything fails, close the socket and try again later.
 */
static network_error_t connect_to_server(void)
{
    struct sockaddr_in server_addr;
    
    /* Create TCP socket */
    g_network_ctx.sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_network_ctx.sock_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return NETWORK_ERR_SERVER_FAILED;
    }

    /* Configure server address structure */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_network_ctx.config.server_port);
    
    /* Convert IP address string to binary form */
    if (inet_pton(AF_INET, g_network_ctx.config.server_ip, &server_addr.sin_addr) <= 0) {
        ESP_LOGE(TAG, "Invalid server IP address: %s", g_network_ctx.config.server_ip);
        close_socket();
        return NETWORK_ERR_SERVER_FAILED;
    }

    ESP_LOGI(TAG, "Connecting to server %s:%d...", 
             g_network_ctx.config.server_ip, g_network_ctx.config.server_port);

    /* Attempt connection to server */
    if (connect(g_network_ctx.sock_fd, (struct sockaddr *)&server_addr, 
                sizeof(server_addr)) != 0) {
        ESP_LOGE(TAG, "Failed to connect to server: errno %d", errno);
        close_socket();
        return NETWORK_ERR_SERVER_FAILED;
    }

    ESP_LOGI(TAG, "Successfully connected to server");
    set_state(NETWORK_STATE_SERVER_CONNECTED);
    
    /* Send device identification message */
    char hello_msg[128];
    snprintf(hello_msg, sizeof(hello_msg), "DEVICE:%s\n", g_network_ctx.config.device_name);
    send(g_network_ctx.sock_fd, hello_msg, strlen(hello_msg), 0);
    
    return NETWORK_OK;
}

/**
 * Close TCP socket safely
 */
static void close_socket(void)
{
    if (g_network_ctx.sock_fd >= 0) {
        shutdown(g_network_ctx.sock_fd, SHUT_RDWR);
        close(g_network_ctx.sock_fd);
        g_network_ctx.sock_fd = -1;
    }
}

/**
 * Set network state with mutex protection, take is decrement and give is increment
 */
static void set_state(network_state_t new_state)
{
    xSemaphoreTake(g_network_ctx.state_mutex, portMAX_DELAY);
    g_network_ctx.state = new_state;
    xSemaphoreGive(g_network_ctx.state_mutex);
}

/*
 * This is the workhorse task that runs in the background handling all network I/O.
 * Main loop:
 * 1. If not connected to server, try to connect
 * 2. Use select() to check for incoming data (with 100ms timeout)
 * 3. If data arrived, read it and look for complete messages (ended by \n)
 * 4. Check tx_queue for messages to send
 * 5. Handle any errors by reconnecting
 * 
 * Uses non-blocking I/O so it can handle both sending and receiving.
 * If the server disconnects or there's an error, auto-reconnect.
 */
static void network_task(void *pvParameters)
{
    network_message_t tx_msg;
    char rx_buffer[NETWORK_RECV_BUFFER_SIZE];
    fd_set read_fds;
    struct timeval timeout;
    int max_fd;
    
    ESP_LOGI(TAG, "Network task started");
    
    while (g_network_ctx.running) {
        /* Check if server connection is established */
        if (g_network_ctx.state != NETWORK_STATE_SERVER_CONNECTED) {
            /* Attempt to reconnect */
            if (g_network_ctx.state == NETWORK_STATE_WIFI_CONNECTED) {
                ESP_LOGI(TAG, "Attempting server connection...");
                if (connect_to_server() == NETWORK_OK) {
                    g_network_ctx.stats.reconnect_count++;
                }
            }
            /* Wait before retry */
            vTaskDelay(pdMS_TO_TICKS(g_network_ctx.config.reconnect_interval_ms));
            continue;
        }
        
        /* Set up select() for non-blocking socket monitoring */
        FD_ZERO(&read_fds);
        FD_SET(g_network_ctx.sock_fd, &read_fds);
        max_fd = g_network_ctx.sock_fd + 1;
        
        /* Short timeout to allow checking the transmit fifo frequently */
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  /* 100ms */
        
        int ret = select(max_fd, &read_fds, NULL, NULL, &timeout);
        
        if (ret > 0 && FD_ISSET(g_network_ctx.sock_fd, &read_fds)) {
            /* Data available to read */
            int len = recv(g_network_ctx.sock_fd, rx_buffer, 
                          sizeof(rx_buffer) - 1, 0);
            
            if (len > 0) {
                rx_buffer[len] = '\0';  /* Null-terminate */
                
                /* Process received data - look for newline-delimited messages */
                char *line_start = rx_buffer;
                char *newline;
                
                while ((newline = strchr(line_start, '\n')) != NULL) {
                    *newline = '\0';  /* Replace newline with null terminator */
                    size_t msg_len = newline - line_start;
                    
                    if (msg_len > 0) {
                        ESP_LOGI(TAG, "Received: %s", line_start);
                        g_network_ctx.stats.messages_received++;
                        
                        /* Invoke callback if registered */
                        if (g_network_ctx.message_callback) {
                            g_network_ctx.message_callback(line_start, msg_len, 
                                                          g_network_ctx.user_data);
                        }
                    }
                    
                    line_start = newline + 1;
                }
            } else if (len == 0) {
                /* Server closed connection */
                ESP_LOGW(TAG, "Server closed connection");
                close_socket();
                set_state(NETWORK_STATE_WIFI_CONNECTED);
                continue;
            } else {
                /* Error occurred */
                ESP_LOGE(TAG, "Socket recv error: errno %d", errno);
                close_socket();
                set_state(NETWORK_STATE_WIFI_CONNECTED);
                continue;
            }
        }
        
        /* Check for messages to send */
        if (xQueueReceive(g_network_ctx.tx_queue, &tx_msg, 0) == pdTRUE) {
            /* Ensure message ends with newline */
            if (tx_msg.data[tx_msg.length - 1] != '\n') {
                if (tx_msg.length < NETWORK_MAX_MESSAGE_LEN - 1) {
                    tx_msg.data[tx_msg.length++] = '\n';
                    tx_msg.data[tx_msg.length] = '\0';
                }
            }
            
            /* Send message */
            int sent = send(g_network_ctx.sock_fd, tx_msg.data, tx_msg.length, 0);
            if (sent < 0) {
                ESP_LOGE(TAG, "Failed to send message: errno %d", errno);
                g_network_ctx.stats.send_errors++;
                close_socket();
                set_state(NETWORK_STATE_WIFI_CONNECTED);
            } else {
                ESP_LOGI(TAG, "Sent: %s", tx_msg.data);
                g_network_ctx.stats.messages_sent++;
            }
        }
    }
    
    ESP_LOGI(TAG, "Network task stopping");
    vTaskDelete(NULL);
}

network_error_t network_init(const network_config_t *config,
                            network_message_callback_t callback,
                            void *user_data)
{
    if (!config || !config->wifi_ssid || !config->server_ip) {
        ESP_LOGE(TAG, "Invalid configuration parameters");
        return NETWORK_ERR_INVALID_PARAM;
    }
    
    /* Initialize NVS (Non-Volatile Storage) which is needed for WiFi */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    /* Copy configuration */
    memcpy(&g_network_ctx.config, config, sizeof(network_config_t));
    g_network_ctx.message_callback = callback;
    g_network_ctx.user_data = user_data;
    
    /* Initialize state */
    g_network_ctx.state = NETWORK_STATE_DISCONNECTED;
    g_network_ctx.sock_fd = -1;
    g_network_ctx.running = false;
    memset(&g_network_ctx.stats, 0, sizeof(network_stats_t));
    
    /* Create FreeRTOS primitives */
    g_network_ctx.tx_queue = xQueueCreate(NETWORK_QUEUE_SIZE, 
                                         sizeof(network_message_t));
    g_network_ctx.wifi_event_group = xEventGroupCreate();
    g_network_ctx.state_mutex = xSemaphoreCreateMutex();
    
    if (!g_network_ctx.tx_queue || !g_network_ctx.wifi_event_group || 
        !g_network_ctx.state_mutex) {
        ESP_LOGE(TAG, "Failed to create FreeRTOS primitives");
        return NETWORK_ERR_NOT_INITIALIZED;
    }
    
    ESP_LOGI(TAG, "Network module initialized");
    return NETWORK_OK;
}

network_error_t network_start(void)
{
    if (g_network_ctx.running) {
        ESP_LOGW(TAG, "Network already started");
        return NETWORK_OK;
    }
    
    /* Initialize WiFi */
    network_error_t err = wifi_init_sta();
    if (err != NETWORK_OK) {
        return err;
    }
    
    /* Create network task/thread */
    g_network_ctx.running = true;
    BaseType_t ret = xTaskCreate(network_task,
                                 "network_task",
                                 NETWORK_TASK_STACK_SIZE,
                                 NULL,
                                 NETWORK_TASK_PRIORITY,
                                 &g_network_ctx.network_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create network task");
        g_network_ctx.running = false;
        return NETWORK_ERR_NOT_INITIALIZED;
    }
    
    ESP_LOGI(TAG, "Network started");
    return NETWORK_OK;
}

network_error_t network_stop(void)
{
    if (!g_network_ctx.running) {
        return NETWORK_OK;
    }
    
    g_network_ctx.running = false;
    close_socket();
    
    /* Wait for task to terminate */
    vTaskDelay(pdMS_TO_TICKS(500));
    
    /* Disconnect WiFi */
    esp_wifi_disconnect();
    esp_wifi_stop();
    
    ESP_LOGI(TAG, "Network stopped");
    return NETWORK_OK;
}

network_error_t network_send_message(const char *message)
{
    if (!message) {
        return NETWORK_ERR_INVALID_PARAM;
    }
    
    size_t len = strlen(message);
    if (len == 0 || len >= NETWORK_MAX_MESSAGE_LEN) {
        return NETWORK_ERR_INVALID_PARAM;
    }
    
    network_message_t tx_msg;
    strncpy(tx_msg.data, message, NETWORK_MAX_MESSAGE_LEN - 1);
    tx_msg.data[NETWORK_MAX_MESSAGE_LEN - 1] = '\0';
    tx_msg.length = strlen(tx_msg.data);
    
    if (xQueueSend(g_network_ctx.tx_queue, &tx_msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "TX queue full, message dropped");
        return NETWORK_ERR_QUEUE_FULL;
    }
    
    return NETWORK_OK;
}

network_state_t network_get_state(void)
{
    network_state_t state;
    xSemaphoreTake(g_network_ctx.state_mutex, portMAX_DELAY);
    state = g_network_ctx.state;
    xSemaphoreGive(g_network_ctx.state_mutex);
    return state;
}

network_error_t network_get_rssi(int8_t *rssi)
{
    if (!rssi) {
        return NETWORK_ERR_INVALID_PARAM;
    }
    
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        *rssi = ap_info.rssi;
        return NETWORK_OK;
    }
    
    return NETWORK_ERR_WIFI_FAILED;
}

network_error_t network_get_stats(network_stats_t *stats)
{
    if (!stats) {
        return NETWORK_ERR_INVALID_PARAM;
    }
    
    memcpy(stats, &g_network_ctx.stats, sizeof(network_stats_t));
    return NETWORK_OK;
}

network_error_t network_deinit(void)
{
    network_stop();
    
    /* Clean up FreeRTOS primitives */
    if (g_network_ctx.tx_queue) {
        vQueueDelete(g_network_ctx.tx_queue);
    }
    if (g_network_ctx.wifi_event_group) {
        vEventGroupDelete(g_network_ctx.wifi_event_group);
    }
    if (g_network_ctx.state_mutex) {
        vSemaphoreDelete(g_network_ctx.state_mutex);
    }
    
    memset(&g_network_ctx, 0, sizeof(network_context_t));
    g_network_ctx.sock_fd = -1;
    
    ESP_LOGI(TAG, "Network module deinitialized");
    return NETWORK_OK;
}
