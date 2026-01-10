cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to DEBUG")
    set(CMAKE_BUILD_TYPE DEBUG)
else()
    message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

include(../cmake/utils.cmake)

project(gkv4_fsbl_a C CXX ASM)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.s
)

add_executable(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/../STM32CubeMP2/Drivers/CMSIS/Device/ST/STM32MP2xx/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../STM32CubeMP2/Drivers/CMSIS/Core/Include"
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/inc
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        STM32MP257Fxx
        CORE_CM33
        ETH_MODE
        PMEM_OFFSET=0
        GKOS_UNCACHED=1
        GK_NO_UART_LOCK=1
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/logger_printf.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/logger_uart.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/spinlock.s
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/init_array.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/strlen.cpp
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        SUFFIX ".elf"
        C_STANDARD 11
        C_EXTENSIONS ON
        CMAKE_C_STANDARD_REQUIRED ON
        CXX_STANDARD 20
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
        -Wno-int-to-pointer-cast
        -mgeneral-regs-only
        -mstrict-align
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

target_link_options(${PROJECT_NAME}
    PRIVATE
        -nostdlib
)

utils_target_set_linker_script(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/stm32mp2_fsbla.ld
)

utils_target_print_size(${PROJECT_NAME})
utils_target_generate_bin(${PROJECT_NAME})

find_program(STM32_SigningTool_CLI STM32_SigningTool_CLI REQUIRED)
add_custom_target(${PROJECT_NAME}-stm32
    ALL
    COMMAND
        ${STM32_SigningTool_CLI}
            -in ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
            -nk -of 0x0 -iv 1 -hv 2.2
            -ep 0x0e002600 -s -t fsbl -la 0x0e002600
            -o ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.stm32.bin
)
add_dependencies(${PROJECT_NAME}-stm32
    ${PROJECT_NAME}
)
