cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(test_usb_sd C CXX)

add_executable(test_usb_sd)

target_sources(test_usb_sd
PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_include_directories(test_usb_sd
PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../gkos/inc
)

target_compile_definitions(test_usb_sd
PRIVATE
	GK_UNIT_TEST
)
