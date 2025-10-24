cmake_minimum_required(VERSION 3.16)
project(YusiManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg)

add_executable(YusiManager
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/profile.h
    src/profile.cpp
    src/terminal_utils.h
    src/terminal_utils.cpp
    src/single_instance.h
    src/single_instance.cpp
    src/profiles_model.h
    src/profiles_model.cpp
    src/filter_proxy.h
    src/filter_proxy.cpp
    src/group_header_proxy.h
    src/group_header_proxy.cpp
    src/resources.qrc
)

target_link_libraries(YusiManager PRIVATE Qt6::Widgets Qt6::Network Qt6::Svg)
