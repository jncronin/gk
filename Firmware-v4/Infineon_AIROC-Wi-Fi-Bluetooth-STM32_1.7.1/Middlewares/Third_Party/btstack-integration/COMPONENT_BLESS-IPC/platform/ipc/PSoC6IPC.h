/*
 * PSoC6IPC.h
 *
 *  Created on: 14-Mar-2021
 *      Author: mcprabhakara
 */
#ifndef BTSTACK_PORT_CYAL_PLATFORM_20829_PSOC6IPC_H_
#define BTSTACK_PORT_CYAL_PLATFORM_20829_PSOC6IPC_H_

/* Endpoint indexes in the pipe array */
#define CY_IPC_EP_CYPIPE_CM0_ADDR           (0UL)
#define CY_IPC_EP_CYPIPE_CM4_ADDR           (1UL)

#define CY_BLE_IPC_HOST_ADDR CY_IPC_EP_CYPIPE_CM4_ADDR
#define CY_BLE_IPC_CONTROLLER_ADDR CY_IPC_EP_CYPIPE_CM0_ADDR

#define CY_BLE_CYPIPE_MSG_COMPLETE_ID                                   (0u)
#define CY_BLE_CYPIPE_MSG_SEND_ID                                       (1u)
#define CY_BLE_CYPIPE_MSG_CMD_ID                                        (3u)



typedef struct _IPC_HOST_MSG
{
    unsigned char clientID;
    unsigned char pktType;
    uint16_t intrRelMask;
    uint16_t pktLen;
    uint32_t pktDataPointer;
    uint16_t hostMetaData;
    /* Below fields are not used for command packets */
    uint32_t hciHeader;
    uint32_t pktDataHeader;  /* Contains l2cap header */
}IPC_HOST_MSG;

typedef struct _IPC_CTRL_MSG
{
	unsigned char clientID;
	unsigned char pktType;
    uint16_t intrRelMask;
    uint32_t pktDataPtr;
    uint16_t pktDataLen;
}IPC_CTRL_MSG;

#define IPC_TX_BUFFER_SIZE 255
#define IPC_NUM_BUFFERS 10
typedef struct
{
    IPC_HOST_MSG host_msg;
    uint8_t ipc_tx_buffer[IPC_TX_BUFFER_SIZE];
}ipc_host_msg_buffer;

/** Number of configurable Radio PA Lobuff parameters */
#define CY_BLE_MXD_PA_NUM_LOBUFF_VALS                                    (4u)
/** Number of configurable Radio PA Target parameters */
#define CY_BLE_MXD_PA_NUM_TARGET_VALS                                    (7u)

/** Number of configurable Radio PA LDO parameters */
#define CY_BLE_MXD_PA_NUM_LDO_SETTINGS                                   (4u)

/** Configuration parameter for Radio PA calibration */
typedef struct
{
    /** LOBUFF value Table
     *  __________________________________
     *  |   Parameter      |  Table Index |
     *  |------------------|--------------|
     *  |   LOBUFF_5dBm    |     0        |
     *  |   LOBUFF_4dBm    |     1        |
     *  |   LOBUFF_0dBm    |     2        |
     *  |   LOBUFF_n6dBm   |     3        |
     *  |------------------|--------------|
     *
     *  Note: Non-zero entries to the below table will over-ride
     *  the Cypress recommended radio PA settings.
     */
    uint16_t lobuffTable[CY_BLE_MXD_PA_NUM_LOBUFF_VALS];

    /** Target Value Table
     *  __________________________________
     *  |   Parameter      |  Table Index |
     *  |------------------|--------------|
     *  |   TARGET_5dBm    |     0        |
     *  |   TARGET_4dBm    |     1        |
     *  |   TARGET_0dBm    |     2        |
     *  |   TARGET_n6dBm   |     3        |
     *  |   TARGET_n12dBm  |     4        |
     *  |   TARGET_n16dBm  |     5        |
     *  |   TARGET_n20dBm  |     6        |
     *  |------------------|--------------|
     *
     *  Note: Non-zero entries to the below table will over-ride
     *  the Cypress recommended radio PA settings.
     */

    uint16_t targetTable[CY_BLE_MXD_PA_NUM_TARGET_VALS];

    /** Limit beyond which gain begins to rail.
     *  Note: Non-zero entries to the below element will over-ride
     *  the Cypress recommended radio PA settings.
     */
    uint8_t lobuffLimit;

    /** Calibration Ceiling.
     *  Note: Non-zero entries to the below element will over-ride
     *  the Cypress recommended radio PA settings.
     */
    uint16_t calValLimit;

    /**
     *  __________________________________
     *  |   Parameter     |  Table Index |
     *  |-----------------|--------------|
     *  |   ACT LDO 0dBm  |     0        |
     *  |   ACT LDO 4dBm  |     1        |
     *  |   PA LDO 0dBm   |     2        |
     *  |   PA LDO 4dBm   |     3        |
     *  |-----------------|--------------|
     *
     *  Note: Non-zero entries to the below table will over-ride
     *  the Cypress recommended radio PA settings.
     */
    uint16_t ldoVoltageTable[CY_BLE_MXD_PA_NUM_LDO_SETTINGS];

} cy_stc_ble_mxd_pa_cal_param_t;


