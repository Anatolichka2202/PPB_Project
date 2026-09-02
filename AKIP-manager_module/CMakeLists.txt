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
    # Windows: заголовки WCH нужны на этапе компиляции, но CH375DLL64.dll
    # загружается динамически через QLibrary. Это убирает зависимость сборки
    # от отсутствующей в репозитории CH375DLL64.lib.
    set(CH375_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ch375_sdk")
    set(CH375_DLL_FILE "${CH375_SDK_DIR}/CH375DLL64.dll")
    set(EXTRA_INCLUDES ${CH375_SDK_DIR})
    set(EXTRA_LIBS "")

    if(MINGW)
        list(APPEND EXTRA_LIBS
            -luser32 -lgdi32 -ladvapi32 -lshell32
            -lole32 -loleaut32 -luuid -lws2_32
        )
    endif()

    # Если фирменная DLL WCH присутствует в локальном SDK, включаем её в
    # install tree/NSIS. В чистом clone её нет, поэтому установка приложения
    # остаётся воспроизводимой, а USB backend сообщит о недостающем runtime.
    install(FILES "${CH375_DLL_FILE}" DESTINATION . OPTIONAL)

    set(CH37X_SOURCES "")
else()
    # Linux: используем SDK из папки ch375_linux/demo
    set(CH375_LINUX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ch375_sdk/ch375_linux/demo")
    set(EXTRA_INCLUDES ${CH375_LINUX_DIR})
    set(EXTRA_LIBS pthread)
    set(CH37X_SOURCES ${CH375_LINUX_DIR}/ch37x_lib.c)
    set_source_files_properties(${CH37X_SOURCES} PROPERTIES LANGUAGE CXX)
endif()

# ==================== БИБЛИОТЕКА МОДУЛЯ ====================
set(LIB_SOURCES
    usbinterface.cpp
    akipfacade.cpp
    laninterface.cpp
    grattenga1483controller.cpp
    abstractscpicontroller.cpp
    ${CH37X_SOURCES}
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

    if(WIN32 AND EXISTS "${CH375_DLL_FILE}")
        add_custom_command(TARGET akip_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CH375_DLL_FILE}"
                $<TARGET_FILE_DIR:akip_test>
            COMMENT "Copying CH375DLL64.dll to test output directory"
        )
    endif()
endif()
