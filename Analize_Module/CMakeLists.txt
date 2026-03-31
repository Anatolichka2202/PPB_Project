cmake_minimum_required(VERSION 3.16)

project(PacketAnalyzerModule LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build tests" Off)

# Пути к общим каталогам
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(COMMON_INCLUDE_DIR ${COMMON_DIR}/include)

# Находим Qt
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

# Исходники и заголовки библиотеки
set(LIB_SOURCES
    packetanalyzer.cpp
)
set(LIB_HEADERS
    packetanalyzer.h
    packetanalyzer_adapter.h
    analyzer_factory.h
)

# Статическая библиотека анализатора
add_library(PacketAnalyzerLib STATIC ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(PacketAnalyzerLib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${COMMON_INCLUDE_DIR}>
)

target_link_libraries(PacketAnalyzerLib PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
)

# Тесты (опционально)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(PacketAnalyzerTest
        main.cpp
        testanalyzer.cpp
        testanalyzer.h
    )
    target_include_directories(PacketAnalyzerTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${COMMON_INCLUDE_DIR}
    )
    target_link_libraries(PacketAnalyzerTest PRIVATE
        PacketAnalyzerLib
        Qt6::Core
        Qt6::Test
    )
    add_test(NAME PacketAnalyzerTest COMMAND PacketAnalyzerTest)
endif()
