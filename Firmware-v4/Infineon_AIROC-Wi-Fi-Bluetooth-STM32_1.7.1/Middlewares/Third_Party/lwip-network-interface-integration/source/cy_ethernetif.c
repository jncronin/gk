/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
*/

#if defined(CYBSP_ETHERNET_CAPABLE)

#include <string.h>
#include <stdint.h>
#include "lwipopts.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/init.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/tcpip.h"
#include "lwip/ethip6.h"
#include "lwip/igmp.h"
#include "lwip/nd6.h"
#include "netif/ethernet.h"
#include "lwip/prot/autoip.h"
#include "lwip/prot/dhcp.h"
#include "lwip/dns.h"
#include "lwip/inet_chksum.h"

#include "lwip/icmp.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

#include "cy_lwip_dhcp_server.h"
#include "cy_network_mw_core.h"
#include "cy_result.h"
#include "cy_lwip_log.h"

#ifdef COMPONENT_CAT3
#include "xmc_gpio.h"
#include "xmc_eth_mac.h"
#include "xmc_eth_phy.h"
#include "cy_eth_config.h"
#include "cyabs_rtos.h"
#endif

#ifdef COMPONENT_CAT1
#include "cy_ecm.h"
#include "cy_internal.h"
#endif

/******************************************************
 *                      Macros
 ******************************************************/
/**
 * Suppress unused parameter warning
 */
#define UNUSED_PARAMETER(x) ( (void)(x) )

/**
 * Suppress unused variable warning
 */
#define UNUSED_VARIABLE(x) ( (void)(x) )

#define EAPOL_PACKET_TYPE                        0x888E

#define MULTICAST_IP_TO_MAC(ip)                  {   (uint8_t) 0x01,             \
                                                     (uint8_t) 0x00,             \
                                                     (uint8_t) 0x5e,             \
                                                     (uint8_t) ((ip)[1] & 0x7F), \
                                                     (uint8_t) (ip)[2],          \
                                                     (uint8_t) (ip)[3]           \
                                                 }
#define IPV6_MULTICAST_TO_MAC_PREFIX             (0x33)

#define MAX_AUTO_IP_RETRIES                      (5)

#define MAX_ETHERNET_PORT                        (2U)
#define ETH_INTERFACE_INDEX                      (2U)
#define MAX_ETH_FRAME_SIZE                       (1500U)
/* MAC address*/
#define MAC_ADDR0                                (0x00U)
#define MAC_ADDR1                                (0x03U)
#define MAC_ADDR2                                (0x19U)
#define MAC_ADDR3                                (0x45U)
#define MAC_ADDR4                                (0x00U)
#define MAC_ADDR5                                (0x00U)

#define MAC_ADDR                                 ((uint64_t)MAC_ADDR0 | \
                                                  ((uint64_t)MAC_ADDR1 << 8) | \
                                                  ((uint64_t)MAC_ADDR2 << 16) | \
                                                  ((uint64_t)MAC_ADDR3 << 24) | \
                                                  ((uint64_t)MAC_ADDR4 << 32) | \
                                                  ((uint64_t)MAC_ADDR5 << 40))

#define ETH_LWIP_0_PHY_ADDR                      (0)

#define ETH_LWIP_0_NUM_RX_BUF                    (4U)
#define ETH_LWIP_0_NUM_TX_BUF                    (4U)

/* Define those to better describe your network interface. */
#define IFNAME0                                  'e'
#define IFNAME1                                  'n'

/*Maximum retry iterations for phy auto-negotiation*/
#define ETH_LWIP_PHY_MAX_RETRIES                 0xfffffU

#define ETH_MIN_FRAME_SIZE                      (64)
#define ETH_MAX_FRAME_SIZE                      (1518)
#define ETH_FRAME_SIZE_FCS                      (4) // (This is constant value)
#define ETH_FRAME_SIZE_HEADER                   (14) // 6 bytes for destination address, 6 bytes for source address, 2 bytes for Type (This is constant.)

#ifdef COMPONENT_CAT3
#define CY_ETH_SIZE_MAX_FRAME                   (1500)

#define ETH_LWIP_0_CRS_DV  XMC_GPIO_PORT15, 9U
#define ETH_LWIP_0_RXER  XMC_GPIO_PORT2, 4U
#define ETH_LWIP_0_RXD0  XMC_GPIO_PORT2, 2U
#define ETH_LWIP_0_RXD1  XMC_GPIO_PORT2, 3U
#define ETH_LWIP_0_TXEN  XMC_GPIO_PORT2, 5U
#define ETH_LWIP_0_TXD0  XMC_GPIO_PORT2, 8U
#define ETH_LWIP_0_TXD1  XMC_GPIO_PORT2, 9U
#define ETH_LWIP_0_RMII_CLK  XMC_GPIO_PORT15, 8U
#define ETH_LWIP_0_MDC  XMC_GPIO_PORT2, 7U
#define ETH_LWIP_0_MDIO  XMC_GPIO_PORT2, 0U
#define ETH_LWIP_0_PIN_LIST_SIZE 10U

#ifdef ENABLE_ECM_LOGS
    #define RX_STATUS_THREAD_STACK_SIZE         ((1024 * 1) + (1024 * 3)) /* Additional 3kb of stack is added for enabling the prints */
#else
    #define RX_STATUS_THREAD_STACK_SIZE         (1024)
#endif
#define RX_EVENT_THREAD_STACK_SIZE              (1024 * 4)
#define RX_EVENT_THREAD_PRIORITY                (CY_RTOS_PRIORITY_ABOVENORMAL)
#define RX_STATUS_THREAD_PRIORITY               (CY_RTOS_PRIORITY_BELOWNORMAL)
#endif

