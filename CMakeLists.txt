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

set(SRC
    src/main.cpp
    src/MainWindow.cpp
    include/MainWindow.hpp
    include/PlasmaPercent.hpp
    src/PlasmaPercent.cpp)

qt_add_executable(kalorite
    ${SRC})

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
    KF6::WidgetsAddons)

install(TARGETS kalorite
    DESTINATION bin
)

install(FILES data/com.monsler.Kalorite.desktop
    DESTINATION share/applications
)

install(FILES data/com.monsler.Kalorite.png
    DESTINATION share/icons/hicolor/scalable/apps
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kalorite_ru.qm
    ${CMAKE_CURRENT_BINARY_DIR}/kalorite_en.qm
    DESTINATION ${CMAKE_INSTALL_DATADIR}/
)