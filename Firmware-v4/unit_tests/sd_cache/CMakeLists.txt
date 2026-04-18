cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(test_sd_cache C CXX)

add_executable(test_sd_cache)

target_sources(test_sd_cache
PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../gkos/src/sdcache.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unittest_mem.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unittest_mutex.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_include_directories(test_sd_cache
PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../gkos/inc
)

target_compile_definitions(test_sd_cache
PRIVATE
	GK_UNIT_TEST
)
