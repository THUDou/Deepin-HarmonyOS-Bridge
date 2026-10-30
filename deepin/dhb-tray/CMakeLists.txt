add_library(dhb-tray MODULE dhb-tray.cpp)
target_include_directories(dhb-tray PRIVATE /usr/include/dde-dock)
target_link_libraries(dhb-tray PRIVATE Qt6::Widgets)
set_target_properties(dhb-tray PROPERTIES
  PREFIX ""
  OUTPUT_NAME "dhb-tray"
)
install(TARGETS dhb-tray LIBRARY DESTINATION lib/dde-dock/plugins)
