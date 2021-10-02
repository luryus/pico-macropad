cmake_minimum_required(VERSION 3.13)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

include(pico_sdk_import)
include(u8g2_import)
include(GetGitRevisionDescription)

get_git_head_revision(GIT_REFSPEC GIT_SHA1)

project(macropad VERSION 0.2)
set(CMAKE_C_STANDARD 11)
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

configure_file("version.h.in" "version.h" @ONLY)

pico_sdk_init()

add_executable(macropad
    main.c
    usb_descriptors.c
    usb_hid.c
    pico_u8g2_i2c.c
    display_ui.c
    utils.c
    ${U8G2_SRC_FILES})

target_include_directories(macropad
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}
    PRIVATE ${U8G2_SRC_PATH})

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