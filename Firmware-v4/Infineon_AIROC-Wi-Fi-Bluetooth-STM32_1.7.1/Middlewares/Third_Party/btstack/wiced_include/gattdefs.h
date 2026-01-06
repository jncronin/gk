/*
 * Copyright 2019-2023, Cypress Semiconductor Corporation or
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

/** @file
 *
 *  WICED Generic Attribute (GATT) Application Programming Interface definitions
 */

#ifndef  _GATTDEFS_H
#define  _GATTDEFS_H

#define GATT_ILLEGAL_UUID            0

/* GATT attribute types
*/
#define GATT_UUID_PRI_SERVICE           0x2800
#define GATT_UUID_SEC_SERVICE           0x2801
#define GATT_UUID_INCLUDE_SERVICE       0x2802
#define GATT_UUID_CHAR_DECLARE          0x2803      /*  Characteristic Declaration*/

#define GATT_UUID_CHAR_EXT_PROP         0x2900      /*	Characteristic Extended Properties */
#define GATT_UUID_CHAR_DESCRIPTION      0x2901      /*  Characteristic User Description*/
#define GATT_UUID_CHAR_CLIENT_CONFIG    0x2902      /*  Client Characteristic Configuration */
#define GATT_UUID_CHAR_SRVR_CONFIG      0x2903      /*  Server Characteristic Configuration */
#define GATT_UUID_CHAR_PRESENT_FORMAT   0x2904      /*  Characteristic Presentation Format*/
#define GATT_UUID_CHAR_AGG_FORMAT       0x2905      /*  Characteristic Aggregate Format*/
#define GATT_UUID_CHAR_VALID_RANGE      0x2906      /*  Characteristic Valid Range */
#define GATT_UUID_EXT_RPT_REF_DESCR     0x2907
#define GATT_UUID_RPT_REF_DESCR         0x2908

#define GATT_UUID_CLIENT_SUPPORTED_FEATURES 0x2B29   /* Client supported features */
#define GATT_UUID_GATT_DATABASE_HASH	    0x2B2A   /* GATT Robust caching */
#define GATT_UUID_SERVER_SUPPORTED_FEATURES 0x2B3A   /* Server supported features */

/* GAP Profile Attributes
*/
#define GATT_UUID_GAP_DEVICE_NAME        UUID_CHARACTERISTIC_DEVICE_NAME
#define GATT_UUID_GAP_ICON               UUID_CHARACTERISTIC_APPEARANCE
#define GATT_UUID_GAP_PREF_CONN_PARAM    UUID_CHARACTERISTIC_PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS
#define GATT_UUID_GAP_CENTRAL_ADDR_RESOL UUID_CHARACTERISTIC_CENTRAL_ADDRESS_RESOLUTION
#define GATT_UUID_GAP_RPA_ONLY           UUID_CHARACTERISTIC_RPA_ONLY

/* Attribute Profile Attribute UUID */
#define GATT_UUID_GATT_SRV_CHGD          UUID_CHARACTERISTIC_SERVICE_CHANGED
/* Attribute Protocol Test */

/* Link Loss Service */
#define GATT_UUID_ALERT_LEVEL            UUID_CHARACTERISTIC_ALERT_LEVEL /* Alert Level */
#define GATT_UUID_TX_POWER_LEVEL         UUID_CHARACTERISTIC_TX_POWER_LEVEL /* TX power level */

/* Time Profile */
/* Current Time Service */
#define GATT_UUID_CURRENT_TIME           UUID_CHARACTERISTIC_CURRENT_TIME /* Current Time */
#define GATT_UUID_LOCAL_TIME_INFO        UUID_CHARACTERISTIC_LOCAL_TIME_INFORMATION /* Local time info */
#define GATT_UUID_REF_TIME_INFO          UUID_CHARACTERISTIC_REFERENCE_TIME_INFORMATION /* reference time information */

/* phone alert */
#define GATT_UUID_ALERT_STATUS           UUID_CHARACTERISTIC_ALERT_STATUS /* alert status */
#define GATT_UUID_RINGER_CP              UUID_CHARACTERISTIC_RINGER_CONTROL_POINT /* ringer control point */
#define GATT_UUID_RINGER_SETTING         UUID_CHARACTERISTIC_RINGER_SETTING      /* ringer setting */

/* Glucose Service */
#define GATT_UUID_GM_MEASUREMENT         UUID_CHARACTERISTIC_GLUCOSE_MEASUREMENT
#define GATT_UUID_GM_CONTEXT             UUID_CHARACTERISTIC_GLUCOSE_MEASUREMENT_CONTEXT
#define GATT_UUID_GM_CONTROL_POINT       UUID_CHARACTERISTIC_RECORD_ACCESS_CONTROL_POINT
#define GATT_UUID_GM_FEATURE             UUID_CHARACTERISTIC_GLUCOSE_FEATURES

