#  _________________________________________________________
#  Copyright (c) 2026 Steven Varga, Toronto, ON, Canada
#  MIT License — see LICENSE
#  _________________________________________________________

cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(bench VERSION 0.1.0 LANGUAGES CXX)

add_library(bench INTERFACE)
add_library(bench::bench ALIAS bench)

target_include_directories(bench INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(bench INTERFACE cxx_std_23)

option(BENCH_BUILD_TESTS "Build smoke tests" ON)
if(BENCH_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()

include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS bench EXPORT bench-targets)
# package config / version files are added by the CI + packaging lane.
