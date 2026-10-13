include(FetchContent)

find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
  message(STATUS "Catch2 not found on the system, fetching it")
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1)
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

add_executable(flute_tests
  test_geometry.cpp
  test_hanan_grid.cpp
  test_io.cpp
  test_lookup_table.cpp
  test_net_breaking.cpp
  test_solver.cpp
  test_tree.cpp
  reference.cpp)
target_link_libraries(flute_tests PRIVATE flute::core Catch2::Catch2WithMain)
flute_set_warnings(flute_tests)

include(Catch)
catch_discover_tests(flute_tests)

# End-to-end regression: generate the full lookup table with the command line tool, compare the
# tree lengths of the instances in data/instances with the recorded values and check that
# eight-pin nets are solved exactly. Takes ~10 minutes, hence labelled "slow"
# (run only these with `ctest -L slow`, skip them with `ctest -LE slow`).
add_executable(golden_check golden_check.cpp reference.cpp)
target_include_directories(golden_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(golden_check PRIVATE flute::core)
flute_set_warnings(golden_check)

add_test(NAME cli_generate_full_table
  COMMAND flute generate --output ${CMAKE_CURRENT_BINARY_DIR}/full_lookup.txt --force --quiet)
set_tests_properties(cli_generate_full_table PROPERTIES
  FIXTURES_SETUP full_lookup_table LABELS "slow" TIMEOUT 3600)

add_test(NAME golden_lengths
  COMMAND golden_check ${CMAKE_CURRENT_BINARY_DIR}/full_lookup.txt ${PROJECT_SOURCE_DIR}/data)
set_tests_properties(golden_lengths PROPERTIES
  FIXTURES_REQUIRED full_lookup_table LABELS "slow" TIMEOUT 600)
