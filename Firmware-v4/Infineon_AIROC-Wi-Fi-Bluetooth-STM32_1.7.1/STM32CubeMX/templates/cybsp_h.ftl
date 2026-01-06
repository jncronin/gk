[#ftl]
/**
  ******************************************************************************
  * File Name          : ${name}
  * Description        : This file provides code for the configuration
  *                      of the ${name} instances.
  ******************************************************************************
[@common.optinclude name=mxTmpFolder+"/license.tmp"/][#--include License text --]
  ******************************************************************************
  */
[#assign s = name]
[#assign toto = s?replace(".","__")]
[#assign dashReplace = toto?replace("-","_")]
[#assign inclusion_protection = dashReplace?upper_case]
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __${inclusion_protection}__
#define __${inclusion_protection}__

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
[#if includes??]
[#list includes as include]
#include "${include}"
[/#list]
[/#if]

/* Defines ------------------------------------------------------------------*/
#define false 0
#define true 1

/* WIFI interface types */
#define CYBSP_SDIO_INTERFACE    (0)
#define CYBSP_SPI_INTERFACE     (1)
#define CYBSP_M2M_INTERFACE     (2)
#define CYBSP_USB_INTERFACE     (3)

[#-- SWIPdatas is a list of SWIPconfigModel --]  
[#list SWIPdatas as SWIP]  
[#-- Global variables --]
[#if SWIP.variables??]
	[#list SWIP.variables as variable]
extern ${variable.value} ${variable.name};
	[/#list]
[/#if]

[#-- Global variables --]

[#assign instName = SWIP.ipName]   
[#assign fileName = SWIP.fileName]   
[#assign version = SWIP.version]   

/**
	MiddleWare name : ${instName}
	MiddleWare fileName : ${fileName}
	MiddleWare version : ${version}
*/
[#if SWIP.defines??]
	[#list SWIP.defines as definition]	
/*---------- [#if definition.comments??]${definition.comments} [/#if] -----------*/
#define ${definition.name} #t#t ${definition.value} 
[#if definition.description??]${definition.description} [/#if]
	[/#list]
[/#if]

[/#list]

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
#endif /*__ ${inclusion_protection}_H */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
