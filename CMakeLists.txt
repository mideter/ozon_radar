cmake_minimum_required(VERSION 3.21)

project(ozon_radar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Qt5 REQUIRED COMPONENTS Quick QuickControls2 Widgets)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(ozon_radar
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/shortcutlayoutfix.cpp
    ${SRC_DIR}/shortcutlayoutfix.h
    ${SRC_DIR}/product.h
    ${SRC_DIR}/product.cpp
    ${SRC_DIR}/productmodel.h
    ${SRC_DIR}/productmodel.cpp
    ${SRC_DIR}/ozon_scraper/urlinputparser.h
    ${SRC_DIR}/ozon_scraper/urlinputparser.cpp
    ${SRC_DIR}/ozon_scraper/pythonfetchprocessrunner.h
    ${SRC_DIR}/ozon_scraper/pythonfetchprocessrunner.cpp
    ${SRC_DIR}/ozon_scraper/productaccumulator.h
    ${SRC_DIR}/ozon_scraper/productaccumulator.cpp
    ${SRC_DIR}/ozon_scraper/batchproductmapper.h
    ${SRC_DIR}/ozon_scraper/batchproductmapper.cpp
    ${SRC_DIR}/ozon_scraper/productcardparser.h
    ${SRC_DIR}/ozon_scraper/productcardparser.cpp
    ${SRC_DIR}/ozon_scraper/fetcheventparser.h
    ${SRC_DIR}/ozon_scraper/fetcheventparser.cpp
    ${SRC_DIR}/ozon_scraper/scraperresultutils.h
    ${SRC_DIR}/ozon_scraper/scraperresultutils.cpp
    ${SRC_DIR}/ozon_scraper/ozonradarscraper.h
    ${SRC_DIR}/ozon_scraper/ozonradarscraper.cpp
    ${SRC_DIR}/settingsservice.h
    ${SRC_DIR}/settingsservice.cpp
    qml.qrc
)

target_link_libraries(ozon_radar PRIVATE
    Qt5::Quick
    Qt5::QuickControls2
    Qt5::Widgets
)

target_include_directories(ozon_radar PRIVATE ${SRC_DIR})
