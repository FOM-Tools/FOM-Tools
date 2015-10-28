find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable( fomtest test.cxx )
target_link_libraries( fomtest ${GTEST_LIBRARIES})
install(TARGETS fomtest DESTINATION bin)

add_test(NAME fomtest COMMAND fomtest)
