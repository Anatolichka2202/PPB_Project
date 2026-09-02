cmake_minimum_required(VERSION 3.16)
project(PPB_Project LANGUAGES C CXX)

include(GNUInstallDirs)

# На Windows exe и runtime DLL должны лежать в одном каталоге. Qt инициализирует
# QT_DEPLOY_BIN_DIR из CMAKE_INSTALL_BINDIR при find_package(Qt6::Core), поэтому
# значение задаётся ДО поиска Qt.
if(WIN32)
    set(CMAKE_INSTALL_BINDIR "." CACHE STRING "Windows runtime install directory" FORCE)
endif()

# определяем выходные директории
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(BUILD_TESTS "Build tests" OFF)
option(PPB_FETCH_LUA_IF_MISSING "Fetch and build Lua when the local static library is missing" ON)

# Поиск Qt (предпочтительно Qt6)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

# ==== Lua ====
# Локальная библиотека сохраняет совместимость с существующей рабочей средой.
# Чистые CI/clone-сборки получают воспроизводимый fallback на Lua 5.4.8.
set(PPB_LOCAL_LUA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lua/include" CACHE PATH "Local Lua include directory")
set(PPB_LOCAL_LUA_LIBRARY "${CMAKE_SOURCE_DIR}/lua/liblua54.a" CACHE FILEPATH "Local Lua static library")

if(EXISTS "${PPB_LOCAL_LUA_INCLUDE_DIR}/lua.h" AND EXISTS "${PPB_LOCAL_LUA_LIBRARY}")
    message(STATUS "Using local Lua library: ${PPB_LOCAL_LUA_LIBRARY}")

    add_library(Lua::Lua STATIC IMPORTED GLOBAL)
    set_target_properties(Lua::Lua PROPERTIES
        IMPORTED_LOCATION "${PPB_LOCAL_LUA_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PPB_LOCAL_LUA_INCLUDE_DIR}"
    )

    set(LUA_INCLUDE_DIR "${PPB_LOCAL_LUA_INCLUDE_DIR}")
    set(LUA_LIBRARY "${PPB_LOCAL_LUA_LIBRARY}")
    set(LUA_LIBRARIES Lua::Lua)
elseif(PPB_FETCH_LUA_IF_MISSING)
    message(STATUS "Local Lua library is missing; fetching pinned Lua 5.4.8 sources")

    include(FetchContent)
    FetchContent_Declare(
        ppb_lua_source
        GIT_REPOSITORY https://github.com/lua/lua.git
        GIT_TAG 6e22fedb74cf0c9b6656e9fce8b7331db847c605 # v5.4.8
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(ppb_lua_source)

    set(PPB_LUA_SOURCES
        lapi.c
        lauxlib.c
        lbaselib.c
        lcode.c
        lcorolib.c
        lctype.c
        ldblib.c
        ldebug.c
        ldo.c
        ldump.c
        lfunc.c
        lgc.c
        linit.c
        liolib.c
        llex.c
        lmathlib.c
        lmem.c
        loadlib.c
        lobject.c
        lopcodes.c
        loslib.c
        lparser.c
        lstate.c
        lstring.c
        lstrlib.c
        ltable.c
        ltablib.c
        ltm.c
        lundump.c
        lutf8lib.c
        lvm.c
        lzio.c
    )

    list(TRANSFORM PPB_LUA_SOURCES PREPEND "${ppb_lua_source_SOURCE_DIR}/")

    add_library(ppb_lua STATIC ${PPB_LUA_SOURCES})
    add_library(Lua::Lua ALIAS ppb_lua)
    target_include_directories(ppb_lua PUBLIC "${ppb_lua_source_SOURCE_DIR}")
    set_target_properties(ppb_lua PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # Lua 5.4.8's luaconf.h already enables LUA_USE_WINDOWS when compiling on
    # Windows; defining it again only creates noisy redefinition warnings.
    if(UNIX AND NOT APPLE)
        target_compile_definitions(ppb_lua PRIVATE LUA_USE_LINUX)
        target_link_libraries(ppb_lua PUBLIC m ${CMAKE_DL_LIBS})
    endif()

    set(LUA_INCLUDE_DIR "${ppb_lua_source_SOURCE_DIR}")
    set(LUA_LIBRARY Lua::Lua)
    set(LUA_LIBRARIES Lua::Lua)
else()
    message(FATAL_ERROR
        "Lua static library was not found at ${PPB_LOCAL_LUA_LIBRARY}. "
        "Provide it or configure with -DPPB_FETCH_LUA_IF_MISSING=ON."
    )
endif()

# Пробрасываем опции в дочерние проекты
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

# ==== Windows installer ====
# Qt runtime deployment is registered next to the executable target in
# PPB_GUI_Sandbox/CMakeLists.txt. CPack/NSIS then turns the install tree into
# a normal Windows setup executable.
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_NAME "PPB")
    set(CPACK_PACKAGE_VENDOR "PPB Project")
    set(CPACK_PACKAGE_VERSION "1.1.0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PPB control and test application")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "PPB")
    set(CPACK_PACKAGE_FILE_NAME "PPB-1.01-beta-Windows-x86_64")
    set(CPACK_PACKAGE_EXECUTABLES "PPB_GUI_Sandbox" "PPB")
    set(CPACK_CREATE_DESKTOP_LINKS "PPB_GUI_Sandbox")

    set(CPACK_NSIS_DISPLAY_NAME "PPB 1.01 beta")
    set(CPACK_NSIS_PACKAGE_NAME "PPB")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)

    include(CPack)
endif()
