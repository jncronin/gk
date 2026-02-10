cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to DEBUG")
    set(CMAKE_BUILD_TYPE DEBUG)
else()
    message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

include(../cmake/utils.cmake)

project(gkos C CXX ASM)


set(LIB_ONLY ON)
add_subdirectory(../../Firmware/lwext4 lwext4)

# hack lwext4 build system a bit to allow us to use our own defines
set(LWEXT4_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/lwext4/include")
file(REMOVE ${LWEXT4_GENERATED_DIR}/generated/ext4_config.h)
target_include_directories(lwext4
PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)
target_include_directories(blockdev
PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

add_subdirectory(../Infineon_AIROC-Wi-Fi-Bluetooth-STM32_1.7.1 airoc)



set(ZLIB_BUILD_EXAMPLES OFF)
add_subdirectory(../../Firmware/zlib-1.3.1 zlib)


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
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Firmware/gk-userlandinterface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Firmware/lwext4/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Firmware/tinyusb/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Firmware/fatfs/source"
        "${CMAKE_CURRENT_SOURCE_DIR}/../cm33-firmware/inc/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Firmware/inih"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        STM32MP257Fxx
        CORE_CM33
        ETH_MODE
        PINS_DONT_RCC_ENABLE
        _POSIX_THREADS
        _POSIX_READER_WRITER_LOCKS
        _UNIX98_THREAD_MUTEX_ATTRIBUTES
        _POSIX_THREAD_PRIO_PROTECT
        _POSIX_THREAD_PROCESS_SHARED
        _GLIBCXX_HAVE_ATOMIC_LOCK_POLICY=1
        __GTHREADS=1
# ensure single threaded gthr is removed
        _GLIBCXX_GCC_GTHR_SINGLE_H
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/logger_printf.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/spinlock.s"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/copy.s"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/pins.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common-a/src/cpuclock.cpp"
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Firmware/fatfs/source/ff.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Firmware/fatfs/source/ffsystem.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Firmware/fatfs/source/ffunicode.c

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
# insist on using thread-aware versions of gthr
            --include=pthread.h
            --include=bits/gthr-posix.h
        >)

target_link_options(${PROJECT_NAME}
    PRIVATE
        -Wl,--wrap=malloc
        -Wl,--wrap=calloc
        -Wl,--wrap=realloc
        -Wl,--wrap=free
        -Wl,--wrap=reallocarray
        -Wl,-z,max-page-size=65536
        -Wl,-z,common-page-size=65536
)

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
        lwext4
        zlib
        airoc_gk_whd
        airoc_btinterface
)

utils_target_set_linker_script(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/gkos.ld
)

utils_target_print_size(${PROJECT_NAME})

# Generate a file suitable for loading into JFlash or equivalent to flash the QSPI
add_custom_target(gkos-hex
    ALL
    COMMAND
        ${CMAKE_STRIP} -S -o ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.strip.elf
            $<TARGET_FILE:${PROJECT_NAME}>
    COMMAND
        ${CMAKE_OBJCOPY} -I binary -O elf64-littleaarch64 --change-addresses 0x60060000
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.strip.elf
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin.elf
    COMMAND
        ${CMAKE_OBJCOPY} -O ihex
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin.elf
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
)
add_dependencies(${PROJECT_NAME}-hex
    ${PROJECT_NAME}
)