#ifdef COMPONENT_CAT1
#define TX_DATA_BUF_MAX                         CY_ETH_DEFINE_TOTAL_BD_PER_TXQUEUE
#define TX_SEMAPHORE_TIMEOUT_MS                 120000
#endif
/******************************************************
 *               Variable Definitions
 ******************************************************/
#ifdef COMPONENT_CAT1
/* PDL Driver requires the data to be 32 byte aligned */
CY_SECTION_SHAREDMEM
CY_ALIGN(32)
static   uint8_t                              data_buffer[TX_DATA_BUF_MAX][CY_ETH_SIZE_MAX_FRAME];
volatile uint8_t                              tx_free_buf_index;
static   uint8_t                              tx_buf_available;
extern   cy_semaphore_t                       tx_semaphore;
extern   cy_mutex_t                           tx_mutex;
extern   cy_worker_thread_info_t              cy_tx_worker_thread;
static   cy_network_activity_event_callback_t activity_callback = NULL;
extern   cy_network_interface_context         iface_context_database[CY_IFACE_MAX_HANDLE];
extern   cy_worker_thread_info_t              cy_rx_worker_thread;
extern   cy_queue_t                           rx_input_buffer_queue;
LWIP_MEMPOOL_DECLARE(RX_POOL, NO_OF_MEMPOOL_ELEMENTS, sizeof(my_custom_pbuf_t), "Zero-copy RX PBUF pool");
#endif

#ifdef COMPONENT_CAT3
static cy_semaphore_t           rx_semaphore = NULL;
static cy_thread_t              rx_event_thread = NULL;
static cy_thread_t              link_status_thread = NULL;

#if defined(__ICCARM__)
#pragma data_alignment=4
static XMC_ETH_MAC_DMA_DESC_t ETH_LWIP_0_rx_desc[ETH_LWIP_0_NUM_RX_BUF];
#pragma data_alignment=4
static XMC_ETH_MAC_DMA_DESC_t ETH_LWIP_0_tx_desc[ETH_LWIP_0_NUM_TX_BUF];
#pragma data_alignment=4
static uint8_t ETH_LWIP_0_rx_buf[ETH_LWIP_0_NUM_RX_BUF][XMC_ETH_MAC_BUF_SIZE];
#pragma data_alignment=4
static uint8_t ETH_LWIP_0_tx_buf[ETH_LWIP_0_NUM_TX_BUF][XMC_ETH_MAC_BUF_SIZE];
#elif defined(__CC_ARM)
static __attribute__((aligned(4))) XMC_ETH_MAC_DMA_DESC_t ETH_LWIP_0_rx_desc[ETH_LWIP_0_NUM_RX_BUF] __attribute__((section ("RW_IRAM1")));
static __attribute__((aligned(4))) XMC_ETH_MAC_DMA_DESC_t ETH_LWIP_0_tx_desc[ETH_LWIP_0_NUM_TX_BUF] __attribute__((section ("RW_IRAM1")));
static __attribute__((aligned(4))) uint8_t ETH_LWIP_0_rx_buf[ETH_LWIP_0_NUM_RX_BUF][XMC_ETH_MAC_BUF_SIZE] __attribute__((section ("RW_IRAM1")));
static __attribute__((aligned(4))) uint8_t ETH_LWIP_0_tx_buf[ETH_LWIP_0_NUM_TX_BUF][XMC_ETH_MAC_BUF_SIZE] __attribute__((section ("RW_IRAM1")));
#elif defined(__GNUC__)
static __attribute__((aligned(4))) XMC_ETH_MAC_DMA_DESC_t ETH_LWIP_0_rx_desc[ETH_LWIP_0_NUM_RX_BUF] __attribute__((section ("ETH_RAM")));
static __attribute__((aligned(4))) XMC_ETH_MAC_DMA_DESC_t ETH_LWIP_0_tx_desc[ETH_LWIP_0_NUM_TX_BUF] __attribute__((section ("ETH_RAM")));
static __attribute__((aligned(4))) uint8_t ETH_LWIP_0_rx_buf[ETH_LWIP_0_NUM_RX_BUF][XMC_ETH_MAC_BUF_SIZE] __attribute__((section ("ETH_RAM")));
static __attribute__((aligned(4))) uint8_t ETH_LWIP_0_tx_buf[ETH_LWIP_0_NUM_TX_BUF][XMC_ETH_MAC_BUF_SIZE] __attribute__((section ("ETH_RAM")));
#else
static __attribute__((aligned(4))) XMC_ETH_MAC_DMA_DESC_t ETH_LWIP_0_rx_desc[ETH_LWIP_0_NUM_RX_BUF];
static __attribute__((aligned(4))) XMC_ETH_MAC_DMA_DESC_t ETH_LWIP_0_tx_desc[ETH_LWIP_0_NUM_TX_BUF];
static __attribute__((aligned(4))) uint8_t ETH_LWIP_0_rx_buf[ETH_LWIP_0_NUM_RX_BUF][XMC_ETH_MAC_BUF_SIZE];
static __attribute__((aligned(4))) uint8_t ETH_LWIP_0_tx_buf[ETH_LWIP_0_NUM_TX_BUF][XMC_ETH_MAC_BUF_SIZE];
#endif

const XMC_ETH_PHY_CONFIG_t eth_phy_config =
{
    .interface = XMC_ETH_LINK_INTERFACE_RMII,
    .enable_auto_negotiate = true
};

XMC_ETH_MAC_t eth_mac =
{
    .regs = ETH0,
    .address = MAC_ADDR,
    .rx_desc = ETH_LWIP_0_rx_desc,
    .tx_desc = ETH_LWIP_0_tx_desc,
    .rx_buf = &ETH_LWIP_0_rx_buf[0][0],
    .tx_buf = &ETH_LWIP_0_tx_buf[0][0],
    .num_rx_buf = ETH_LWIP_0_NUM_RX_BUF,
    .num_tx_buf = ETH_LWIP_0_NUM_TX_BUF
};

