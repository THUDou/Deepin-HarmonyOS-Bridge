find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus Widgets)

set(CMAKE_AUTOMOC ON)

add_subdirectory(dhb-daemon)
add_subdirectory(dhb-tray)
