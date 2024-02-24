cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(gk-jlink-rtos C CXX)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(${PROJECT_NAME} SHARED)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${SOURCES}
)

target_link_options(${PROJECT_NAME}
    PRIVATE
        /INCREMENTAL:NO)
