cmake_minimum_required(VERSION 3.16)
project(PPBControllerLib VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Опции (могут быть переданы из корневого CMake)
if(NOT DEFINED USE_REAL_DEPS)
    option(USE_REAL_DEPS "Use real dependencies from monolith" OFF)
endif()
if(NOT DEFINED BUILD_TESTS)
    option(BUILD_TESTS "Build tests" Off)
endif()

# Пути к общим каталогам
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(COMMON_INCLUDE_DIR ${COMMON_DIR}/include)
#(COMMON_STUBS_DIR ${COMMON_DIR}/stubs)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Собираем все заголовки из common для отображения в IDE
file(GLOB_RECURSE COMMON_HEADERS
    ${COMMON_INCLUDE_DIR}/*.h
    #${COMMON_STUBS_DIR}/*.h
)

# Исходные файлы библиотеки
set(LIB_SOURCES
    src/ppbcontrollerlib.cpp
)

# Публичные заголовки контроллера
set(LIB_PUBLIC_HEADERS
    include/ppbcontrollerlib.h
    include/controller_dependencies.h
)

add_library(${PROJECT_NAME} STATIC
    ${LIB_SOURCES}
    ${LIB_PUBLIC_HEADERS}

)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${COMMON_INCLUDE_DIR}>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/common/logging>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/common/converters>
        ${CMAKE_SOURCE_DIR}/AKIP-manager_module
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        CommonLib
        LoggingLib
        akip_module
)

# Реальные зависимости из монолита (если включены)
if(USE_REAL_DEPS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_REAL_DEPS)
    if(DEFINED MONOLITH_INCLUDE_DIR)
        target_include_directories(${PROJECT_NAME} PRIVATE ${MONOLITH_INCLUDE_DIR})
    endif()
	target_link_libraries(${PROJECT_NAME} PUBLIC PPBCommunicationLib)
    # При необходимости добавить линковку с библиотеками монолита
    # target_link_libraries(${PROJECT_NAME} PRIVATE MonolithLib)
endif()

# ===== Тесты =====
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(test_ppbcontroller
        tests/main.cpp
    )

    target_include_directories(test_ppbcontroller
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${COMMON_INCLUDE_DIR}
            #${COMMON_STUBS_DIR}
    )

    target_link_libraries(test_ppbcontroller
        PRIVATE
            ${PROJECT_NAME}
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            Qt6::Widgets
    )

    if(USE_REAL_DEPS)
        target_compile_definitions(test_ppbcontroller PRIVATE USE_REAL_DEPS)
    endif()

    add_test(NAME ControllerTest COMMAND test_ppbcontroller)
endif()
