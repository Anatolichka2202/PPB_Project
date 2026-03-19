cmake_minimum_required(VERSION 3.16)
project(ScenarioModule LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)
find_package(Lua REQUIRED)

include(FetchContent)
FetchContent_Declare(
  sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(sol2)

set(SOURCES
    scenarioengine.cpp
)

set(HEADERS
    scenarioengine.h
)

add_library(scenario_lib STATIC ${SOURCES} ${HEADERS})

target_include_directories(scenario_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LUA_INCLUDE_DIR}
)

target_link_libraries(scenario_lib PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    ${LUA_LIBRARIES}
    sol2::sol2   # теперь PUBLIC
)
# Остальные библиотеки можно оставить PRIVATE, если они не нужны в интерфейсе
target_link_libraries(scenario_lib PRIVATE
    PPBControllerLib
    CommonLib
    LoggingLib
    akip_module
)
