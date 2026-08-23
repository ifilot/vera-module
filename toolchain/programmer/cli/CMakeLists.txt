cmake_minimum_required(VERSION 3.16)

project(vera-flash C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(vera-flash
  main.c
  programmer.c
  serial_port.c
)

if(MINGW)
  target_compile_options(vera-flash PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_link_options(vera-flash PRIVATE -static -static-libgcc)
elseif(MSVC)
  target_compile_options(vera-flash PRIVATE /W4 /WX)
endif()

enable_testing()
add_test(NAME vera-flash-self-test COMMAND vera-flash --self-test)
