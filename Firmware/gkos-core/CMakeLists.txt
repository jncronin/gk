cmake_minimum_required(VERSION 3.17 FATAL_ERROR)    # for ZIP_LISTS

project(gkos-core C CXX ASM)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# define the libraries to build.  Default for STM32H747 is to build generic, m4 and m7
list(APPEND lib_names           generic m7      m4)
list(APPEND lib_core_masks      3       1       2)

foreach(lib_name lib_core_mask IN ZIP_LISTS lib_names lib_core_masks)
    set(gk_name "gkos-core-${lib_name}")
    message(STATUS "Generating code for ${gk_name}, core_mask ${lib_core_mask}")

    add_library(${gk_name})

    target_include_directories(${gk_name}
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/inc"
            "${CMAKE_CURRENT_SOURCE_DIR}/gk-userlandinterface"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_device_h7/Include"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_Core/Include"
    )

    target_compile_definitions(${gk_name}
        PRIVATE
            _POSIX_READER_WRITER_LOCKS
            _UNIX98_THREAD_MUTEX_ATTRIBUTES
            STM32H747xx
            GKOS_CODE_CORE=${lib_core_mask}
    )

    target_sources(${gk_name}
        PRIVATE
            ${SOURCES}
    )

    set_target_properties(${gk_name}
        PROPERTIES
            C_STANDARD 11
            C_EXTENSIONS ON
            CMAKE_C_STANDARD_REQUIRED ON
            CXX_STANDARD 17
            CXX_EXTENSIONS ON
            CMAKE_CXX_STANDARD_REQUIRED ON
    )

    target_compile_options(${gk_name}
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
endforeach()
