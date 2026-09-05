cmake_minimum_required(VERSION 3.16)
project(PPB_GUI_Sandbox)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core Network Charts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core Network Charts)

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
    updatemanager.cpp
)

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
    updatemanager.h
)

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

set(RESOURCES themes.qrc app_resources.qrc)
set(PLATFORM_SOURCES)
if(WIN32)
    list(APPEND PLATFORM_SOURCES windows_app.rc)
endif()

add_executable(PPB_GUI_Sandbox
    ${SOURCES}
    ${HEADERS}
    ${UIS}
    ${RESOURCES}
    ${PLATFORM_SOURCES}
    applicationmanager.h applicationmanager.cpp
    performancewindow.h performancewindow.cpp performancewindow.ui
    moc_objects/mockgeneratorcontroller.h moc_objects/mockgeneratorcontroller.cpp
    thememanager.h thememanager.cpp
)

set_target_properties(PPB_GUI_Sandbox PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(PPB_GUI_Sandbox PRIVATE PPB_VERSION="${PPB_VERSION}")

target_include_directories(PPB_GUI_Sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/PPB_GUI_Sandbox_autogen/include
    ${CMAKE_SOURCE_DIR}/common/include
    ${CMAKE_SOURCE_DIR}/common/logging
    ${CMAKE_SOURCE_DIR}/common/converters
    ${CMAKE_SOURCE_DIR}/AKIP-manager_module
)

target_link_libraries(PPB_GUI_Sandbox PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Charts
    PPBControllerLib
    PPBCommunicationLib
    CommonLib
    akip_module
    scenario_lib
    PacketAnalyzerLib
)

if(WIN32)
    set_target_properties(PPB_GUI_Sandbox PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS PPB_GUI_Sandbox
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)

if(WIN32 AND QT_VERSION_MAJOR EQUAL 6)
    qt_generate_deploy_app_script(
        TARGET PPB_GUI_Sandbox
        OUTPUT_SCRIPT ppb_qt_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${ppb_qt_deploy_script})
elseif(WIN32)
    message(WARNING "Automated Windows runtime deployment is supported for Qt 6 builds only")
endif()