static struct netif *xnetif = NULL;
#endif
/******************************************************
 *               Static Function Declarations
 ******************************************************/
#ifdef COMPONENT_CAT1
void cy_process_ethernet_data_cb(ETH_Type *eth_type, uint8_t * rx_buffer, uint32_t length);
void cy_notify_ethernet_rx_data_cb(ETH_Type *base, uint8_t **u8RxBuffer, uint32_t *u32Length);
void cy_tx_complete_cb ( ETH_Type *pstcEth, uint8_t u8QueueIndex );
void cy_tx_failure_cb ( ETH_Type *pstcEth, uint8_t u8QueueIndex );
#endif

static err_t ethif_output(struct netif *netif, struct pbuf *p);
err_t ethernetif_init(struct netif *netif);

#ifdef COMPONENT_CAT3
static void low_level_init(struct netif *netif);
static struct pbuf * low_level_input(void);
static void ethernetif_input(void *arg);

#if LWIP_NETIF_LINK_CALLBACK == 1
static void ethernetif_link_callback(struct netif *netif);
static void ethernetif_link_status(void *args);
#endif
#endif
/******************************************************
 *               Function Definitions
 ******************************************************/
#ifdef COMPONENT_CAT3
/*Weak function to be called in case of error*/
__WEAK void ETH_LWIP_Error (ETH_LWIP_ERROR_t error_code)
{
    switch (error_code)
    {
        case ETH_LWIP_ERROR_PHY_DEVICE_ID:
        {
            /* Incorrect PHY address configured in the ETH_LWIP APP network interface.
            * Because the connect PHY does not match the configuration or the PHYADR is incorrect*/
            cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "%s(): Wrong PHY ID used \n", __FUNCTION__ );
            break;
        }

        case ETH_LWIP_ERROR_PHY_TIMEOUT:
        {
            /* PHY did not respond.*/
            cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "%s(): PHY read failed \n", __FUNCTION__ );
            break;
        }

        case ETH_LWIP_ERROR_PHY_ERROR:
        {
            /*PHY register update failed.*/
            cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "%s(): PHY status error \n", __FUNCTION__ );
            break;
        }

        case ETH_LWIP_ERROR_PHY_BUSY:
        {
            cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "%s(): PHY is busy \n", __FUNCTION__ );
            break;
        }
        default:
        {
            cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "%s(): Generic PHY error \n", __FUNCTION__ );
            break;
        }
    }
    return;
}

#if LWIP_NETIF_LINK_CALLBACK == 1
static void ethernetif_link_callback(struct netif *netif)
{
    XMC_ETH_LINK_SPEED_t speed;
    XMC_ETH_LINK_DUPLEX_t duplex;
    bool phy_autoneg_state;
    uint32_t retries = 0U;
    int32_t status;

    if (netif_is_link_up(netif))
    {
        if((status = XMC_ETH_PHY_Init(&eth_mac, ETH_LWIP_0_PHY_ADDR, &eth_phy_config)) != XMC_ETH_PHY_STATUS_OK)
        {
            ETH_LWIP_Error((ETH_LWIP_ERROR_t)status);
        }

        /* If autonegotiation is enabled */
        do {
            phy_autoneg_state = XMC_ETH_PHY_IsAutonegotiationCompleted(&eth_mac, ETH_LWIP_0_PHY_ADDR);
            retries++;
        } while ((phy_autoneg_state == false) && (retries < ETH_LWIP_PHY_MAX_RETRIES));

        if(phy_autoneg_state == false)
        {
            ETH_LWIP_Error(ETH_LWIP_ERROR_PHY_TIMEOUT);
        }

        speed = XMC_ETH_PHY_GetLinkSpeed(&eth_mac, ETH_LWIP_0_PHY_ADDR);
        duplex = XMC_ETH_PHY_GetLinkDuplex(&eth_mac, ETH_LWIP_0_PHY_ADDR);

        XMC_ETH_MAC_SetLink(&eth_mac, speed, duplex);
        /* Enable Ethernet interrupts */
        XMC_ETH_MAC_EnableEvent(&eth_mac, (uint32_t)XMC_ETH_MAC_EVENT_RECEIVE);

        NVIC_SetPriority((IRQn_Type)108, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 62U, 0U));
        NVIC_ClearPendingIRQ((IRQn_Type)108);
        NVIC_EnableIRQ((IRQn_Type)108);
        XMC_ETH_MAC_EnableTx(&eth_mac);
        XMC_ETH_MAC_EnableRx(&eth_mac);

        netifapi_netif_set_up(xnetif);

    }
    else
    {
        /* Disable Ethernet interrupts */
        XMC_ETH_MAC_DisableEvent(&eth_mac, (uint32_t)XMC_ETH_MAC_EVENT_RECEIVE);
        NVIC_DisableIRQ((IRQn_Type)108);

        XMC_ETH_MAC_DisableTx(&eth_mac);
        XMC_ETH_MAC_DisableRx(&eth_mac);

        netifapi_netif_set_down(xnetif);

    }
}
#endif

