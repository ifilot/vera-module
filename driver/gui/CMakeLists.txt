cmake_minimum_required(VERSION 3.21)
project(vera_gui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets SerialPort)

qt_add_executable(vera-gui WIN32
  main.cpp
  mainwindow.cpp
  mainwindow.h
  bridgeclient.cpp
  bridgeclient.h
  resources.qrc
  app.rc
)

target_link_libraries(vera-gui PRIVATE Qt6::Widgets Qt6::SerialPort)

install(TARGETS vera-gui RUNTIME DESTINATION .)
