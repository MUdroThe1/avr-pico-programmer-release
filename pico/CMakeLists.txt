cmake_minimum_required(VERSION 3.13)

# Always import the bundled SDK relative to this file
include(${CMAKE_CURRENT_LIST_DIR}/../pico-sdk/external/pico_sdk_import.cmake)

# Optional extras if present
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../pico-extras/external/pico_extras_import.cmake")
    include(${CMAKE_CURRENT_LIST_DIR}/../pico-extras/external/pico_extras_import.cmake)
endif()

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
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_spi
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(${PROJECT_NAME})
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)