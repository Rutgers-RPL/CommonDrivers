add_executable(common_drivers_tests)

add_subdirectory(sensors)
add_subdirectory(mocks)
target_link_libraries(common_drivers_tests PRIVATE common_drivers fff GTest::gtest_main gmock)

target_compile_options(common_drivers_tests PRIVATE --coverage)
target_link_options(common_drivers_tests PRIVATE --coverage)

include(GoogleTest)
gtest_discover_tests(common_drivers_tests)
