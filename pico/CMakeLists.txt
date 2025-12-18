cmake_minimum_required(VERSION 3.13)

# Default to the repo-local pico-sdk to avoid mixing multiple SDK checkouts.
if (NOT DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../pico-sdk" CACHE PATH "Path to the Raspberry Pi Pico SDK")
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(prog C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

pico_sdk_init()

add_executable(${PROJECT_NAME}
    main.c
    avrprog.c
    avr_devices.c
    stk500v1.c
    usb_descriptors.c
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_spi
    tinyusb_device
    tinyusb_board
)

# Ensure our local headers (e.g., tusb_config.h) are visible to TinyUSB build
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_add_extra_outputs(${PROJECT_NAME})
# We use TinyUSB CDC directly for STK500v1; do not enable stdio-over-USB (it also uses TinyUSB).
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)