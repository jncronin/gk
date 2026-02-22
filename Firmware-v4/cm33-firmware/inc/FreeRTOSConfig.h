#ifndef FREERTOSCONFIG_H
#define FREERTOSCONFIG_H

#define configUSE_PREEMPTION                                        1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION                     0
#define configUSE_TICKLESS_IDLE                                     0
#define configCPU_CLOCK_HZ                                          400000000
#define configSYSTICK_CLOCK_HZ                                      50000000
#define configTICK_RATE_HZ                                          250
#define configMAX_PRIORITIES                                        5
#define configMINIMAL_STACK_SIZE                                    1024
#define configMAX_TASK_NAME_LEN                                     32
#define configUSE_16_BIT_TICKS                                      0
#define configIDLE_SHOULD_YIELD                                     1
#define configUSE_TASK_NOTIFICATIONS                                1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES                       3
#define configUSE_MUTEXES                                           1
#define configUSE_RECURSIVE_MUTEXES                                 1
#define configUSE_COUNTING_SEMAPHORES                               1
#define configUSE_ALTERNATIVE_API                                   0 /* Deprecated! */
#define configQUEUE_REGISTRY_SIZE                                   10
#define configUSE_QUEUE_SETS                                        0
#define configUSE_TIME_SLICING                                      0
#define configUSE_NEWLIB_REENTRANT                                  0
#define configENABLE_BACKWARD_COMPATIBILITY                         0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS                     5
#define configUSE_MINI_LIST_ITEM                                    1
#define configSTACK_DEPTH_TYPE                                      uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE                            size_t
#define configHEAP_CLEAR_MEMORY_ON_FREE                             1

/* Memory allocation related definitions. */
#define configSUPPORT_STATIC_ALLOCATION                             0
#define configSUPPORT_DYNAMIC_ALLOCATION                            1
#define configTOTAL_HEAP_SIZE                                       32768
#define configAPPLICATION_ALLOCATED_HEAP                            0
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP                   0

/* Hook function related definitions. */
#define configUSE_IDLE_HOOK                                 1
#define configUSE_TICK_HOOK                                 0
#define configCHECK_FOR_STACK_OVERFLOW                      0
#define configUSE_MALLOC_FAILED_HOOK                        0
#define configUSE_DAEMON_TASK_STARTUP_HOOK                  0
#define configUSE_SB_COMPLETED_CALLBACK                     0

/* Run time and task stats gathering related definitions. */
#define configGENERATE_RUN_TIME_STATS                       0
#define configUSE_TRACE_FACILITY                            0
#define configUSE_STATS_FORMATTING_FUNCTIONS                0

/* Co-routine related definitions. */
#define configUSE_CO_ROUTINES                               0
#define configMAX_CO_ROUTINE_PRIORITIES                     1

/* Software timer related definitions. */
#define configUSE_TIMERS                                    0
#define configTIMER_TASK_PRIORITY                           3
#define configTIMER_QUEUE_LENGTH                            10
#define configTIMER_TASK_STACK_DEPTH                        configMINIMAL_STACK_SIZE

/* Interrupt nesting behaviour configuration. */
#define configKERNEL_INTERRUPT_PRIORITY         0xf0
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    0x80
#define configMAX_API_CALL_INTERRUPT_PRIORITY   0x80

/* Define to trap errors during development. */
void vAssertCalled(const char *, int);
#define configASSERT(x) if( ( x ) == 0 ) {}// vAssertCalled( __FILE__, __LINE__ )

/* FreeRTOS MPU specific definitions. */
#define configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS 0
#define configTOTAL_MPU_REGIONS                                8 /* Default value */
#define configTEX_S_C_B_FLASH                                  0x07UL /* Default value */
#define configTEX_S_C_B_SRAM                                   0x07UL /* Default value */
#define configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY            1
#define configALLOW_UNPRIVILEGED_CRITICAL_SECTIONS             1
#define configENABLE_ERRATA_837070_WORKAROUND                  1

/* ARMv8-M port specific configuration definitions. */
#define configENABLE_TRUSTZONE                                  0
#define configRUN_FREERTOS_SECURE_ONLY                          1
#define configENABLE_MPU                                        0
#define configENABLE_FPU                                        1
#define configENABLE_MVE                                        0

/* ARMv8-M secure side port related definitions. */
#define secureconfigMAX_SECURE_CONTEXTS         5

/* Optional functions - most linkers will remove unused functions anyway. */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     0
#define INCLUDE_uxTaskGetStackHighWaterMark2    0
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0
#define INCLUDE_xTaskResumeFromISR              1


#endif