static void ethernetif_link_status(void *args)
{
    UNUSED_PARAMETER(args);

    while(1)
    {
        if (XMC_ETH_PHY_GetLinkStatus(&eth_mac, ETH_LWIP_0_PHY_ADDR) == XMC_ETH_LINK_STATUS_DOWN)
        {
            if (netif_is_link_up(xnetif))
            {
                netif_set_link_down(xnetif);
            }
        }
        else
        {
             if (!netif_is_link_up(xnetif))
            {
                netif_set_link_up(xnetif);
            }
        }

        cy_rtos_delay_milliseconds( 1000 );
    }

}

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif Already initialized lwIP network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif)
{
    UNUSED_PARAMETER(netif);

    XMC_ETH_MAC_PORT_CTRL_t port_control;
    XMC_GPIO_CONFIG_t gpio_config;
    gpio_config.output_level = XMC_GPIO_OUTPUT_LEVEL_LOW;
    gpio_config.mode = XMC_GPIO_MODE_INPUT_TRISTATE;

    XMC_GPIO_Init(ETH_LWIP_0_CRS_DV, &gpio_config);

    gpio_config.mode = XMC_GPIO_MODE_INPUT_TRISTATE;

    XMC_GPIO_Init(ETH_LWIP_0_RXER, &gpio_config);

    gpio_config.mode = XMC_GPIO_MODE_INPUT_TRISTATE;

    XMC_GPIO_Init(ETH_LWIP_0_RXD0, &gpio_config);

    gpio_config.mode = XMC_GPIO_MODE_INPUT_TRISTATE;

    XMC_GPIO_Init(ETH_LWIP_0_RXD1, &gpio_config);

    gpio_config.output_strength = XMC_GPIO_OUTPUT_STRENGTH_STRONG_SHARP_EDGE;
    gpio_config.mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT1;

    XMC_GPIO_Init(ETH_LWIP_0_TXEN, &gpio_config);

    gpio_config.output_strength = XMC_GPIO_OUTPUT_STRENGTH_STRONG_SHARP_EDGE;
    gpio_config.mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT1;

    XMC_GPIO_Init(ETH_LWIP_0_TXD0, &gpio_config);

    gpio_config.output_strength = XMC_GPIO_OUTPUT_STRENGTH_STRONG_SHARP_EDGE;
    gpio_config.mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT1;

    XMC_GPIO_Init(ETH_LWIP_0_TXD1, &gpio_config);

    gpio_config.mode = XMC_GPIO_MODE_INPUT_TRISTATE;

    XMC_GPIO_Init(ETH_LWIP_0_RMII_CLK, &gpio_config);

    gpio_config.output_strength = XMC_GPIO_OUTPUT_STRENGTH_STRONG_SHARP_EDGE;
    gpio_config.mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT1;

    XMC_GPIO_Init(ETH_LWIP_0_MDC, &gpio_config);

    gpio_config.mode = XMC_GPIO_MODE_INPUT_TRISTATE;

    XMC_GPIO_Init(ETH_LWIP_0_MDIO, &gpio_config);


    XMC_GPIO_SetHardwareControl(ETH_LWIP_0_MDIO, XMC_GPIO_HWCTRL_PERIPHERAL1);


    port_control.mode = XMC_ETH_MAC_PORT_CTRL_MODE_RMII;
    port_control.rxd0 = (XMC_ETH_MAC_PORT_CTRL_RXD0_t)0U;
    port_control.rxd1 = (XMC_ETH_MAC_PORT_CTRL_RXD1_t)0U;
    port_control.clk_rmii = (XMC_ETH_MAC_PORT_CTRL_CLK_RMII_t)2U;
    port_control.crs_dv = (XMC_ETH_MAC_PORT_CTRL_CRS_DV_t)2U;
    port_control.rxer = (XMC_ETH_MAC_PORT_CTRL_RXER_t)0U;
    port_control.mdio = (XMC_ETH_MAC_PORT_CTRL_MDIO_t)1U;
    XMC_ETH_MAC_SetPortControl(&eth_mac, port_control);


    (void)XMC_ETH_MAC_Init(&eth_mac);

    XMC_ETH_MAC_DisableJumboFrame(&eth_mac);

    XMC_ETH_MAC_EnableReceptionBroadcastFrames(&eth_mac);
}
#endif

#ifdef COMPONENT_CAT1
#if LWIP_IPV4 && LWIP_IGMP
/*
 * This function is used to respond to IGMP (group management) requests.
 */
static err_t igmp_eth_filter(struct netif *iface, const ip4_addr_t *group, enum netif_mac_filter_action action)
{
    cy_stc_ethif_filter_config_t filter_config;
    uint8_t mac_addr[CY_MAC_ADDR_LEN] = MULTICAST_IP_TO_MAC((uint8_t*)group);

    filter_config.typeFilter = CY_ETHIF_FILTER_TYPE_DESTINATION;
    filter_config.filterAddr.byte[0] = mac_addr[0];
    filter_config.filterAddr.byte[1] = mac_addr[1];
    filter_config.filterAddr.byte[2] = mac_addr[2];
    filter_config.filterAddr.byte[3] = mac_addr[3];
    filter_config.filterAddr.byte[4] = mac_addr[4];
    filter_config.filterAddr.byte[5] = mac_addr[5];
    filter_config.ignoreBytes        = 0x00;

    cy_network_interface_context *if_ctx;
    if_ctx = (cy_network_interface_context *)iface->state;

    switch ( action )
    {
        case NETIF_ADD_MAC_FILTER:
#if (CY_IP_MXETH_INSTANCES > 1u)
            if(if_ctx->iface_idx == (uint8_t)CY_ECM_INTERFACE_ETH1)
            {
                if (Cy_ETHIF_SetFilterAddress(ETH1, CY_ETHIF_FILTER_NUM_1, &filter_config) != CY_ETHIF_SUCCESS)
                {
                    return ERR_VAL;
                }
            }
            else if(if_ctx->iface_idx == (uint8_t)CY_ECM_INTERFACE_ETH0)
            {
                if (Cy_ETHIF_SetFilterAddress(ETH0, CY_ETHIF_FILTER_NUM_1, &filter_config) != CY_ETHIF_SUCCESS)
                {
                    return ERR_VAL;
                }
            }
#else
            if(if_ctx->iface_idx == (uint8_t)CY_ECM_INTERFACE_ETH0)
            {
                if (Cy_ETHIF_SetFilterAddress(ETH0, CY_ETHIF_FILTER_NUM_1, &filter_config) != CY_ETHIF_SUCCESS)
                {
                    return ERR_VAL;
                }
            }
#endif
            break;

        case NETIF_DEL_MAC_FILTER:
            memset((void *)&(filter_config.filterAddr.byte[0]), 0, CY_MAC_ADDR_LEN);
#if (CY_IP_MXETH_INSTANCES > 1u)
            if(if_ctx->iface_idx == (uint8_t)CY_ECM_INTERFACE_ETH1)
            {
                if (Cy_ETHIF_SetFilterAddress(ETH1, CY_ETHIF_FILTER_NUM_1, &filter_config) != CY_ETHIF_SUCCESS)
                {
                    return ERR_VAL;
                }
            }
            else if(if_ctx->iface_idx == (uint8_t)CY_ECM_INTERFACE_ETH0)
            {
                if (Cy_ETHIF_SetFilterAddress(ETH0, CY_ETHIF_FILTER_NUM_1, &filter_config) != CY_ETHIF_SUCCESS)
                {
                    return ERR_VAL;
                }
            }

#else
            if(if_ctx->iface_idx == (uint8_t)CY_ECM_INTERFACE_ETH0)
            {
                if (Cy_ETHIF_SetFilterAddress(ETH0, CY_ETHIF_FILTER_NUM_1, &filter_config) != CY_ETHIF_SUCCESS)
                {
                    return ERR_VAL;
                }
            }
#endif
            break;

        default:
            return ERR_VAL;
    }

    return ERR_OK;
}
#endif

