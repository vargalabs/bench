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

# Optional external plot:: library (vargalabs/plots). When present, the opt-in
# SVG sink example/test (include/bench/sink/svg.hpp -> <plot/all>) is built;
# when absent the bench core still configures, builds, and tests cleanly.
find_package(plot CONFIG QUIET)

option(BENCH_BUILD_TESTS "Build smoke tests" ON)
if(BENCH_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()

option(BENCH_BUILD_EXAMPLES "Build examples (incl. the svg_sink heatmap demo)" ON)
if(BENCH_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS bench EXPORT bench-targets
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# package config / version files
include(CMakePackageConfigHelpers)

set(BENCH_INSTALL_CMAKEDIR ${CMAKE_INSTALL_DATADIR}/cmake/bench)

configure_package_config_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/bench-config.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/bench-config.cmake
	INSTALL_DESTINATION ${BENCH_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/bench-config-version.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
	ARCH_INDEPENDENT
)

install(EXPORT bench-targets
	FILE bench-targets.cmake
	NAMESPACE bench::
	DESTINATION ${BENCH_INSTALL_CMAKEDIR}
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/bench-config.cmake
	${CMAKE_CURRENT_BINARY_DIR}/bench-config-version.cmake
	DESTINATION ${BENCH_INSTALL_CMAKEDIR}
)
