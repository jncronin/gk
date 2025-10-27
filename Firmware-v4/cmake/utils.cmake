macro(utils_target_generate_hex TARGET_NAME)
    add_custom_target(${TARGET_NAME}-hex
        ALL
        COMMAND
            ${CMAKE_OBJCOPY} -O ihex
                $<TARGET_FILE:${TARGET_NAME}>
                ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.hex
    )
    add_dependencies(${TARGET_NAME}-hex
        ${TARGET_NAME}
    )
endmacro()

macro(utils_target_generate_bin TARGET_NAME)
    add_custom_target(${TARGET_NAME}-bin
        ALL
        COMMAND
            ${CMAKE_OBJCOPY} -O binary
                $<TARGET_FILE:${TARGET_NAME}>
                ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.bin
    )
    add_dependencies(${TARGET_NAME}-bin
        ${TARGET_NAME}
    )
endmacro()

macro(utils_target_print_size TARGET_NAME)
    add_custom_command(TARGET ${TARGET_NAME}
        POST_BUILD
            COMMAND
                ${CMAKE_SIZE_UTIL} --format=sysv --radix=16 $<TARGET_FILE:${TARGET_NAME}>
    )
endmacro()


macro(utils_target_set_linker_script TARGET_NAME LINKER_SCRIPT)
    target_link_options(${TARGET_NAME}
        PRIVATE
            -T ${LINKER_SCRIPT}
    )
    set_target_properties(${TARGET_NAME}
        PROPERTIES
            LINK_DEPENDS
                ${LINKER_SCRIPT}
    )
endmacro()
