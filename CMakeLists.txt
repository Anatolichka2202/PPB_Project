cmake_minimum_required(VERSION 3.16)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" PPB_VERSION)
string(STRIP "${PPB_VERSION}" PPB_VERSION)
if(NOT PPB_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)([-+].*)?$")
    message(FATAL_ERROR "VERSION must contain SemVer (MAJOR.MINOR.PATCH[-PRERELEASE]): ${PPB_VERSION}")
endif()
set(PPB_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(PPB_VERSION_MINOR "${CMAKE_MATCH_2}")
set(PPB_VERSION_PATCH "${CMAKE_MATCH_3}")

project(PPB_Project
    VERSION "${PPB_VERSION_MAJOR}.${PPB_VERSION_MINOR}.${PPB_VERSION_PATCH}"
    LANGUAGES C CXX
)

include(GNUInstallDirs)

# На Windows exe и runtime DLL должны лежать в одном каталоге. Qt инициализирует
# QT_DEPLOY_BIN_DIR из CMAKE_INSTALL_BINDIR при find_package(Qt6::Core), поэтому
# значение задаётся ДО поиска Qt.
if(WIN32)
    set(CMAKE_INSTALL_BINDIR "." CACHE STRING "Windows runtime install directory" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(BUILD_TESTS "Build tests" OFF)
option(PPB_FETCH_LUA_IF_MISSING "Fetch and build Lua when the local static library is missing" ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

# ==== Lua ====
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

set(BUILD_TESTS ${BUILD_TESTS} CACHE BOOL "Build tests" FORCE)

add_subdirectory(PPBCommunicationLib)
add_subdirectory(PPBControllerLib)
add_subdirectory(PPB_GUI_Sandbox)
add_subdirectory(CommonLib)
add_subdirectory(common/logging)
add_subdirectory(AKIP-manager_module)
add_subdirectory(ScenarioModule)
add_subdirectory(Analize_Module)

# ==== Windows installer ====
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_NAME "PPB")
    set(CPACK_PACKAGE_VENDOR "PPB Project")
    # NSIS file-version metadata stays numeric; PPB_VERSION carries prerelease identity.
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PPB control and test application")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "PPB")
    set(CPACK_PACKAGE_FILE_NAME "PPB-${PPB_VERSION}-Windows-x86_64")
    set(CPACK_PACKAGE_EXECUTABLES "PPB_GUI_Sandbox" "PPB")
    set(CPACK_CREATE_DESKTOP_LINKS "PPB_GUI_Sandbox")

    set(CPACK_NSIS_DISPLAY_NAME "PPB ${PPB_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "PPB")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/PPB_GUI_Sandbox/resources/ppb-icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/PPB_GUI_Sandbox/resources/ppb-icon.ico")

    include(CPack)
endif()
