# Main Component CMakeLists.txt
#
# This file defines the main component that contains all application source files.
# The ESP-IDF build system automatically discovers and builds all .c files listed here.
#
# Component dependencies are automatically resolved by ESP-IDF based on the
# included headers and linked libraries.

idf_component_register(
    SRCS 
        "network_test.c"
        "network_api.c"
        "ssd1306_display.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        driver       # For GPIO and I2C drivers
        esp_wifi     # For WiFi functionality  
        nvs_flash    # For Non-Volatile Storage (required by WiFi)
        esp_netif    # For TCP/IP stack interface
        esp_event    # For event loop
        lwip         # For socket API
)