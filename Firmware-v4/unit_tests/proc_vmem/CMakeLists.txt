cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(test_proc_vmem CXX)

add_executable(test_proc_vmem)

target_sources(test_proc_vmem
PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../../gkos/src/proc_vmem_map_alloc.cpp
)

target_include_directories(test_proc_vmem
PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../gkos/inc
	${CMAKE_CURRENT_SOURCE_DIR}/../../common-a/inc
	${CMAKE_CURRENT_SOURCE_DIR}/../../../Firmware/gk-userlandinterface
)

set_target_properties(test_proc_vmem
PROPERTIES
	CXX_STANDARD 20
)

target_compile_definitions(test_proc_vmem
PRIVATE
	__GK_UNIT_TEST__=1
	__GAMEKID__=4
)
