cmake_minimum_required(VERSION 3.16)
project(PPBCommunicationLib VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Опции (наследуются из корня, если определены)

if(NOT DEFINED BUILD_TESTS)
    option(BUILD_TESTS "Build tests" Off)
endif()

# Пути к общим каталогам
set(COMMON_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(COMMON_INCLUDE_DIR ${COMMON_DIR}/include)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)


# Исходные файлы библиотеки
set(LIB_SOURCES
    src/commandandoperation.cpp
    src/communicationengine.cpp
    src/communicationfacade.cpp
    src/packetbuilder.cpp
    src/protocoladapter.cpp
    src/udpclient.cpp
)

# Заголовочные файлы (для отображения в IDE)
set(LIB_HEADERS
    include/iprotocol_adapter.h
    include/itransport.h
    src/commandandoperation.h
    src/commandinterface.h
    src/communicationengine.h
    src/communicationfacade.h
    src/packetbuilder.h
    src/protocoladapter.h
    src/udpclient.h
    comm_dependencies.h
)

add_library(${PROJECT_NAME} STATIC ${LIB_SOURCES} ${LIB_HEADERS}
   )

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${COMMON_INCLUDE_DIR}>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${COMMON_DIR}>          # для доступа к common/stubs
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Gui
    CommonLib
    LoggingLib)



# ===== Тесты =====
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test )

    add_executable(test_communication
        tests/testengine.cpp
        tests/testengine.h
        tests/mockudpclient.h
    )

    target_include_directories(test_communication
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${COMMON_INCLUDE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${COMMON_DIR}
    )

    target_link_libraries(test_communication
        PRIVATE
            ${PROJECT_NAME}
            Qt6::Core
            Qt6::Network
            Qt6::Test
    )



    add_test(NAME CommunicationTest COMMAND test_communication)
endif()
