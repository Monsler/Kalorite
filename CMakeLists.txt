cmake_minimum_required(VERSION 4.02)

project(kalorite VERSION 0.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CDIO_PARANOIA IMPORTED_TARGET libcdio_paranoia)

else()
    find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Multimedia Network)
endif()

add_subdirectory(external/json)

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

file(GLOB_RECURSE SRC_FULL
    CONFIGURE_DEPENDS 
    "src/*.cpp" 
    "include/*.hpp"
)

configure_file(${CMAKE_SOURCE_DIR}/resources.qrc
               ${CMAKE_BINARY_DIR}/resources.qrc
               COPYONLY)

qt_add_executable(kalorite
    ${SRC_FULL}
    ${CMAKE_BINARY_DIR}/resources.qrc
    ${CMAKE_SOURCE_DIR}/assets.qrc)

qt_add_translations(kalorite
    TS_FILES
        languages/kalorite_ru.ts
        languages/kalorite_en.ts

    SOURCES
        ${SRC_FULL}
)

target_include_directories(kalorite PRIVATE include/ ${LUAJIT_DIR}/src)

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
    target_link_options(kalorite PRIVATE -Wl,-E)

    if(CDIO_PARANOIA_FOUND)
        target_link_libraries(kalorite PRIVATE PkgConfig::CDIO_PARANOIA)
        target_compile_definitions(kalorite PRIVATE KALORITE_HAVE_CDDA)
        message(STATUS "Audio CD support: enabled (libcdio_paranoia)")
    else()
        message(STATUS "Audio CD support: disabled (libcdio_paranoia not found)")
    endif()
else()
    target_link_libraries(kalorite PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::Network
        nlohmann_json::nlohmann_json
        luajit)
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
