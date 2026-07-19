cmake_minimum_required(VERSION 3.16)

# ================================================================
# ESP32-C3 RID Scanner + Simulator
# ================================================================
# Functions:
#   - C-RID Sniffer & Decode (GB 42590 / ASTM F3411)
#   - C-RID Simulated Transmitter
#   - Web Management + Captive Portal
#   - NVS Caching
#   - Serial Debug Output
#
# Target: ESP32-C3 (SuperMini)
# ================================================================

set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_crid)
