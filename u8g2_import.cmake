set(U8G2_ROOT_PATH ${CMAKE_CURRENT_LIST_DIR}/u8g2)
set(U8G2_SRC_PATH ${U8G2_ROOT_PATH}/csrc)

if (NOT EXISTS ${U8G2_ROOT_PATH})
    message(FATAL_ERROR "u8g2 source directory not found, submodule not initialized?")
endif()

file(GLOB U8G2_SRC_FILES CONFIGURE_DEPENDS ${U8G2_SRC_PATH}/*.c)