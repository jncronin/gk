cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

include(../cmake/utils.cmake)

project(ospi_flasher)

add_executable(ospi_flasher)
target_sources(ospi_flasher
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/FlashDev.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/FlashPrg.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/pins.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/logger_printf.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/logger_uart.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/strlen.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/memcpy.cpp"
)

set_target_properties(ospi_flasher
    PROPERTIES
        SUFFIX ".FLM"
        C_STANDARD 11
        C_EXTENSIONS ON
        CMAKE_C_STANDARD_REQUIRED ON
        CXX_STANDARD 20
        CXX_EXTENSIONS ON
        CMAKE_CXX_STANDARD_REQUIRED ON
)

target_compile_options(ospi_flasher
    PRIVATE
        -fpic
        -g
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
        -ffreestanding
)

target_link_options(ospi_flasher
    PRIVATE
        -fpic
        -nostdlib
        -Wl,-e,0
)

target_compile_definitions(ospi_flasher
    PRIVATE
        STM32MP257Fxx
        CORE_CM33
        ETH_MODE
        PMEM_OFFSET=0
        GKOS_UNCACHED=1
        GK_NO_UART_LOCK=1
        GK_FLASH_LOADER=1
)


target_include_directories(ospi_flasher
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../STM32CubeMP2/Drivers/CMSIS/Device/ST/STM32MP2xx/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../STM32CubeMP2/Drivers/CMSIS/Core/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../STM32CubeMP2/Drivers/CMSIS/Core_A/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/inc"
)

utils_target_set_linker_script(ospi_flasher
    ${CMAKE_CURRENT_SOURCE_DIR}/stm32mp2-ospi-flasher.ld
)