/* device infor characteristic */
#define GATT_UUID_SYSTEM_ID              UUID_CHARACTERISTIC_SYSTEM_ID
#define GATT_UUID_MODEL_NUMBER_STR       UUID_CHARACTERISTIC_MODEL_NUMBER_STRING
#define GATT_UUID_SERIAL_NUMBER_STR      UUID_CHARACTERISTIC_SERIAL_NUMBER_STRING
#define GATT_UUID_FW_VERSION_STR         UUID_CHARACTERISTIC_FIRMWARE_REVISION_STRING
#define GATT_UUID_HW_VERSION_STR         UUID_CHARACTERISTIC_HARDWARE_REVISION_STRING
#define GATT_UUID_SW_VERSION_STR         UUID_CHARACTERISTIC_SOFTWARE_REVISION_STRING
#define GATT_UUID_MANU_NAME              UUID_CHARACTERISTIC_MANUFACTURER_NAME_STRING
#define GATT_UUID_IEEE_DATA              UUID_CHARACTERISTIC_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST
#define GATT_UUID_PNP_ID                 UUID_CHARACTERISTIC_PNP_ID

/* HID characteristics */
#define GATT_UUID_HID_INFORMATION        UUID_CHARACTERISTIC_HID_INFORMATION
#define GATT_UUID_HID_REPORT_MAP         UUID_CHARACTERISTIC_HID_REPORT_MAP
#define GATT_UUID_HID_CONTROL_POINT      UUID_CHARACTERISTIC_HID_CONTROL_POINT
#define GATT_UUID_HID_REPORT             UUID_CHARACTERISTIC_HID_REPORT
#define GATT_UUID_HID_PROTO_MODE         UUID_CHARACTERISTIC_HID_PROTOCOL_MODE
#define GATT_UUID_HID_BT_KB_INPUT        UUID_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT
#define GATT_UUID_HID_BT_KB_OUTPUT       UUID_CHARACTERISTIC_BOOT_KEYBOARD_OUTPUT_REPORT
#define GATT_UUID_HID_BT_MOUSE_INPUT     UUID_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT

/* Battery Service char */
#define GATT_UUID_BATTERY_LEVEL          UUID_CHARACTERISTIC_BATTERY_LEVEL

#define GATT_UUID_SC_CONTROL_POINT       UUID_CHARACTERISTIC_SC_CONTROL_POINT
#define GATT_UUID_SENSOR_LOCATION        UUID_CHARACTERISTIC_SENSOR_LOCATION

/* RUNNERS SPEED AND CADENCE SERVICE      */
#define GATT_UUID_RSC_MEASUREMENT        UUID_CHARACTERISTIC_RSC_MEASUREMENT
#define GATT_UUID_RSC_FEATURE            UUID_CHARACTERISTIC_RSC_FEATURE

/* CYCLING SPEED AND CADENCE SERVICE      */
#define GATT_UUID_CSC_MEASUREMENT        UUID_CHARACTERISTIC_CSC_MEASUREMENT
#define GATT_UUID_CSC_FEATURE            UUID_CHARACTERISTIC_CSC_FEATURE

/* CYCLING POWER SERVICE  (Temp for IOP)    */
#define GATT_UUID_CP_MEASUREMENT         UUID_CHARACTERISTIC_CYCLING_POWER_MEASUREMENT
#define GATT_UUID_CP_VECTOR              UUID_CHARACTERISTIC_CYCLING_POWER_VECTOR
#define GATT_UUID_CP_FEATURE             UUID_CHARACTERISTIC_CYCLING_POWER_FEATURE
#define GATT_UUID_CP_CONTROL_POINT       UUID_CHARACTERISTIC_CYCLING_POWER_CONTROL_POINT

/* LOCATION AND NAVIGATION SERVICE  (Temp for IOP)    */
#define GATT_UUID_LN_LOC_AND_SPEED       UUID_CHARACTERISTIC_LOCATION_AND_SPEED
#define GATT_UUID_LN_NAVIGATION          UUID_CHARACTERISTIC_NAVIGATION
#define GATT_UUID_LN_POSITION_QUALITY    UUID_CHARACTERISTIC_POSITION_QUALITY
#define GATT_UUID_LN_FEATURE             UUID_CHARACTERISTIC_LN_FEATURE
#define GATT_UUID_LN_CONTROL_POINT       UUID_CHARACTERISTIC_LN_CONTROL_POINT


