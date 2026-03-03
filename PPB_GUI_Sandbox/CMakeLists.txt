cmake_minimum_required(VERSION 3.16)
project(PPB_GUI_Sandbox)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Network)

# Список исходных файлов (убедитесь, что все .cpp перечислены)
set(SOURCES
    main.cpp
    testerwindow.cpp
    akip_pult.cpp
    pult.cpp
    statuswidget.cpp
    connectionwidget.cpp
    controlwidget.cpp
    akipwidget.cpp
    fuwidget.cpp
    scenariowidget.cpp
    dataviewwidget.cpp
    logwidget.cpp
    loguimanager.cpp
    grattencontrolwidget.cpp
)

# Заголовочные файлы (для отображения в IDE, не обязательны для компиляции)
set(HEADERS
    testerwindow.h
    akip_pult.h
    pult.h
    statuswidget.h
    connectionwidget.h
    controlwidget.h
    akipwidget.h
    fuwidget.h
    scenariowidget.h
    dataviewwidget.h
    logwidget.h
    dependencies.h
    loguimanager.h
    grattencontrolwidget.h
)

# UI-файлы
set(UIS
    akip_pult.ui
    pult.ui
    testerwindow.ui
    statuswidget.ui
    connectionwidget.ui
    controlwidget.ui
    akipwidget.ui
    fuwidget.ui
    scenariowidget.ui
    dataviewwidget.ui
    logwidget.ui
    grattencontrolwidget.ui
)

# Ресурсы
set(RESOURCES
    assets.qrc
)

qt_add_executable(PPB_GUI_Sandbox
    ${SOURCES}
    ${HEADERS}
    ${UIS}
    ${RESOURCES}
    applicationmanager.h applicationmanager.cpp
    # dataconverter.h dataconverter.cpp
)

# Подключаем директории с заголовками
target_include_directories(PPB_GUI_Sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common/include
    #${CMAKE_SOURCE_DIR}/stubs
    ${CMAKE_SOURCE_DIR}/common/logging
    ${CMAKE_SOURCE_DIR}/common/converters
    ${CMAKE_SOURCE_DIR}/AKIP-manager_module
)

# Линкуемся с Qt и нашими библиотеками
target_link_libraries(PPB_GUI_Sandbox PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Network
    PPBControllerLib
    PPBCommunicationLib
    CommonLib
    akip_module
)

# Для Windows: скрыть консоль
if(WIN32)
    set_target_properties(PPB_GUI_Sandbox PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
