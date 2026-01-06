# RTOS Abstraction Library
The RTOS Abstraction APIs allow middleware to be written to be RTOS aware, but not need to care about what the actual RTOS is.

### What's Included?
The this release of the RTOS Abstraction API includes support for the following:
* APIs for interacting with common RTOS Features including:
    * Threads
    * Mutexes
    * Semaphores
    * Timers
    * Queues
    * Events
* Implementations are provided for
    * FreeRTOS
    * RTX (CMSIS RTOS)
    * ThreadX

### What Changed?
#### v1.8.0
Pre-production release for CAT5 only. Only one application thread is supported. The Scheduler API functions are not supported.
#### v1.7.1
Explicitly include cmsis_compiler.h where required.
#### v1.7.0
* FreeRTOS: When DeepSleep is locked, the default idle loop will attempt to enter Sleep instead.
* FreeRTOS: Add pre-production support for warm-boot after wake from DeepSleep-RAM power state (currently only present on CAT1B devices).
#### v1.6.0
* Removed the explicit in_isr argument from all functions. CPU status registers are used to automatically determine whether a function is executing in an interrupt context.
* Renamed all functions in order to make grouping more clear. The original function names are deprecated.
#### v1.5.0
* Added additional parameter validation for ThreadX
#### v1.4.0
* Added new thread functions: cy_rtos_wait_thread_notification and cy_rtos_set_thread_notification
* Minor documentation updates for clarity
#### v1.3.1
* Fixed a couple bugs in the FreeRTOS port
#### v1.3.0
* Update to work with a wider range of MCUs
* Fixed a race condition in the worker thread
* Fixed possible issue with thread priorities
* Minor documentation updates
#### v1.2.2
* Minor update for documentation & branding
#### v1.2.1
* Fixed possible race condition in cy_rtos_join_thread for FreeRTOS
* Minor documentation updates
#### v1.2.0
* Added utility library for for creating background worker threads: cy_worker_thread.h
* Added support for getting a semaphore's count: cy_rtos_get_count_semaphore()
* Added support for non-recursive mutexes: cy_rtos_init_mutex2()
* Added default implementations for FreeRTOS vApplicationGetIdleTaskMemory, vApplicationGetTimerTaskMemory, and vApplicationSleep
* Added support for ThreadX
NOTE: The vApplicationSleep implementation for FreeRTOS depends on the psoc6hal 1.2.0 or later.
#### v1.1.0
* Fixed an issue with the FreeRTOS implementation where it would always allocate memory for the thread stack, even if a pre-allocated buffer was provided.
* Removed usage of assert() and replaced with CY_ASSERT()
#### v1.0.1
* Added a new function to get the ID of the currently running thread.
* A few minor updates to avoid possible warnings depending on compiler.
* Minor documentation updates
#### v1.0.0
* Initial release supporting FreeRTOS & RTX

### Supported Software and Tools
This version of the RTOS Abstraction API was validated for compatibility with the following Software and Tools:

| Software and Tools                        | Version |
| :---                                      | :----:  |
| ModusToolbox™ Software Environment        | 2.4.0   |
| GCC Compiler                              | 10.3.1  |
| IAR Compiler                              | 9.30.1  |
| ARM Compiler                              | 6.16    |

Minimum required ModusToolbox™ Software Environment: v2.0

### More information
Use the following links for more information, as needed:
* [API Reference Guide](https://infineon.github.io/abstraction-rtos/html/modules.html)
* [Cypress Semiconductor, an Infineon Technologies Company](http://www.cypress.com)
* [Infineon GitHub](https://github.com/infineon)
* [ModusToolbox™](https://www.cypress.com/products/modustoolbox-software-environment)

---
© Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation, 2019-2023.
