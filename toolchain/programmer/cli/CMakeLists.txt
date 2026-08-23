cmake_minimum_required(VERSION 3.16)

include("${CMAKE_CURRENT_LIST_DIR}/../version.cmake")

project(vera-flash VERSION "${VERA_VERSION}" LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(vera-flash
  main.c
  programmer.c
  serial_port.c
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/../version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/vera_version.h"
  @ONLY
)
configure_file("${VERA_VERSION_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/VERSION" COPYONLY)
target_include_directories(vera-flash PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

if(MINGW)
  target_compile_options(vera-flash PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_link_options(vera-flash PRIVATE -static -static-libgcc)
elseif(MSVC)
  target_compile_options(vera-flash PRIVATE /W4 /WX)
endif()

enable_testing()
add_test(NAME vera-flash-self-test COMMAND vera-flash --self-test)
add_test(NAME vera-flash-version COMMAND vera-flash --version)
set_tests_properties(vera-flash-version PROPERTIES
  PASS_REGULAR_EXPRESSION "vera-flash v${PROJECT_VERSION}"
)
