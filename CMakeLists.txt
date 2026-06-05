cmake_minimum_required(VERSION 3.21)

project(GoogleCalendarAccessory VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

qt_standard_project_setup()

qt_add_executable(GoogleCalendarAccessory
    WIN32
    src/main.cpp
    src/GoogleCalendarClient.cpp
    src/GoogleCalendarClient.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/EventEditDialog.cpp
    src/EventEditDialog.h
    src/CalendarWidget.cpp
    src/CalendarWidget.h
    app.rc
)

target_link_libraries(GoogleCalendarAccessory
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
)

set_target_properties(GoogleCalendarAccessory PROPERTIES
    WIN32_EXECUTABLE TRUE
)
