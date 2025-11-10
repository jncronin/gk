cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to DEBUG")
    set(CMAKE_BUILD_TYPE DEBUG)
else()
    message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

include(../cmake/utils.cmake)

project(gkos_sm C CXX ASM)

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
        "${CMAKE_CURRENT_SOURCE_DIR}/../STM32CubeMP2/Drivers/CMSIS/Core_A/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/inc"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        STM32MP257Fxx
        CORE_CM33
        ETH_MODE
        PMEM_OFFSET=0
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/spinlock.s"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/logger_printf.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/logger_uart.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/clocks_cur.cpp"
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
        -Wl,--wrap=malloc
        -Wl,--wrap=calloc
        -Wl,--wrap=realloc
        -Wl,--wrap=free
        -Wl,-z,max-page-size=65536
        -Wl,-z,common-page-size=65536
)

utils_target_set_linker_script(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/gkos_sm.ld
)

utils_target_print_size(${PROJECT_NAME})


# Generate a file suitable for loading into JFlash or equivalent to flash the QSPI
add_custom_target(${PROJECT_NAME}-hex
    ALL
    COMMAND
        ${CMAKE_OBJCOPY} -I binary -O elf64-littleaarch64 --change-addresses 0x60300000
            $<TARGET_FILE:${PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin.elf
    COMMAND
        ${CMAKE_OBJCOPY} -O ihex
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin.elf
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
)
add_dependencies(${PROJECT_NAME}-hex
    ${PROJECT_NAME}
)