/* HTTP Proxy Service */
#define GATT_UUID_HPS_URI                0x7F11
#define GATT_UUID_HPS_HEADERS            0x7F12
#define GATT_UUID_HPS_STATUS_CODE        0x7F13
#define GATT_UUID_HPS_ENTITY_BODY        0x7F14
#define GATT_UUID_HPS_CONTROL_POINT      0x7F15
#define GATT_UUID_HPS_SERCURITY          0x7F16


/* Scan Parameter charatceristics */
#define GATT_UUID_SCAN_INT_WINDOW        UUID_CHARACTERISTIC_SCAN_INTERVAL_WINDOW
#define GATT_UUID_SCAN_REFRESH           UUID_CHARACTERISTIC_SCAN_REFRESH


/**
 * GATT appearance definitions
 *
 * @ingroup wicedbt_gatt
 *
 */
enum gatt_appearance_e
{
    APPEARANCE_GENERIC_PHONE                    = 64,
    APPEARANCE_GENERIC_COMPUTER                 = 128,
    APPEARANCE_GENERIC_WATCH                    = 192,
    APPEARANCE_WATCH_SPORTS                     = 193,
    APPEARANCE_GENERIC_CLOCK                    = 256,
    APPEARANCE_GENERIC_DISPLAY                  = 320,
    APPEARANCE_GENERIC_REMOTE_CONTROL           = 384,
    APPEARANCE_GENERIC_EYE_GLASSES              = 448,
    APPEARANCE_GENERIC_TAG                      = 512,
    APPEARANCE_GENERIC_KEYRING                  = 576,
    APPEARANCE_GENERIC_MEDIA_PLAYER             = 640,
    APPEARANCE_GENERIC_BARCODE_SCANNER          = 704,
    APPEARANCE_GENERIC_THERMOMETER              = 768,
    APPEARANCE_THERMOMETER_EAR                  = 769,
    APPEARANCE_GENERIC_HEART_RATE_SENSOR        = 832,
    APPEARANCE_HEART_RATE_BELT                  = 833,
    APPEARANCE_GENERIC_BLOOD_PRESSURE           = 896,
    APPEARANCE_BLOOD_PRESSURE_ARM               = 897,
    APPEARANCE_BLOOD_PRESSURE_WRIST             = 898,
    APPEARANCE_GENERIC_HID_DEVICE               = 960,
    APPEARANCE_HID_KEYBOARD                     = 961,
    APPEARANCE_HID_MOUSE                        = 962,
    APPEARANCE_HID_JOYSTICK                     = 963,
    APPEARANCE_HID_GAMEPAD                      = 964,
    APPEARANCE_HID_DIGITIZER_TABLET             = 965,
    APPEARANCE_HID_CARD_READER                  = 966,
    APPEARANCE_HID_DIGITAL_PEN                  = 967,
    APPEARANCE_HID_BARCODE_SCANNER              = 968,
    APPEARANCE_GENERIC_GLUCOSE_METER            = 1024,
    APPEARANCE_GENERIC_RUNNING_WALKING_SENSOR   = 1088,
    APPEARANCE_RUNNING_WALKING_SENSOR_IN_SHOE   = 1089,
    APPEARANCE_RUNNING_WALKING_SENSOR_ON_SHOE   = 1090,
    APPEARANCE_RUNNING_WALKING_SENSOR_ON_HIP    = 1091,
    APPEARANCE_GENERIC_CYCLING                  = 1152,
    APPEARANCE_CYCLING_COMPUTER                 = 1153,
    APPEARANCE_CYCLING_SPEED_SENSOR             = 1154,
    APPEARANCE_CYCLING_CADENCE_SENSOR           = 1155,
    APPEARANCE_CYCLING_POWER_SENSOR             = 1156,
    APPEARANCE_CYCLING_SPEED_AND_CADENCE_SENSOR = 1157,
    APPEARANCE_CONTROL_DEVICE_SWITCH            = 1217, /// Switch Control Device subtype
    APPEARANCE_CONTROL_DEVICE_MULTI_SWITCH      = 1218, /// Multi - switch
    APPEARANCE_CONTROL_DEVICE_BUTTON            = 1219, /// Button
    APPEARANCE_CONTROL_DEVICE_SLIDER            = 1220, /// Slider
    APPEARANCE_CONTROL_DEVICE_ROTARY            = 1221, /// Rotary
    APPEARANCE_CONTROL_DEVICE_TOUCH_PANEL       = 1222, /// Touch - panel
    APPEARANCE_NETWORK_DEVICE                   = 1280, /// Generic Network Device Generic category
    APPEARANCE_ACCESS_POINT                     = 1281, /// Access Point Generic Network subtype
    APPEARANCE_SENSOR_GENERIC                   = 1344, /// Generic Sensor Generic category
    APPEARANCE_SENSOR_MOTION                    = 1345, /// Motion Sensor
    APPEARANCE_SENSOR_AIR_QUALITY               = 1346, /// Air Quality Sensor
    APPEARANCE_SENSOR_TEMPERATURE               = 1347, /// Temperature Sensor
    APPEARANCE_SENSOR_HUMIDITY                  = 1348, /// Humidity Sensor
    APPEARANCE_SENSOR_LEAK                      = 1349, /// Leak Sensor
    APPEARANCE_SENSOR_SMOKE                     = 1350, /// Smoke Sensor
    APPEARANCE_SENSOR_OCCUPANCY                 = 1351, /// Occupancy Sensor
    APPEARANCE_SENSOR_CONTACT                   = 1352, /// Contact Sensor
    APPEARANCE_SENSOR_CARBON_MONOXIDE           = 1353, /// Carbon Monoxide Sensor
    APPEARANCE_SENSOR_CARBON_DIOXIDE            = 1354, /// Carbon Dioxide Sensor
    APPEARANCE_SENSOR_AMBIENT_LIGHT             = 1355, /// Ambient Light Sensor
    APPEARANCE_SENSOR_ENERGY                    = 1356, /// Energy Sensor
    APPEARANCE_SENSOR_COLOR_LIGHT               = 1357, /// Color Light Sensor
    APPEARANCE_SENSOR_RAIN                      = 1358, /// Rain Sensor
    APPEARANCE_SENSOR_FIRE                      = 1359, /// Fire SensorF
    APPEARANCE_SENSOR_WIND                      = 1360, /// Wind Sensor
    APPEARANCE_SENSOR_PROXYMITY                 = 1361, /// Proximity Sensor
    APPEARANCE_SENSOR_MULTI_SENSOR              = 1362, /// Multi - Sensor
    APPEARANCE_LIGHT_GENERIC_FIXTURE            = 1408, /// Generic Light Fixtures	Generic category
    APPEARANCE_LIGHT_WALL                       = 1409, /// Wall Light
    APPEARANCE_LIGHT_CEILING                    = 1410, /// Ceiling Light
    APPEARANCE_LIGHT_FLOOR                      = 1411, /// Floor Light
    APPEARANCE_LIGHT_CABINET                    = 1412, /// Cabinet Light
    APPEARANCE_LIGHT_DESK                       = 1413, /// Desk Light
    APPEARANCE_LIGHT_TROFFER                    = 1414, /// Troffer Light
    APPEARANCE_LIGHT_PENDANT                    = 1415, /// Pendant Light
    APPEARANCE_LIGHT_IN_GROUND                  = 1416, /// In - ground Light
    APPEARANCE_LIGHT_FLOOD                      = 1417, /// Flood Light
    APPEARANCE_LIGHT_UNDERWATER                 = 1418, /// Underwater Light
    APPEARANCE_LIGHT_BOLLAR                     = 1419, /// Bollard with Light
    APPEARANCE_LIGHT_PATHWAY                    = 1420, /// Pathway Light
    APPEARANCE_LIGHT_GARDEN                     = 1421, /// Garden Light
    APPEARANCE_LIGHT_POLE                       = 1422, /// Pole - top Light
    APPEARANCE_LIGHT_SPOTLIGHT                  = 1423, /// Spotlight
    APPEARANCE_LIGHT_LINEAR                     = 1424, /// Linear Light
    APPEARANCE_LIGHT_STREET                     = 1425, /// Street Light
    APPEARANCE_LIGHT_SHELVE                     = 1426, /// Shelves Light
    APPEARANCE_LIGHT_HIGH_BAY_LOW_BAY           = 1427, /// High - bay / Low - bay Light
    APPEARANCE_LIGHT_EMERGENCY_EXIT             = 1428, /// Emergency Exit Light
    APPEARANCE_LOCATION_AND_NAVIGATION_POD      = 5188,
};

#define IS_CHAR_DECLARATION_UUID(type)    ((type.len == 2) && (type.uu.uuid16 == GATT_UUID_CHAR_DECLARE))
#define IS_CHAR_CLIENT_CONFIG_UUID(type)  ((type.len == 2) && (type.uu.uuid16 == GATT_UUID_CHAR_CLIENT_CONFIG))

#endif
