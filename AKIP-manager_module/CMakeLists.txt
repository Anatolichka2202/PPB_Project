cmake_minimum_required(VERSION 3.16)
project(AKIP-module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_AKIP_TESTS "Build AKIP test application" OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

if(BUILD_AKIP_TESTS)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
endif()

# ==================== Платформозависимые настройки ====================
if(WIN32)
    # Windows: используем оригинальный SDK от WCH
    set(CH375_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ch375_sdk")
    set(EXTRA_INCLUDES ${CH375_SDK_DIR})
    set(EXTRA_LIBS ${CH375_SDK_DIR}/CH375DLL64.lib)
    # Для MinGW могут потребоваться дополнительные системные библиотеки
    if(MINGW)
        set(EXTRA_LIBS ${EXTRA_LIBS} -luser32 -lgdi32 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -lws2_32)
    endif()
    # Переменная для исходников CH37X (пустая на Windows)
    set(CH37X_SOURCES "")
else()
    # Linux: используем SDK из папки ch375_linux
    set(CH375_LINUX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ch375_linux")
    set(EXTRA_INCLUDES ${CH375_LINUX_DIR})
    set(EXTRA_LIBS pthread)   # может потребоваться для многопоточности
    set(CH37X_SOURCES ${CH375_LINUX_DIR}/ch37x_lib.c)
    # Явно указываем, что файл нужно компилировать как C++
    set_source_files_properties(${CH37X_SOURCES} PROPERTIES LANGUAGE CXX)
endif()

# ==================== БИБЛИОТЕКА МОДУЛЯ ====================
set(LIB_SOURCES
    usbinterface.cpp
    akipfacade.cpp
    laninterface.cpp
    grattenga1483controller.cpp
    abstractscpicontroller.cpp
    ${CH37X_SOURCES}   # для Linux добавится ch37x_lib.c, для Windows останется пустым
)

set(LIB_HEADERS
    usbinterface.h
    iakipcontroller.h
    akipfacade.h
    laninterface.h
    grattenga1483controller.h
    abstractscpicontroller.h
    gratten_limits.h
)

add_library(akip_module STATIC ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(akip_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${EXTRA_INCLUDES}
)

target_link_libraries(akip_module PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    ${EXTRA_LIBS}
)

set_target_properties(akip_module PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

# ==================== ТЕСТОВОЕ ПРИЛОЖЕНИЕ (ОПЦИОНАЛЬНО) ====================
if(BUILD_AKIP_TESTS)
    set(TEST_SOURCES
        main.cpp
        mainwindow.cpp
    )
    set(TEST_HEADERS
        mainwindow.h
    )
    set(TEST_UIS
        mainwindow.ui
    )
    qt_wrap_ui(test_ui_files ${TEST_UIS})
    add_executable(akip_test ${TEST_SOURCES} ${TEST_HEADERS} ${test_ui_files})
    target_link_libraries(akip_test PRIVATE
        akip_module
        Qt${QT_VERSION_MAJOR}::Widgets
    )
    # Копирование CH375DLL64.dll только для Windows
    if(WIN32)
        add_custom_command(TARGET akip_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CH375_SDK_DIR}/CH375DLL64.dll"
                $<TARGET_FILE_DIR:akip_test>
            COMMENT "Copying CH375DLL64.dll to test output directory"
        )
    endif()
    if(MINGW)
        target_link_libraries(akip_test PRIVATE
            -luser32 -lgdi32 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -lws2_32
        )
    endif()
endif()