void custom_pbuf_free_cb(struct pbuf* p)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if(p == NULL)
    {
        return;
    }

    my_custom_pbuf_t* my_pbuf = (my_custom_pbuf_t*)p;
    result = cy_rtos_put_queue(&rx_input_buffer_queue, &my_pbuf->pbuffer, CY_RTOS_NEVER_TIMEOUT, false);
    if (CY_RSLT_SUCCESS != result)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Failed to push the free buffer to queue:[%d]\n", result);
    }

    LWIP_MEMPOOL_FREE(RX_POOL, my_pbuf);
}

static void rx_data_event_handler(void* arg)
{
    uint8_t *buffer = (uint8_t *)arg;
    cy_rx_buffer_info_t *rx_buffer_info;

    struct netif *netif = NULL;
    uint8_t i = 0;

    rx_buffer_info = (cy_rx_buffer_info_t *)buffer;
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Rx Buffer address from pool rx_buffer_info->rx_data_ptr:[%p] rx_buffer_info->length:[%d] rx_buffer_info->eth_idx:[%d]\n", rx_buffer_info->rx_data_ptr, rx_buffer_info->length, rx_buffer_info->eth_idx );

    for (i = 0; i < CY_IFACE_MAX_HANDLE; i++)
    {
        if( ( iface_context_database[i].is_initialized == true ) &&
            ( iface_context_database[i].iface_type == CY_NETWORK_ETH_INTERFACE ) &&
            ( iface_context_database[i].iface_idx == rx_buffer_info->eth_idx ) )
        {
            netif = (struct netif *)iface_context_database[i].nw_interface;
            break;
        }
    }

    if( i == CY_IFACE_MAX_HANDLE )
    {
        cy_rtos_put_queue(&rx_input_buffer_queue, &rx_buffer_info, CY_RTOS_NEVER_TIMEOUT, false);
        return;
    }

    my_custom_pbuf_t* my_pbuf  = (my_custom_pbuf_t*)LWIP_MEMPOOL_ALLOC(RX_POOL);
    if(my_pbuf == NULL)
    {
        cy_rtos_put_queue(&rx_input_buffer_queue, &rx_buffer_info, CY_RTOS_NEVER_TIMEOUT, false);
        return;
    }

    my_pbuf->pbuf_custom.custom_free_function = custom_pbuf_free_cb;
    my_pbuf->pbuffer = rx_buffer_info;

    struct pbuf* p = pbuf_alloced_custom(PBUF_RAW, rx_buffer_info->length,
                                         PBUF_REF,
                                         &my_pbuf->pbuf_custom,
                                         rx_buffer_info->rx_data_ptr,
                                         CY_ETH_SIZE_MAX_FRAME); /* Length size = payload + header size */

    if (p != NULL)
    {

        /* Call activity handler which is registered with the argument as false
        * indicating that there is RX packet.
        */
        if (activity_callback)
        {
            activity_callback(false);
        }

#ifdef ENABLE_CONNECTIVITY_MIDDLEWARE_LOGS
        size_t num_waiting;
        cy_rtos_count_queue(&rx_input_buffer_queue, &num_waiting);
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "the number of items currently in the queue:[%d]\n", num_waiting);
#endif
        /* Call netif->input */
        /* If the interface is not yet set up, drop the packet here. */
        if (netif->input == NULL || netif->input(p, netif) != ERR_OK)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "Drop packet before lwip \n");
            (void)pbuf_free(p);
        }
    }
    else
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "rx_data_event_handler: pbuf allocation failed\n");
        cy_rtos_put_queue(&rx_input_buffer_queue, &rx_buffer_info, CY_RTOS_NEVER_TIMEOUT, false);
    }
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
}

void cy_process_ethernet_data_cb( ETH_Type *eth_type, uint8_t *rx_buffer, uint32_t length )
{
    cy_rx_buffer_info_t *rx_params;
    ETH_Type *base_reg = eth_type;

    if (!((length >= CY_ETH_SIZE_MIN_FRAME) && (length <= CY_ETH_SIZE_MAX_FRAME)))
    {
        /* Ethernet packet received with invalid Rx length */
        cy_rtos_put_queue(&rx_input_buffer_queue, rx_buffer, CY_RTOS_NEVER_TIMEOUT, true);
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "Invalid Ethernet frame size. Push the rx_buffer back to the rx pool\n");
        return;
    }

    rx_params = (cy_rx_buffer_info_t *)(rx_buffer - sizeof(cy_rx_buffer_info_t));
    rx_params->rx_data_ptr = rx_buffer;
