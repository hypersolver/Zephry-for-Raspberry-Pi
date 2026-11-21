cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rpiduino_zavira LANGUAGES C CXX)

if(NOT CONFIG_BOARD_RPI_4B AND NOT CONFIG_BOARD_QEMU_CORTEX_A53)
  message(FATAL_ERROR "Select rpi_4b or qemu_cortex_a53")
endif()

include(FetchContent)
FetchContent_Declare(zavira
  GIT_REPOSITORY https://github.com/hypersolver/zavira-gui.git
  GIT_TAG 39a7f1fb999ecb014d305ac06a273a477d79ee4f)
FetchContent_MakeAvailable(zavira)

target_include_directories(app PRIVATE include ${zavira_SOURCE_DIR}/include)
target_compile_definitions(app PRIVATE RPIDUINO_ZAPP_DESKTOP RPIDUINO_UART_CONTROL)
target_sources(app PRIVATE
  src/main.cpp src/heartbeat.cpp src/uart_control.cpp src/zapp_loader.c)
if(CONFIG_BOARD_RPI_4B)
  target_sources(app PRIVATE src/reboot.c)
endif()

add_llext_target(zavira_desktop OUTPUT ${PROJECT_BINARY_DIR}/desktop.zapp
  SOURCES ${zavira_SOURCE_DIR}/zapps/desktop.c)
add_llext_target(zavira_demo OUTPUT ${PROJECT_BINARY_DIR}/lvgl-demo.zapp
  SOURCES ${zavira_SOURCE_DIR}/zapps/lvgl_demo.c)

# Zephyr 4.5 orders the packaging step after the link without tracking the linked file.
foreach(extension IN ITEMS zavira_desktop zavira_demo)
  add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/llext/${extension}_debug.elf
    APPEND DEPENDS $<TARGET_FILE:${extension}_llext_lib>)
endforeach()
add_custom_target(rpiduino_zapps ALL
  DEPENDS ${PROJECT_BINARY_DIR}/desktop.zapp ${PROJECT_BINARY_DIR}/lvgl-demo.zapp)
