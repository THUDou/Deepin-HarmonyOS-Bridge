cmake_minimum_required(VERSION 3.20)

project(dhb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DHB_BUILD_TESTS "Build the DHB unit/integration tests" ON)
option(DHB_BUILD_CLI "Build the dhb command line tool" ON)

add_library(dhb_core STATIC
  src/crypto.cpp
  src/protocol.cpp
  src/transport.cpp
  src/session.cpp
  src/discovery.cpp
)

target_include_directories(dhb_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(dhb_core PUBLIC Threads::Threads)

if(MSVC)
  target_compile_options(dhb_core PRIVATE /W4 /utf-8)
else()
  target_compile_options(dhb_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DHB_BUILD_CLI)
  add_executable(dhb apps/dhb-cli/main.cpp)
  target_link_libraries(dhb PRIVATE dhb_core)
  install(TARGETS dhb RUNTIME DESTINATION bin)
endif()

if(DHB_BUILD_TESTS)
  enable_testing()
  add_executable(dhb_tests tests/dhb_tests.cpp)
  target_link_libraries(dhb_tests PRIVATE dhb_core)
  add_test(NAME dhb_tests COMMAND dhb_tests)
endif()

include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
