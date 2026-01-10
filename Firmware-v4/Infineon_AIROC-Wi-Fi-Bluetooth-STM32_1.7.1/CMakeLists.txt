# This folder contains part of the Infineon/STM32 pack
# CMakeLists.txt added for gk based upon Murata 1YN module
# include/source definitions from pdsc file

cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(airoc_gk)

add_library(airoc_gk_module)

target_include_directories(airoc_gk_module
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/resources/firmware/COMPONENT_43439
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/resources/clm/COMPONENT_43439/COMPONENT_MURATA-1YN
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/resources/nvram/COMPONENT_43439/COMPONENT_MURATA-1YN
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-connection-manager/source/COMPONENT_43439
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/resources/resource_imp
)

target_sources(airoc_gk_module
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/resources/firmware/COMPONENT_43439/43439a0_bin.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/resources/firmware/COMPONENT_43439/43439a0-mfgtest_bin.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/resources/clm/COMPONENT_43439/43439A0-mfgtest_clm_blob.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/resources/clm/COMPONENT_43439/COMPONENT_MURATA-1YN/43439A0_clm_blob.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/firmware/COMPONENT_43439/COMPONENT_MURATA-1YN/w_bt_firmware_controller.c
)

add_library(airoc_gk_pal INTERFACE)

target_include_directories(airoc_gk_pal
INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/pal/abstraction-rtos/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/pal/core-lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/pal/hal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/pal/common/Inc
)

#target_sources(airoc_gk_pal
#PRIVATE
#    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/pal/abstraction-rtos/source/cy_worker_thread.c
#    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/pal/common/Src/cyhal_system.c
#    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/pal/common/Src/wifi_bt_if.c
#)

add_library(airoc_gk_whd)

target_link_libraries(airoc_gk_whd
PUBLIC
    airoc_gk_module
    airoc_gk_pal
)

target_compile_options(airoc_gk_whd
PRIVATE
    -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast
)

target_include_directories(airoc_gk_whd
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/resources/resource_imp
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/bus_protocols
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/whd-bsp-integration
)

target_sources(airoc_gk_whd
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/bus_protocols/whd_bus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/bus_protocols/whd_bus_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/bus_protocols/whd_bus_sdio_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/bus_protocols/whd_bus_spi_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/bus_protocols/whd_bus_emusbh_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/bus_protocols/whd_bus_usb_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_ap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_buffer_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_cdc_bdc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_chip.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_chip_constants.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_clm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_debug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_events.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_logging.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_management.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_network_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_proto.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_resource_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_sdpcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_wifi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_wifi_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_wifi_p2p.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_commonring.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_flowring.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_proto.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/src/whd_ring.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/wifi-host-driver/COMPONENT_WIFI5/resources/resource_imp/whd_resources.c
)

add_library(airoc_btinterface)

target_link_libraries(airoc_btinterface
PUBLIC
    airoc_gk_module
    airoc_gk_pal
)

target_compile_options(airoc_btinterface
PRIVATE
    -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast
)

target_include_directories(airoc_btinterface
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/common
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/debug
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack/wiced_include
)

target_sources(airoc_btinterface
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/common/cybt_hci_rx_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/common/cybt_hci_tx_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/common/cybt_host_stack_platform_interface.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/common/cybt_patchram_download.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/common/cybt_platform_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/common/cybt_platform_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/common/cybt_platform_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/common/cybt_prm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/btstack-integration/COMPONENT_HCI-UART/debug/cybt_debug_uart.c
)
