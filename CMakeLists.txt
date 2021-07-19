cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(macropad)
set(CMAKE_C_STANDARD 11)
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)


pico_sdk_init()

add_executable(macropad main.c)

pico_add_extra_outputs(main)
target_link_libraries(main pico_stdlib)
