cmake_minimum_required(VERSION 3.16)
project(CommonLib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

# Файлы общих интерфейсов (все, которые содержат Q_OBJECT)
set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include/icommunication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include/icontroller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include/packetanalyzer_interface.h
    # ppbprotocol.h не содержит Q_OBJECT, но можно включить для удобства
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include/ppbprotocol.h
    ${CMAKE_SOURCE_DIR}/common/converters/dataconverter.cpp
)

add_library(${PROJECT_NAME} STATIC ${COMMON_SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common/converters>
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core)
