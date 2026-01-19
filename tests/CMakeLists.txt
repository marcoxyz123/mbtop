# SPDX-License-Identifier: Apache-2.0

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
  FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)

add_library(libmbtop_test)
target_include_directories(libmbtop_test PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(libmbtop_test libmbtop GTest::gtest_main)

add_executable(mbtop_test tools.cpp)
target_link_libraries(mbtop_test libmbtop_test)

include(GoogleTest)
gtest_discover_tests(mbtop_test)
