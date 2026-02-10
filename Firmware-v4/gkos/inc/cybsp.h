#ifndef CYBSP_H
#define CYBSP_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

/* Defines ------------------------------------------------------------------*/
#define false 0
#define true 1

/* WIFI interface types */
#define CYBSP_SDIO_INTERFACE    (0)
#define CYBSP_SPI_INTERFACE     (1)
#define CYBSP_M2M_INTERFACE     (2)
#define CYBSP_USB_INTERFACE     (3)

/**
	MiddleWare name : Infineon.AIROC-Wi-Fi-Bluetooth-STM32.1.6.0
	MiddleWare fileName : cybsp.h
	MiddleWare version :
*/
/*----------   -----------*/
#define CYBSP_BT_UART_CTS      NC

/*----------   -----------*/
#define CY_WIFI_COUNTRY_CUSTOM      WHD_COUNTRY_XX

/*----------   -----------*/
#define CYBSP_BT_UART_TX      NC

/*----------   -----------*/
#define CYBSP_USER_BTN      PC13

/*----------   -----------*/
#define COUNTRY_CUSTOM_ENABLE      false

/*----------   -----------*/
#define CYBSP_BT_POWER      NC

/*----------   -----------*/
#define COMMAND_CONSOLE_UART_TX      NC

/*----------   -----------*/
#define COMMAND_CONSOLE_UART_RX      NC

/*----------   -----------*/
#define CYBSP_WIFI_HOST_WAKE      NC

/*----------   -----------*/
#define CYHAL_ISR_PRIORITY_DEFAULT      5

/*----------   -----------*/
#define SDMMC_CLK_FREQ_OVERRIDE      0

/*----------   -----------*/
#define CYBSP_WIFI_WL_REG_ON      PD0

/*----------   -----------*/
#define CYBSP_BT_DEVICE_WAKE      NC

/*----------   -----------*/
#define CYBSP_BT_HOST_WAKE      NC

/*----------   -----------*/
#define CYBSP_WIFI_HOST_WAKE_IRQ_EVENT      CYHAL_GPIO_IRQ_RISE

/*----------   -----------*/
#define CYHAL_UART_MAX_INSTANCES      3

/*----------   -----------*/
#define CYBSP_BT_UART_RX      NC

/*----------   -----------*/
#define CY_WIFI_COUNTRY      WHD_COUNTRY_UNITED_STATES

/*----------   -----------*/
#define CYBSP_BT_UART_RTS      NC

/*----------   -----------*/
#define _CYHAL_SDIO_DMA_BUFFER_SIZE      0

#if COUNTRY_CUSTOM_ENABLE == 1
#if defined(CY_WIFI_COUNTRY)
#undef CY_WIFI_COUNTRY
#define CY_WIFI_COUNTRY CY_WIFI_COUNTRY_CUSTOM
#endif
#endif

#if !defined(CYBSP_WIFI_INTERFACE_TYPE)
#define CYBSP_WIFI_INTERFACE_TYPE 	(CYBSP_SDIO_INTERFACE)
#endif

#ifdef __cplusplus
}
#endif


#endif
