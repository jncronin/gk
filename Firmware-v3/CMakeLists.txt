cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to DEBUG")
    set(CMAKE_BUILD_TYPE DEBUG)
else()
    message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

include(cmake/utils.cmake)

add_definitions(-DCONFIG_BLOCK_DEV_CACHE_SIZE=32)
add_definitions(-DCONFIG_HAVE_OWN_OFLAGS=0)
add_subdirectory(../Firmware/lwext4 lwext4)
set(LWEXT4_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/lwext4/include")

set(ZLIB_BUILD_EXAMPLES OFF)
#add_subdirectory(zlib-1.3.1)

project(gkv3 C CXX ASM)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.s
    ${CMAKE_CURRENT_SOURCE_DIR}/../Firmware/fatfs/source/ff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Firmware/fatfs/source/ffsystem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Firmware/fatfs/source/ffunicode.c
)

file(GLOB_RECURSE TUSB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/lib/networking/rndis_reports.c
)

file(GLOB_RECURSE WINC_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/winc1500/common/source/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/winc1500/driver/source/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/winc1500/socket/source/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/winc1500/spi_flash/source/*.c
)

#[[
add_library(tusb)
target_include_directories(tusb
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_device_h7/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_Core/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/gk-userlandinterface"
        "${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/lib/networking"
)
target_compile_definitions(tusb
    PRIVATE
        STM32H747xx
        CORE_CM7
        ETH_MODE
        _POSIX_READER_WRITER_LOCKS
        _UNIX98_THREAD_MUTEX_ATTRIBUTES
        _POSIX_THREAD_PRIO_PROTECT
        _POSIX_THREAD_PROCESS_SHARED

)

target_sources(tusb
    PRIVATE
        ${TUSB_SOURCES}
)

add_library(winc)
target_include_directories(winc
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_device_h7/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_Core/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/winc1500"
)
target_compile_definitions(winc
    PRIVATE
    STM32H747xx
    CORE_CM7
    ETH_MODE
    _POSIX_READER_WRITER_LOCKS
    _UNIX98_THREAD_MUTEX_ATTRIBUTES
    _POSIX_THREAD_PRIO_PROTECT
    _POSIX_THREAD_PROCESS_SHARED

)

target_sources(winc
PRIVATE
    ${WINC_SOURCES}
)
]]

add_executable(${PROJECT_NAME})


target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_device_h7rs/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_core/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/gk-userlandinterface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Firmware/lwext4/include"
        "${LWEXT4_GENERATED_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/winc1500"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Firmware/fatfs/source"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        STM32H7S7xx
        CORE_CM7
        ETH_MODE
        _POSIX_READER_WRITER_LOCKS
        _UNIX98_THREAD_MUTEX_ATTRIBUTES
        _POSIX_THREAD_PRIO_PROTECT
        _POSIX_THREAD_PROCESS_SHARED

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
        #lwext4
        #tusb
        #winc
        #zlib
        #lvgl

)

target_link_options(${PROJECT_NAME}
    PRIVATE
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map,--cref


#        -Wl,--wrap,malloc
#        -Wl,--wrap,_malloc_r
#        -Wl,--wrap,calloc
#        -Wl,--wrap,_calloc_r
#        -Wl,--wrap,realloc
#        -Wl,--wrap,_realloc_r
#        -Wl,--wrap,free
#        -Wl,--wrap,_free_r
)

utils_target_set_linker_script(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/stm32h7s7xx_flash.ld
)

utils_target_print_size(${PROJECT_NAME})

add_subdirectory(xspi_flasher)
