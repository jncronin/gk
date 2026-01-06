# Middleware FreeRTOS MCU Component

## Overview

**STM32Cube** is an STMicroelectronics original initiative to ease the developers life by reducing efforts, time and cost.

**STM32Cube** covers the overall STM32 products portfolio. It includes a comprehensive embedded software platform, delivered for each STM32 series.
   * The CMSIS modules (core and device) corresponding to the ARM(tm) core implemented in this STM32 product.
   * The STM32 HAL-LL drivers : an abstraction drivers layer, the API ensuring maximized portability across the STM32 portfolio.
   * The BSP Drivers of each evaluation or demonstration board provided by this STM32 series.
   * A consistent set of middlewares components such as RTOS, USB, FatFS, Graphics, STM32_TouchSensing_Library...
   * A full set of software projects (basic examples, applications or demonstrations) for each board provided by this STM32 series.

Two models of publication are proposed for the STM32Cube embedded software:
   * The monolithic **MCU Package** : all STM32Cube software modules of one STM32 series are present (Drivers, Middlewares, Projects, Utilities) in the repo (usual name **STM32Cubexx**, xx corresponding to the STM32 series).
   * The **MCU component** : progressively from November 2019, each STM32Cube software module being part of the STM32Cube MCU Package, will be delivered as an individual repo, allowing the user to select and get only the required software functions.

## Description

This **stm32_mw_freertos** MCU component repository is one element **common to all** STM32Cube MCU embedded software packages, providing the **FreeRTOS Middleware** part.

## License

Copyright (c) 2019 Amazon.com, Inc.

This software component is licensed by Amazon.com under the **MIT open source** license. You may not use this file except in compliance with this license. You may obtain a copy of the license [here](https://opensource.org/licenses/MIT).

## Release note

Details about the content of this release are available in the release note [here](./Source/History.txt).

Details about the updates made by STMicroelectronics are available in the release note [here](./Source/st_readme.txt).

## Compatibility information

This table shows the correspondence between the MW component version and other components corresponding versions. It is **crucial** that you use a consistent set of versions, as mentioned in this table.

In the case of the FreeRTOS, this correspondence is particular to the CMSIS-RTOS V2, the CMSIS-RTOS V1 being compatible with older CMSIS Core versions too.

FreeRTOS | CMSIS Core |
-------- | ---------- |
Tag v10.0.1 | Tag v5.4.0
Tag v10.2.1 | Tag v5.4.0
Tag v10.3.1 | Tag v5.6.0
Tag v10.4.6 | Tag v5.6.0

## Troubleshooting

If you have any issue with the **software content** of this repository, you can file an issue [here](https://github.com/STMicroelectronics/stm32_mw_freertos/issues/new/choose).

For any other question related to the product, the tools, the environment, you can submit a topic to the [ST Community](https://community.st.com/s/).
