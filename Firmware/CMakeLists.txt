cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to DEBUG")
    set(CMAKE_BUILD_TYPE DEBUG)
else()
    message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

include(cmake/utils.cmake)

project(gk C CXX ASM)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.s
)

add_executable(${PROJECT_NAME})


target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_device_h7/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_Core/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/gk-userlandinterface"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        STM32H747xx
        CORE_CM7
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${SOURCES}
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        SUFFIX ".elf"
        C_STANDARD 11
        C_EXTENSIONS ON
        CMAKE_C_STANDARD_REQUIRED ON
        CXX_STANDARD 17
        CXX_EXTENSIONS ON
        CMAKE_CXX_STANDARD_REQUIRED ON
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wduplicated-cond
        -Wno-duplicated-branches
        -Wlogical-op
        -Wnull-dereference
        -Wshadow
        -Wno-unused-parameter
        -Wno-psabi
        -Wno-missing-field-initializers
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        $<$<COMPILE_LANGUAGE:C>:
            -Wstrict-prototypes
            -Wno-missing-prototypes
            -Wno-address
        >)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:
            -Wno-register
        >)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        -lm
)

target_link_options(${PROJECT_NAME}
    PRIVATE
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map,--cref
        -Wl,--wrap,malloc
        -Wl,--wrap,_malloc_r
        -Wl,--wrap,calloc
        -Wl,--wrap,_calloc_r
        -Wl,--wrap,realloc
        -Wl,--wrap,_realloc_r
        -Wl,--wrap,free
        -Wl,--wrap,_free_r
)

utils_target_set_linker_script(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32H747BIx_FLASH.ld
)

utils_target_print_size(${PROJECT_NAME})
