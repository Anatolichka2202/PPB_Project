cmake_minimum_required(VERSION 3.16)
project(LoggingLib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

set(LOGGING_SOURCES
    logentry.cpp
    logconfig.cpp
    logdistributor.cpp
    logwrapper.cpp
    # если нужен
)

set(LOGGING_HEADERS
    logentry.h
    logconfig.h
    logdistributor.h
    logwrapper.h


)

add_library(${PROJECT_NAME} STATIC ${LOGGING_SOURCES} ${LOGGING_HEADERS}
    logcategories.h
    logmacros.h)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>   # для #include "logentry.h" и т.д.
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui)