#if (CY_IP_MXETH_INSTANCES > 1u)
    if(base_reg == ETH1)
    {
        rx_params->eth_idx = 1;
    }
    else if(base_reg == ETH0)
    {
        rx_params->eth_idx = 0;
    }
#else
    if(base_reg == ETH0)
    {
        rx_params->eth_idx = 0;
    }
#endif
    rx_params->length = length;

    if((cy_worker_thread_enqueue(&cy_rx_worker_thread, rx_data_event_handler, (void *)rx_params)) != CY_RSLT_SUCCESS)
    {
        /* ERROR : Failed to send Rx callback notification. */
        /* TODO: The rx_buffer to be freed? */
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_worker_thread_enqueue Failed\n");
        return;
    }
}

void cy_notify_ethernet_rx_data_cb(ETH_Type *base, uint8_t **u8RxBuffer, uint32_t *u32Length)
{
    uint8_t *pbuffer = NULL;
    cy_rx_buffer_info_t *params;

    if(cy_rtos_get_queue(&rx_input_buffer_queue, (void *)&pbuffer, CY_RTOS_NEVER_TIMEOUT, true) != CY_RSLT_SUCCESS)
    {
        /* Failed to get a free buffer from the queue. */
        *u8RxBuffer = NULL;
        *u32Length = 0;
        return;
    }

    params = (cy_rx_buffer_info_t *)pbuffer;
    params->rx_data_ptr = (uint8_t *)(pbuffer + sizeof(cy_rx_buffer_info_t));
    *u32Length = CY_ETH_SIZE_MAX_FRAME;
    *u8RxBuffer = params->rx_data_ptr;
    return;
}

static void tx_event_handler(void* arg)
{
    cy_rslt_t result;

    result = cy_rtos_get_mutex( &tx_mutex , CY_RTOS_NEVER_TIMEOUT);
    if(result != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_rtos_get_mutex failed\n");
        return;
    }
    if(tx_buf_available == 0)
    {
        tx_buf_available++;
        cy_rtos_set_semaphore( &tx_semaphore, 1 );
    }
    else
    {
        tx_buf_available++;
    }
    result = cy_rtos_set_mutex( &tx_mutex );
    if(result != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "cy_rtos_set_mutex failed\n");
        return;
    }
}
void cy_tx_complete_cb ( ETH_Type *pstcEth, uint8_t u8QueueIndex )
{
    (void)pstcEth;
    (void)u8QueueIndex;
    
    /* Push the tx event into the worker queue */
    if((cy_worker_thread_enqueue(&cy_tx_worker_thread, tx_event_handler, NULL)) != CY_RSLT_SUCCESS)
    {
        return;
    }
}

void cy_tx_failure_cb ( ETH_Type *pstcEth, uint8_t u8QueueIndex )
{
    (void)pstcEth;
    (void)u8QueueIndex;
    
    /* Push the tx event into the worker queue */
    if((cy_worker_thread_enqueue(&cy_tx_worker_thread, tx_event_handler, NULL)) != CY_RSLT_SUCCESS)
    {
        return;
    }
}

#endif
/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif The lwIP network interface structure for this ethernetif
 * @param p The MAC packet to send (e.g., IP packet including the MAC addresses and type)
 * @return ERR_OK if the packet could be sent;
 *         an err_t value if the packet couldn't be sent.
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full, which can lead to
 *       unexpected results. Consider waiting for space in the DMA queue
 *       to become available because the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */
static err_t ethif_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

#ifdef COMPONENT_CAT3
    XMC_UNUSED_ARG(netif);
    uint8_t *buf;
    uint32_t framelen = 0U;

    if (p->tot_len > (u16_t)XMC_ETH_MAC_BUF_SIZE)
    {
        return ERR_BUF;
    }

    if (XMC_ETH_MAC_IsTxDescriptorOwnedByDma(&eth_mac))
    {
        XMC_ETH_MAC_ResumeTx(&eth_mac);
        return ERR_BUF;
    }
    else
    {
        buf = XMC_ETH_MAC_GetTxBuffer(&eth_mac);

#if ETH_PAD_SIZE
        pbuf_header(p, -ETH_PAD_SIZE);    /* Drop the padding word. */
#endif

        for(q = p; q != NULL; q = q->next)
        {
            /* Send the data from the pbuf to the interface, one pbuf at a
            time. The size of the data in each pbuf is kept in the ->len
            variable. */
            MEMCPY(buf, q->payload, q->len);
            framelen += (uint32_t)q->len;
            buf += q->len;
        }

#if ETH_PAD_SIZE
        pbuf_header(p, ETH_PAD_SIZE);    /* Reclaim the padding word. */
#endif


        XMC_ETH_MAC_SetTxBufferSize(&eth_mac, framelen);

        XMC_ETH_MAC_ReturnTxDescriptor(&eth_mac);
        XMC_ETH_MAC_ResumeTx(&eth_mac);

        return ERR_OK;
    }
#endif