typedef struct
{
    /** Tx 5dbm mode enable */
    bool                           tx5dbmModeEn;

    /**  Configuration parameter for Radio PA calibration */
    cy_stc_ble_mxd_pa_cal_param_t *paCalConfig;

    /** The feature set mask used to control usage of
     *  specified feature in the BLE stack */
    uint16_t                       featureMask;

    /** Max Tx payload size */
    uint16_t                       dleMaxTxCapability;

    /** Max Rx payload size */
    uint16_t                       dleMaxRxCapability;

    /** Memory heap pointer size */
    uint16_t                       totalHeapSz;

    /** Memory heap pointer */
    uint8_t                       *memoryHeapPtr;

    /** Controller memory heap size (uses only in dual CPU mode) */
    uint16_t                       controllerTotalHeapSz;

    /** Controller memory heap pointer (uses only in dual CPU mode) */
    uint8_t                       *controllerMemoryHeapPtr;

    /** Configuration for the L2CAP buffer for data transmission */
    uint8_t                        l2capBufferPerConn;

    /** The maximum number of devices that can be added to the resolve list */
    uint8_t                        maxResolvableDevListSize;

    /** The maximum number of devices that can be added to the white list */
    uint8_t                        maxBondedDevListSize;

    /** The maximum number of bonded devices to be supported by this device */
    uint8_t                        maxWhiteListSize;

    /** Maximum number of BLE connections */
    uint8_t                        maxConnCount;

    /** Package type */
    uint16_t                       packageType;

    /** Package type */
} cy_stc_ble_stack_params_t;


/* Package type constants */
#define CY_BLE_PACKAGE_TYPE_MASK             (0x00FFu)
#define CY_BLE_PACKAGE_PIN_NUM_MASK          (0xFF00u)
#define CY_BLE_PACKAGE_PIN_NUM_OFFSET        (8u)


/** Minimum stack queue depth requirement per connection */
/** The application can choose to give higher queue depth for better throughput. */
#define CY_BLE_L2CAP_STACK_Q_DEPTH_PER_CONN       (0x06u)
#define CY_BLE_ADD_Q_DEPTH_PER_CONN         (0u)

#define CY_BLE_L2CAP_Q_DEPTH_PER_CONN           (CY_BLE_L2CAP_STACK_Q_DEPTH_PER_CONN + CY_BLE_ADD_Q_DEPTH_PER_CONN)

/** Default Tx capability. If DLE is not enabled, this is Tx capability*/
#define CY_BLE_LL_DEFAULT_TX_CAPABILITY                                  251//(0x1Bu)

/** Default Rx capability. If DLE is not enabled, this is Rx capability*/
#define CY_BLE_LL_DEFAULT_RX_CAPABILITY                                  251//(0x1Bu)

/* 
*  Feature Mask Values :
*  B0 : DLE_FEATURE_MASK
*  B1 : PRIVACY_1_2_FEATURE_MASK
*  B2 : SECURE_CONN_FEATURE_MASK
*  B3 : PHY_UPDATE_FEATURE_MASK
*  B4 : PERSISTENT_STORE_BONDLIST
*  B5 : PERSISTENT_STORE_RESOLVING_LIST
*  B6 : PERSISTENT_STORE_WHITELIST
*  B7 : PERSISTENT_RADIO_CALIBRATION_MASK
*  Enabling only B0,B1,B3
*/
#define CY_BLE_FEATURE_MASK 0x000Bu

#define CY_BLE_RESOLVABLE_DEV_LIST_SIZE 16
#define CY_BLE_BONDED_DEV_LIST_SIZE 16
#define CY_BLE_WHITE_LIST_SIZE 16

cy_stc_ble_stack_params_t stackParam =
{
    /** Host memory heap pointer  */
    .memoryHeapPtr = NULL,

    /** Controller memory heap pointer  */
    .controllerMemoryHeapPtr = NULL,

    /** Memory heap size for host side */
    .totalHeapSz = 0,

    /** Memory heap size for controller side */
    .controllerTotalHeapSz = 0,

    /** Configuration for the L2CAP buffer for data transmission */
    .l2capBufferPerConn = CY_BLE_L2CAP_Q_DEPTH_PER_CONN,

    /** Max Tx payload size. */
    .dleMaxTxCapability = CY_BLE_LL_DEFAULT_TX_CAPABILITY,

    /** Max Rx payload size. */
    .dleMaxRxCapability = CY_BLE_LL_DEFAULT_RX_CAPABILITY,

    /** The feature set mask used to control usage of specified feature in the BLE stack */
    .featureMask = CY_BLE_FEATURE_MASK,

    /** The maximum number of devices that can be added to the resolve list */
    .maxResolvableDevListSize = CY_BLE_RESOLVABLE_DEV_LIST_SIZE,

    /** The maximum number of devices that can be added to the white list */
    .maxBondedDevListSize = CY_BLE_BONDED_DEV_LIST_SIZE,

    /** The maximum number of bonded devices to be supported by this device */
    .maxWhiteListSize = CY_BLE_WHITE_LIST_SIZE,

    /** Maximum number of BLE connections */
    .maxConnCount = 0,

    /** Tx 5dbm mode enable */
    .tx5dbmModeEn = false,

    /** Package type */
    #if defined(CY_GPIO_PIN_COUNT)
    .packageType = CY_GPIO_PACKAGE_TYPE | (CY_GPIO_PIN_COUNT << CY_BLE_PACKAGE_PIN_NUM_OFFSET),
    #else
    .packageType = CY_GPIO_PACKAGE_TYPE,
    #endif /* defined(CY_GPIO_PIN_COUNT) */
};
#endif /* BTSTACK_PORT_CYAL_PLATFORM_20829_PSOC6IPC_H_ */

