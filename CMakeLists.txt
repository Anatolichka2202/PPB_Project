cmake_minimum_required(VERSION 3.16)
project(PPB_Project LANGUAGES CXX)

#определяем выходные директории
##set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Опции сборки (можно переопределить при вызове cmake)

option(BUILD_TESTS "Build tests" OFF)

# Поиск Qt (предпочтительно Qt6)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)


# ==== Lua ====
# Явно задаём пути к локальной Lua
set(LUA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lua/include" CACHE PATH "Lua include directory")
set(LUA_LIBRARY "${CMAKE_SOURCE_DIR}/lua/liblua54.a" CACHE FILEPATH "Lua library")
set(LUA_LIBRARIES ${LUA_LIBRARY} CACHE STRING "Lua libraries")

# Проверяем, что заголовки действительно существуют
if(NOT EXISTS "${LUA_INCLUDE_DIR}/lua.h")
    message(FATAL_ERROR "Lua headers not found in ${LUA_INCLUDE_DIR}")
endif()

# Создаём импортированную цель для Lua (удобно для линковки в подпроектах)
add_library(Lua::Lua UNKNOWN IMPORTED)
set_target_properties(Lua::Lua PROPERTIES
    IMPORTED_LOCATION "${LUA_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
)

# =================


# Пробрасываем опции в дочерние проекты (как cache variables)

set(BUILD_TESTS ${BUILD_TESTS} CACHE BOOL "Build tests" FORCE)

# Подключаем подпроекты (порядок важен: сначала коммуникация, потом контроллер)
add_subdirectory(PPBCommunicationLib)
add_subdirectory(PPBControllerLib)
add_subdirectory(PPB_GUI_Sandbox)
add_subdirectory(CommonLib)
add_subdirectory(common/logging)
add_subdirectory(AKIP-manager_module)
add_subdirectory(ScenarioModule)
