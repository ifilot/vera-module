cmake_minimum_required(VERSION 3.18)

set(VERA_PICO_SDK_VERSION "2.2.0" CACHE STRING "Pico SDK version to download")
set(PICO_BOARD pico CACHE STRING "Pico board type (pico or pico2)")

if(DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
  set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

if(NOT PICO_SDK_PATH)
  include(FetchContent)
  message(STATUS "Downloading Raspberry Pi Pico SDK ${VERA_PICO_SDK_VERSION}")
  FetchContent_Declare(
    pico_sdk
    GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
    GIT_TAG ${VERA_PICO_SDK_VERSION}
    GIT_SHALLOW TRUE
    GIT_SUBMODULES lib/tinyusb
    GIT_SUBMODULES_RECURSE FALSE
  )
  FetchContent_GetProperties(pico_sdk)
  if(NOT pico_sdk_POPULATED)
    FetchContent_Populate(pico_sdk)
  endif()
  set(PICO_SDK_PATH "${pico_sdk_SOURCE_DIR}" CACHE PATH "Pico SDK path" FORCE)
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(vera-programmer C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(vera-programmer
  main.c
  commands.c
  flash.c
)

target_include_directories(vera-programmer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(vera-programmer
  pico_stdlib
  hardware_spi
)

# Use USB CDC for the programmer protocol; the UART remains available to VERA.
pico_enable_stdio_usb(vera-programmer 1)
pico_enable_stdio_uart(vera-programmer 0)
pico_add_extra_outputs(vera-programmer)
