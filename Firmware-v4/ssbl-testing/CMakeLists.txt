cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to DEBUG")
    set(CMAKE_BUILD_TYPE DEBUG)
else()
    message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

include(../cmake/utils.cmake)

project(ssbl_testing C CXX ASM)

add_subdirectory(Fusion/Fusion)

add_subdirectory(../Infineon_AIROC-Wi-Fi-Bluetooth-STM32_1.7.1 airoc)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/inc/ddr_init
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../eigen/Eigen
        ${CMAKE_CURRENT_SOURCE_DIR}/IMU_EKF/src
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        STM32MP257Fxx
        CORE_CM33
        ETH_MODE
        PMEM_OFFSET=0
        GKOS_UNCACHED=1
        GKOS_FPU=1
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/elf.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/logger_printf.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/logger_uart.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/spinlock.s
        ${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/copy.s
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
        -Wno-main
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

target_include_directories(airoc_gk_whd
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_include_directories(airoc_btinterface
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_compile_definitions(airoc_gk_whd
PRIVATE
    WPRINT_ENABLE_WHD_INFO
    WPRINT_ENABLE_WHD_DEBUG
    WPRINT_ENABLE_WHD_DATA_LOG
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Fusion
        airoc_gk_whd
        airoc_btinterface
)

target_link_options(${PROJECT_NAME}
    PRIVATE
        -Wl,--wrap=memmove
        -Wl,--wrap=memcpy
        -Wl,--wrap=memset
        -Wl,--wrap=strlen
        -Wl,--wrap=strcmp
)

utils_target_set_linker_script(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/stm32mp2_ssbla.ld
)

utils_target_print_size(${PROJECT_NAME})
utils_target_generate_bin(${PROJECT_NAME})
utils_target_generate_hex(${PROJECT_NAME})
