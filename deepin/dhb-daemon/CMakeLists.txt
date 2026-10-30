add_executable(dhb-daemon ${PROJECT_SOURCE_DIR}/apps/dhb-daemon/main.cpp)
target_link_libraries(dhb-daemon PRIVATE dhb_core Qt6::Core Qt6::DBus)
install(TARGETS dhb-daemon RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