#ifdef COMPONENT_CAT1
    cy_network_interface_context *if_ctx;
    cy_en_ethif_status_t eth_status;
    uint32_t framelen=0;
    cy_rslt_t result;
    
    if_ctx = (cy_network_interface_context *)netif->state;

    if (p->tot_len > (u16_t)CY_ETH_SIZE_MAX_FRAME) 
    {
      return ERR_BUF;
    }

    /* Call activity handler which is registered with the argument as true
     * indicating there is TX packet.
     */
    if (activity_callback)
    {
        activity_callback(true);
    }

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE);    /* Drop the padding word. */
#endif

    if(tx_buf_available == 0)
    {
        result = cy_rtos_get_semaphore( &tx_semaphore, TX_SEMAPHORE_TIMEOUT_MS, false );
        if(result != CY_RSLT_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_rtos_get_semaphore failed\n");
            return ERR_TIMEOUT;
        }
    }
    for(q = p; q != NULL; q = q->next)
    {
        MEMCPY((data_buffer[tx_free_buf_index] + framelen), q->payload, q->len);
        framelen += (uint32_t)q->len;
    }

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE);    /* Reclaim the padding word. */
#endif

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): Before Start transmitting frame p->tot_len:[%d] p->len:[%d] \n", __FUNCTION__, p->tot_len, p->len );
#if (CY_IP_MXETH_INSTANCES > 1u)
    if(if_ctx->iface_idx == (uint8_t)CY_ECM_INTERFACE_ETH1)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): Start transmitting frame ETH1 \n", __FUNCTION__ );
        eth_status = Cy_ETHIF_TransmitFrame(ETH1, (uint8_t*)data_buffer[tx_free_buf_index], framelen, CY_ETH_QS0_0, true);

        if(eth_status != CY_ETHIF_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to send outgoing packet :[%d]\n", eth_status);
        }
    }
    else
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): Start transmitting frame ETH0 \n", __FUNCTION__ );
        eth_status = Cy_ETHIF_TransmitFrame(ETH0, (uint8_t*)data_buffer[tx_free_buf_index], framelen, CY_ETH_QS0_0, true);
        if(eth_status != CY_ETHIF_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to send outgoing packet :[%d]\n", eth_status);
        }
    }
#else
    if(if_ctx->iface_idx == (uint8_t)CY_ECM_INTERFACE_ETH0)
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): Start transmitting frame ETH0 \n", __FUNCTION__ );
        eth_status = Cy_ETHIF_TransmitFrame(ETH0, (uint8_t*)data_buffer[tx_free_buf_index], framelen, CY_ETH_QS0_0, true);
        if(eth_status != CY_ETHIF_SUCCESS)
        {
            cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "failed to send outgoing packet :[%d]\n", eth_status);
        }
    }

#endif
    result = cy_rtos_get_mutex( &tx_mutex , CY_RTOS_NEVER_TIMEOUT);
    if(result != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_rtos_get_mutex failed\n");
        return ERR_IF;
    }
    /* Next data_buffer index to be used
     * Note: Tx completion callback is synchronous.
     */
    tx_free_buf_index = (tx_free_buf_index + 1) % TX_DATA_BUF_MAX;
    tx_buf_available--;
    result = cy_rtos_set_mutex( &tx_mutex );
    if(result != CY_RSLT_SUCCESS)
    {
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_ERR, "cy_rtos_set_mutex failed\n");
    }
#endif

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return ERR_OK ;
}

#ifdef COMPONENT_CAT3
/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif The lwIP network interface structure for this ethernetif
 * @return A pbuf filled with the received packet (including MAC header);
 *         NULL on memory error
 */
static struct pbuf * low_level_input(void)
{

    struct pbuf *p = NULL;
    struct pbuf *q;
    uint32_t len;
    uint8_t *buffer;

    if (XMC_ETH_MAC_IsRxDescriptorOwnedByDma(&eth_mac) == false)
    {
        len = XMC_ETH_MAC_GetRxFrameSize(&eth_mac);

        if ((len > 0U) && (len <= (uint32_t)XMC_ETH_MAC_BUF_SIZE))
        {
#if ETH_PAD_SIZE
            len += ETH_PAD_SIZE;    /* Allow room for Ethernet padding. */
#endif

            /* Allocate a pbuf chain of pbufs from the pool. */
            p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);

            if (p != NULL)
            {
#if ETH_PAD_SIZE
                pbuf_header(p, -ETH_PAD_SIZE);  /* Drop the padding word. */
#endif

                buffer = XMC_ETH_MAC_GetRxBuffer(&eth_mac);

                len = 0U;
                /* Iterate over the pbuf chain until the entire
                 * packet is read into the pbuf. */
                for (q = p; q != NULL; q = q->next)
                {
                /* Read enough bytes to fill this pbuf in the chain. The
                 * available data in the pbuf is given by the q->len
                 * variable.
                 * This does not necessarily have to be a memcpy; you can also preallocate
                 * pbufs for a DMA-enabled MAC and after receiving truncate it to the
                 * actually received size. In this case, ensure the tot_len member of the
                 * pbuf is the sum of the chained pbuf len members.
                 */
                     MEMCPY(q->payload, &buffer[len], q->len);
                     len += q->len;
                }
#if ETH_PAD_SIZE
                pbuf_header(p, ETH_PAD_SIZE);    /* Reclaim the padding word. */
#endif

                XMC_ETH_MAC_ReturnRxDescriptor(&eth_mac);
            }
        }
        else
        {
            XMC_ETH_MAC_ReturnRxDescriptor(&eth_mac);
        }
    }

    XMC_ETH_MAC_ResumeRx(&eth_mac);

  return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif The lwIP network interface structure for this ethernetif.
 */
static void ethernetif_input(void *arg)
{
    struct pbuf *p = NULL;
    struct eth_hdr *ethhdr;
    struct netif *netif = (struct netif *)arg;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s():%d netif:[%p]\n", __func__, __LINE__, netif);

    while(1)
    {
        cy_rtos_get_semaphore( &rx_semaphore, CY_RTOS_NEVER_TIMEOUT, false );

        /* Receive data from RX descriptor, post data to queue, enable IRQ. */
        NVIC_DisableIRQ((IRQn_Type)108);

        p = low_level_input();

        while (p != NULL)
        {
            ethhdr = p->payload;
            switch (htons(ethhdr->type))
            {
                case ETHTYPE_IP:
                case ETHTYPE_ARP:
                    /* Full packet send to tcpip_thread to process. */
                    if (netif->input( p, netif) != ERR_OK)
                    {
                        pbuf_free(p);
                    }

                    break;

                default:
                    pbuf_free(p);
                    break;
            }

            p = low_level_input();
        }

        NVIC_ClearPendingIRQ((IRQn_Type)108);
        NVIC_EnableIRQ((IRQn_Type)108);

    }

}
#endif

