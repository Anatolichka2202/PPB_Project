cmake_minimum_required(VERSION 3.16)
project(ScenarioModule LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

# Lua уже настроена в корневом проекте, используем переменные
# Путь к sol2 (локальная копия)
set(SOL2_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/sol2/include")

# Файлы модуля
set(SOURCES scenarioengine.cpp)
set(HEADERS scenarioengine.h)

add_library(scenario_lib STATIC ${SOURCES} ${HEADERS}
    scripts.qrc
    scripts/autotest.lua)

target_include_directories(scenario_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LUA_INCLUDE_DIR}          # путь из корневого проекта
    ${SOL2_INCLUDE_DIR}          # путь к sol2
)

target_link_libraries(scenario_lib PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    ${LUA_LIBRARIES}             # библиотека Lua
    # Если где-то в коде используется цель Lua::Lua, можно добавить её вместо ${LUA_LIBRARIES}
    # но проще оставить переменные
)

# Остальные зависимости (если нужны)
target_link_libraries(scenario_lib PRIVATE
    PPBControllerLib
    CommonLib
    LoggingLib
    akip_module
)
