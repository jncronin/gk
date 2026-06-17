cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(test_pmem_buddy CXX)

add_executable(test_pmem_buddy)

target_sources(test_pmem_buddy
PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_include_directories(test_pmem_buddy
PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../gkos/inc
	${CMAKE_CURRENT_SOURCE_DIR}/../../common-a/inc
	${CMAKE_CURRENT_SOURCE_DIR}/../../../Firmware/gk-userlandinterface
)

set_target_properties(test_pmem_buddy
PROPERTIES
	CXX_STANDARD 20
)

target_compile_definitions(test_pmem_buddy
PRIVATE
	__GK_UNIT_TEST__=1
	__GAMEKID__=4
)
