cmake_minimum_required(VERSION 3.16)
project(CommonLib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

# Заголовочные файлы с Q_OBJECT (должны быть обработаны moc)
set(COMMON_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include/icommunication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include/icontroller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include/packetanalyzer_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include/ppbprotocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/performance/performancemonitor.h
)

# Исходные файлы (.cpp)
set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/common/converters/dataconverter.cpp
    ${CMAKE_SOURCE_DIR}/common/performance/performancemonitor.cpp
)

# Добавляем и исходники, и заголовки в библиотеку
add_library(${PROJECT_NAME} STATIC
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common/converters>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common/performance>
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Core)