/**
 * Should be called at the beginning of the program to set up the
 * network interface.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif The lwIP network interface structure for this ethernetif.
 * @return ERR_OK if the loopif is initialized;
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error.
 */
err_t ethernetif_init(struct netif* netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    cy_network_interface_context *if_ctx = (cy_network_interface_context *)netif->state;

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): START \n", __FUNCTION__ );

#ifdef COMPONENT_CAT1
    /* Initialize the RX POOL */
    LWIP_MEMPOOL_INIT(RX_POOL);
    /* Initialise the buffer index */
    tx_free_buf_index = 0;
    /* Set the maximum buffers available initially */
    tx_buf_available = TX_DATA_BUF_MAX;
#endif

    /* Set the MAC hardware address to the interface.*/
    for(int i=0; i<ETH_HWADDR_LEN; i++)
    {
        netif->hwaddr[i] =  (u8_t)if_ctx->mac_address[i];
        cm_cy_log_msg(CYLF_MIDDLEWARE, CY_LOG_INFO, "netif->hwaddr[%d]:[0x%x] \n", i, netif->hwaddr[i]);
    }

    netif->hwaddr_len = ETH_HWADDR_LEN;

    /* Maximum transfer unit */
    netif->mtu = CY_ETH_SIZE_MAX_FRAME;

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;

#if LWIP_IPV4
    netif->output = etharp_output;
#endif
    netif->linkoutput = ethif_output;

    /*
     * Set up the information associated with sending the packets.
     */
    netif->flags = NETIF_FLAG_ETHARP;

#if ETH_BROADCAST_EN == 1
    netif->flags |= NETIF_FLAG_BROADCAST;
#endif

#ifdef LWIP_IPV6_MLD
    netif->flags |= NETIF_FLAG_MLD6;
#endif

#ifdef COMPONENT_CAT1
#if LWIP_IPV4 && LWIP_IGMP
    netif->flags |= NETIF_FLAG_IGMP;
    netif_set_igmp_mac_filter(netif, igmp_eth_filter) ;
#endif
#endif

#if LWIP_IPV6 == 1
    /*
     * Filter output packets for IPV6 through the Ethernet output
     * function for IPv6.
     */
    netif->output_ip6 = ethip6_output ;

    /*
     * Automatically generate a unicast IP address based on
     * neighbor discovery.
     */
    netif->ip6_autoconfig_enabled = 1 ;

    /*
     * Create a link-local IPv6 address.
     */
    netif_create_ip6_linklocal_address(netif, 1);

#if LWIP_IPV6_MLD
    /*
     * Register MLD MAC filter callback function that will be called by lwIP to add or delete an
     * entry in the IPv6 multicast filter table of the Ethernet MAC.
     */
    netif_set_mld_mac_filter(netif, NULL);
#endif
#endif

#ifdef COMPONENT_CAT3
    cy_rslt_t result = CY_RSLT_SUCCESS;
    ETH_LWIP_t *eth_lwip = if_ctx->hw_interface;

    /* Copy the netif address, which will be used for other netif operations */
    xnetif = netif;
    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG,"assigned netif address: [%p]\n", xnetif);

    /* initialize the hardware */
    low_level_init(netif);

    result = cy_rtos_init_semaphore( &rx_semaphore, 1, 0);
    if( result != CY_RSLT_SUCCESS )
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\n cy_rtos_init_semaphore failed with Error : [0x%X]\n", (unsigned int)result );
        return ERR_IF;
    }

    /* Create the thread to handle receive data events */
    result = cy_rtos_create_thread( &rx_event_thread, ethernetif_input, "eth_rx_input_thread", NULL,
                                    RX_EVENT_THREAD_STACK_SIZE, RX_EVENT_THREAD_PRIORITY, netif );
    if( result != CY_RSLT_SUCCESS )
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_create_thread failed with Error : [0x%X]\n", (unsigned int)result );
        cy_rtos_deinit_semaphore( &rx_semaphore);
        return ERR_IF;
    }

#if LWIP_NETIF_LINK_CALLBACK == 1
    netif_set_link_callback(netif, ethernetif_link_callback);
#endif

    /*Create a thread to keep track of PHY link status */
    result = cy_rtos_create_thread( &link_status_thread, ethernetif_link_status, "phy_link_status_thread", NULL,
                                    RX_STATUS_THREAD_STACK_SIZE, RX_STATUS_THREAD_PRIORITY, netif );
    if( result != CY_RSLT_SUCCESS )
    {
        cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_ERR, "\ncy_rtos_create_thread failed with Error : [0x%X]\n", (unsigned int)result );
        /* Delete the rx_event_thread thread */
        cy_rtos_terminate_thread( &rx_event_thread );
        cy_rtos_join_thread( &rx_event_thread );

        /* De Initialize the semaphore */
        cy_rtos_deinit_semaphore( &rx_semaphore);
        return ERR_IF;
    }

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "initialized:%d\n", eth_lwip->initialized );

    eth_lwip->eth_mac = &eth_mac;
    eth_lwip->xnetif = xnetif;
    eth_lwip->initialized = true;
#endif

    cm_cy_log_msg( CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s(): END \n", __FUNCTION__ );
    return ERR_OK;
}

#ifdef COMPONENT_CAT3
__WEAK void ETH_LWIP_UserIRQ(void)
{
}

void IRQ_Hdlr_108(void)
{
    XMC_ETH_MAC_ClearEventStatus(&eth_mac, XMC_ETH_MAC_EVENT_RECEIVE);
    cy_rtos_set_semaphore( &rx_semaphore, 1 );

    ETH_LWIP_UserIRQ();
}

void ETH_LWIP_Poll(void)
{
    cy_rtos_set_semaphore( &rx_semaphore, 1 );
}

#endif
#endif

