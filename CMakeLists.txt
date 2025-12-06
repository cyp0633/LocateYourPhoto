cmake_minimum_required(VERSION 3.20)
project(LocateYourPhoto VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Quick
    QuickWidgets
    Positioning
    Location
)

# Find exiv2
find_package(PkgConfig REQUIRED)
pkg_check_modules(EXIV2 REQUIRED exiv2)

# Find pugixml for GPX parsing
find_package(pugixml QUIET)
if(NOT pugixml_FOUND)
    pkg_check_modules(PUGIXML REQUIRED pugixml)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/core/gpx_parser.cpp
    src/core/exif_handler.cpp
    src/core/exiftool_writer.cpp
    src/core/gps_matcher.cpp
    src/core/photo_processor.cpp
    src/ui/main_window.cpp
    src/ui/file_list_panel.cpp
    src/ui/map_panel.cpp
    src/models/photo_list_model.cpp
)

set(HEADERS
    src/core/gpx_parser.h
    src/core/exif_handler.h
    src/core/exiftool_writer.h
    src/core/gps_matcher.h
    src/core/photo_processor.h
    src/models/track_point.h
    src/models/photo_item.h
    src/models/photo_list_model.h
    src/ui/main_window.h
    src/ui/file_list_panel.h
    src/ui/map_panel.h
)

# Qt resources (QML files)
set(QML_FILES
    src/qml/MapView.qml
)

qt6_add_resources(RESOURCES resources.qrc)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${EXIV2_INCLUDE_DIRS}
    ${PUGIXML_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::Positioning
    Qt6::Location
    ${EXIV2_LIBRARIES}
)

# Link pugixml
if(pugixml_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE pugixml::pugixml)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PUGIXML_LIBRARIES})
endif()

# Install target
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
