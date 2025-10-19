cmake_minimum_required(VERSION 3.15)

project(kalorite VERSION 0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STRANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(ECM 1.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})
find_package(KF6I18n NO_MODULE REQUIRED)
find_package(KF6WidgetsAddons REQUIRED)

find_package(KF6 REQUIRED COMPONENTS CoreAddons WidgetsAddons)
find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Multimedia)

add_subdirectory(external/json)

set(SRC
    src/main.cpp
    src/MainWindow.cpp
    include/MainWindow.hpp
    include/PlasmaPercent.hpp
    src/PlasmaPercent.cpp)

configure_file(${CMAKE_SOURCE_DIR}/resources.qrc
               ${CMAKE_BINARY_DIR}/resources.qrc
               COPYONLY)

qt_add_executable(kalorite
    ${SRC}
    ${CMAKE_BINARY_DIR}/resources.qrc)

qt_add_translations(kalorite
    TS_FILES
        languages/kalorite_ru.ts
        languages/kalorite_en.ts

    SOURCES
        ${SRC}
)

target_include_directories(kalorite PRIVATE include/)

target_link_libraries(kalorite PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    KF6::CoreAddons
    KF6::I18n
    KF6::WidgetsAddons
    nlohmann_json::nlohmann_json)

install(TARGETS kalorite
    DESTINATION bin
)

install(FILES data/io.github.monsler.Kalorite.desktop
    DESTINATION share/applications
)

install(FILES data/io.github.monsler.Kalorite.appdata.xml
    DESTINATION share/appdata
)

install(FILES data/io.github.monsler.Kalorite.png
    DESTINATION share/icons/hicolor/512x512/apps
)
