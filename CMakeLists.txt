cmake_minimum_required(VERSION 4.02)

project(kalorite VERSION 0.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STRANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


if(NOT APPLE AND UNIX)
    find_package(ECM 1.0 REQUIRED NO_MODULE)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})


    find_package(KF6 REQUIRED COMPONENTS CoreAddons WidgetsAddons)
    find_package(KF6I18n NO_MODULE REQUIRED)
    find_package(KF6WidgetsAddons REQUIRED)
    find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Multimedia DBus Network)


else()
    find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Multimedia Network)
endif()

add_subdirectory(external/json)

# ---------------------------------------------------------------------------
# LuaJIT: built via its own Makefile as a static lib, then imported here.
# ---------------------------------------------------------------------------
set(LUAJIT_DIR ${CMAKE_SOURCE_DIR}/external/luajit)
set(LUAJIT_LIB ${LUAJIT_DIR}/src/libluajit.a)

add_custom_command(
    OUTPUT ${LUAJIT_LIB}
    COMMAND make -C ${LUAJIT_DIR} BUILDMODE=static amalg
    WORKING_DIRECTORY ${LUAJIT_DIR}
    COMMENT "Building LuaJIT (static)"
    VERBATIM
)
add_custom_target(luajit_build DEPENDS ${LUAJIT_LIB})

add_library(luajit STATIC IMPORTED GLOBAL)
set_target_properties(luajit PROPERTIES IMPORTED_LOCATION ${LUAJIT_LIB})
add_dependencies(luajit luajit_build)

set(SRC
    src/main.cpp
    src/MainWindow.cpp
    include/MainWindow.hpp
    include/PlasmaPercent.hpp
    src/PlasmaPercent.cpp
    src/Mixer.cpp
    include/Mixer.hpp
    src/SongDownloader.cpp
    include/SongDownloader.hpp
    src/WinampDisplay.cpp
    include/WinampDisplay.hpp
    src/VolumeSignalWidget.cpp
    include/VolumeSignalWidget.hpp
    src/PluginManager.cpp
    include/PluginManager.hpp
    src/LuaApi.cpp
    include/LuaApi.hpp
    src/miniaudio.c
)

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

target_include_directories(kalorite PRIVATE include/ ${LUAJIT_DIR}/src)

# LuaJIT's amalgamated static lib must be finished before we compile/link.
add_dependencies(kalorite luajit_build)

if (NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(kalorite PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::Network
        KF6::CoreAddons
        KF6::I18n
        KF6::WidgetsAddons
        Qt6::DBus
        Threads::Threads
        ${CMAKE_DL_LIBS}
        nlohmann_json::nlohmann_json
        luajit)
    # LuaJIT needs the executable's symbols exported for its FFI/GC to work.
    target_link_options(kalorite PRIVATE -Wl,-E)
else()
    target_link_libraries(kalorite PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::Network
        nlohmann_json::nlohmann_json
        luajit)
    # Required by LuaJIT on 64-bit macOS.
    target_link_options(kalorite PRIVATE
        -pagezero_size 10000 -image_base 100000000)
endif()


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
