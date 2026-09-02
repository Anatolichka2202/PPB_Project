cmake_minimum_required(VERSION 3.16)
project(PPB_Project LANGUAGES CXX)

#определяем выходные директории
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

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
# Для существующих локальных сборок сохраняем прежний layout. В чистом clone
# статическая lua/liblua54.a в репозитории отсутствует, поэтому используем
# системную Lua 5.4 вместо требования несуществующего бинарного файла.
set(PPB_LOCAL_LUA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lua/include" CACHE PATH "Bundled Lua include directory")
set(PPB_LOCAL_LUA_LIBRARY "${CMAKE_SOURCE_DIR}/lua/liblua54.a" CACHE FILEPATH "Bundled Lua library")

if(EXISTS "${PPB_LOCAL_LUA_INCLUDE_DIR}/lua.h" AND EXISTS "${PPB_LOCAL_LUA_LIBRARY}")
    set(LUA_INCLUDE_DIR "${PPB_LOCAL_LUA_INCLUDE_DIR}")
    set(LUA_INCLUDE_DIRS "${PPB_LOCAL_LUA_INCLUDE_DIR}")
    set(LUA_LIBRARY "${PPB_LOCAL_LUA_LIBRARY}")
    set(LUA_LIBRARIES "${PPB_LOCAL_LUA_LIBRARY}")
    message(STATUS "Using bundled Lua: ${PPB_LOCAL_LUA_LIBRARY}")
else()
    find_package(Lua 5.4 REQUIRED)
    message(STATUS "Using system Lua ${LUA_VERSION_STRING}: ${LUA_LIBRARIES}")
endif()

# CMake 3.16 FindLua does not guarantee an imported Lua::Lua target. Create a
# small interface target when needed so subprojects can use one stable name.
if(NOT TARGET Lua::Lua)
    add_library(Lua::Lua INTERFACE IMPORTED)
    set_target_properties(Lua::Lua PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}"
    )
endif()
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
add_subdirectory(Analize_Module)
