# CMakeLists.txt for Network + Display Application
# 
# This file configures the ESP-IDF build system for the project.
# ESP-IDF uses CMake as its build system.
#
# Build Instructions:
#   1. Set up ESP-IDF environment: source ~/esp/esp-idf/export.sh
#   2. Configure project: idf.py set-target esp32
#   3. Build project: idf.py build
#   4. Flash to device: idf.py -p /dev/ttyUSB0 flash monitor
#
# Reference: ESP-IDF Build System Documentation
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "src")

# Include ESP-IDF CMake build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project name and version
project(network_display VERSION 1.0)

# Project will automatically find and include components from the 'main' directory
# The main component must have its own CMakeLists.txt