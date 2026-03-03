cmake_minimum_required(VERSION 3.16)
project(PPB_Project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Опции сборки (можно переопределить при вызове cmake)
option(USE_REAL_DEPS "Use real dependencies from monolith" OFF)
option(BUILD_TESTS "Build tests" OFF)

# Поиск Qt (предпочтительно Qt6)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

# Пробрасываем опции в дочерние проекты (как cache variables)
set(USE_REAL_DEPS ${USE_REAL_DEPS} CACHE BOOL "Use real dependencies from monolith" FORCE)
set(BUILD_TESTS ${BUILD_TESTS} CACHE BOOL "Build tests" FORCE)

# Подключаем подпроекты (порядок важен: сначала коммуникация, потом контроллер)
add_subdirectory(PPBCommunicationLib)
add_subdirectory(PPBControllerLib)
add_subdirectory(PPB_GUI_Sandbox)
add_subdirectory(CommonLib)
add_subdirectory(common/logging)
add_subdirectory(AKIP-manager_module)
