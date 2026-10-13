cmake_minimum_required(VERSION 3.20)

project(flute
  VERSION 1.0.0
  DESCRIPTION "Fast lookup-table based rectilinear Steiner minimal trees (FLUTE)"
  LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FLUTE_BUILD_TESTS "Build the unit tests" ON)
option(FLUTE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(FLUTE_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

include(cmake/CompilerWarnings.cmake)

find_package(Threads REQUIRED)

add_library(flute_core
  src/geometry.cpp
  src/hanan_grid.cpp
  src/io.cpp
  src/lookup_table.cpp
  src/lut_generator.cpp
  src/net_breaking.cpp
  src/solver.cpp
  src/tree.cpp
  src/verify.cpp)
add_library(flute::core ALIAS flute_core)
target_include_directories(flute_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(flute_core PUBLIC Threads::Threads)
target_compile_features(flute_core PUBLIC cxx_std_17)
flute_set_warnings(flute_core)

add_executable(flute apps/flute_cli.cpp)
target_link_libraries(flute PRIVATE flute::core)
target_compile_definitions(flute PRIVATE FLUTE_VERSION="${PROJECT_VERSION}")
flute_set_warnings(flute)

add_executable(flute_bench apps/flute_bench.cpp)
target_link_libraries(flute_bench PRIVATE flute::core)
flute_set_warnings(flute_bench)

if(FLUTE_ENABLE_SANITIZERS AND NOT MSVC)
  foreach(target flute_core flute flute_bench)
    target_compile_options(${target} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
  endforeach()
endif()

if(FLUTE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS flute RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
