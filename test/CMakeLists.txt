cmake_minimum_required (VERSION 3.1)

project (registerer)

add_compile_options(-Wall -g -O0 -std=c++14)
add_definitions(-DBOOST_LOG_DYN_LINK -DHAVE_STDINT_H)
link_directories(../intel-cmt-cat/lib ../intel-pcm/lib ../fmt/fmt)


# Download and install GoogleTest
include(ExternalProject)
ExternalProject_Add(
	gtest
	GIT_REPOSITORY "git@github.com:google/googletest.git"
	GIT_TAG "master"
	INSTALL_COMMAND ""
	PREFIX ${CMAKE_CURRENT_BINARY_DIR}/gtest
)
ExternalProject_Get_Property(gtest source_dir binary_dir)


# Includes
include_directories(${source_dir}/googletest/include)
include_directories(${source_dir}/googlemock/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../libcpuid/libcpuid)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)
include_directories(/usr/include/glib-2.0)
include_directories(/usr/lib/x86_64-linux-gnu/glib-2.0/include)
include_directories(/usr/lib/glib-2.0/include)
include_directories(../fmt)


# Macro for adding tests
macro(add_gtest test_name)
	target_link_libraries(${test_name} ${binary_dir}/googlemock/gtest/libgtest.a)
	target_link_libraries(${test_name} ${binary_dir}/googlemock/gtest/libgtest_main.a)
	target_link_libraries(${test_name} pthread rt boost_system boost_log boost_log_setup boost_thread boost_filesystem yaml-cpp pqos boost_program_options glib-2.0 pcm fmt dl backtrace)
	add_test(
		NAME    ${test_name}
		COMMAND ${test_name}
	)
endmacro(add_gtest)


# Add tests
add_executable(kmeans_test kmeans_test.cpp ../kmeans.cpp)
add_gtest(kmeans_test)

add_executable(cat-linux_test cat-linux_test.cpp ${CMAKE_CURRENT_BINARY_DIR}/../cat-linux.cpp ${CMAKE_CURRENT_BINARY_DIR}/../cat-intel.cpp ${CMAKE_CURRENT_BINARY_DIR}/../common.cpp)
target_link_libraries(cat-linux_test ${CMAKE_CURRENT_BINARY_DIR}/../libcpuid/libcpuid/.libs/libcpuid.a)
add_gtest(cat-linux_test)


# Make the test runnable with make test
enable_testing()
