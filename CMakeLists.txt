cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(macropad)
set(CMAKE_C_STANDARD 11)
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)


pico_sdk_init()

add_executable(macropad
    main.c
    ssd1306_display.c
    usb_descriptors.c
    usb_hid.c)
target_include_directories(macropad
    PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_add_extra_outputs(macropad)
target_link_libraries(macropad 
    pico_stdlib
    pico_unique_id
    hardware_pio
    hardware_clocks
    hardware_irq
    hardware_i2c
    tinyusb_device)


pico_generate_pio_header(macropad ${CMAKE_CURRENT_LIST_DIR}/encoder.pio)
pico_generate_pio_header(macropad ${CMAKE_CURRENT_LIST_DIR}/key_matrix.pio)