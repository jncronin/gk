cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

add_executable(xspi_flasher)
target_sources(xspi_flasher
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/FlashDev.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/FlashPrg.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/pins.cpp"
)

set_target_properties(xspi_flasher
    PROPERTIES
        SUFFIX ".FLM"
        C_STANDARD 11
        C_EXTENSIONS ON
        CMAKE_C_STANDARD_REQUIRED ON
        CXX_STANDARD 17
        CXX_EXTENSIONS ON
        CMAKE_CXX_STANDARD_REQUIRED ON
)

target_compile_options(xspi_flasher
    PRIVATE
        -fpic
)

target_link_options(xspi_flasher
    PRIVATE
        -fpic
        -nostdlib
        -Wl,-e,0
)

target_compile_definitions(xspi_flasher
    PRIVATE
        STM32H7S7xx
)


target_include_directories(xspi_flasher
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/../cmsis_device_h7rs/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../cmsis_core/Include"
)

utils_target_set_linker_script(xspi_flasher
    ${CMAKE_CURRENT_SOURCE_DIR}/stm32h7s7xx_flash-xspi-flasher.ld
)

