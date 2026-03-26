cmake_minimum_required(VERSION 3.21)

project(ozon_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Qt5 REQUIRED COMPONENTS Quick QuickControls2 Widgets)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(ozon_cpp
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/shortcutlayoutfix.cpp
    ${SRC_DIR}/shortcutlayoutfix.h
    ${SRC_DIR}/product.h
    ${SRC_DIR}/product.cpp
    ${SRC_DIR}/productmodel.h
    ${SRC_DIR}/productmodel.cpp
    ${SRC_DIR}/productcardparser.h
    ${SRC_DIR}/productcardparser.cpp
    ${SRC_DIR}/ozonscraper.h
    ${SRC_DIR}/ozonscraper.cpp
    qml.qrc
)

target_link_libraries(ozon_cpp PRIVATE
    Qt5::Quick
    Qt5::QuickControls2
    Qt5::Widgets
)

target_include_directories(ozon_cpp PRIVATE ${SRC_DIR})
