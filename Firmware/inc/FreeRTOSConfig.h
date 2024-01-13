#include <stm32h7xx.h>

#ifndef FREERTOSCONFIG_H
#define FREERTOSCONFIG_H

#define configNUMBER_OF_CORES           2
#define configUSE_CORE_AFFINITY         1
#define configRUN_MULTIPLE_PRIORITIES   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configALLOW_UNPRIVILEGED_CRITICAL_SECTIONS  0

#define configSYSTEM_CALL_STACK_SIZE    128
#define configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY 1

#define configUSE_MPU                   1
#define configUSE_PREEMPTION			1
#define configUSE_IDLE_HOOK				1
#define configUSE_PASSIVE_IDLE_HOOK     1
#define configUSE_TICK_HOOK				0
//#define configCPU_CLOCK_HZ				( "don't use this" )
#define configSYSTICK_CLOCK_HZ			( 4000000 )	/* PER_CK = HSE16, LPTIM1 prescaler /4 */
#define configTICK_RATE_HZ				( ( TickType_t ) 100 )
#define configMAX_PRIORITIES			( 5 )
#define configMINIMAL_STACK_SIZE		( ( unsigned short ) 130 )
#define configTOTAL_HEAP_SIZE			( ( size_t ) ( 64 * 1024 ) )
#define configAPPLICATION_ALLOCATED_HEAP	1
#define configSUPPORT_DYNAMIC_ALLOCATION	1
#define configSUPPORT_STATIC_ALLOCATION		0
#define configMAX_TASK_NAME_LEN			( 10 )
#define configUSE_TRACE_FACILITY		1
#define configUSE_16_BIT_TICKS			0
#define configIDLE_SHOULD_YIELD			1
#define configUSE_MUTEXES				1
#define configQUEUE_REGISTRY_SIZE		8
#define configCHECK_FOR_STACK_OVERFLOW	2
#define configUSE_RECURSIVE_MUTEXES		1
#define configUSE_MALLOC_FAILED_HOOK	1
#define configUSE_APPLICATION_TASK_TAG	0
#define configUSE_COUNTING_SEMAPHORES	1
#define configGENERATE_RUN_TIME_STATS	0
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() { /* nothing*/ }
#define portGET_RUN_TIME_COUNTER_VALUE() ( "TODO" )
#define configUSE_STATS_FORMATTING_FUNCTIONS	1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 4
#define configUSE_NEWLIB_REENTRANT		1

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES 		0
#define configMAX_CO_ROUTINE_PRIORITIES ( 2 )

/* Software timer definitions. */
#define configUSE_TIMERS				1
#define configTIMER_TASK_PRIORITY		( 2 )
#define configTIMER_QUEUE_LENGTH		10
#define configTIMER_TASK_STACK_DEPTH	( configMINIMAL_STACK_SIZE * 2 )

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
#define INCLUDE_vTaskPrioritySet		1
#define INCLUDE_uxTaskPriorityGet		1
#define INCLUDE_vTaskDelete				1
#define INCLUDE_vTaskCleanUpResources	1
#define INCLUDE_vTaskSuspend			1
#define INCLUDE_vTaskDelayUntil			1
#define INCLUDE_vTaskDelay				1

/* Cortex-M specific definitions. */
#ifdef __NVIC_PRIO_BITS
	/* __BVIC_PRIO_BITS will be specified when CMSIS is being used. */
	#define configPRIO_BITS       		__NVIC_PRIO_BITS
#else
	#define configPRIO_BITS       		4        /* 15 priority levels */
#endif

/* The lowest interrupt priority that can be used in a call to a "set priority"
function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY			0xf

/* The highest interrupt priority that can be used by any interrupt service
routine that makes calls to interrupt safe FreeRTOS API functions.  DO NOT CALL
INTERRUPT SAFE FREERTOS API FUNCTIONS FROM ANY INTERRUPT THAT HAS A HIGHER
PRIORITY THAN THIS! (higher priorities are lower numeric values. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY	5

/* Interrupt priorities used by the kernel port layer itself.  These are generic
to all Cortex-M ports, and do not rely on any particular library functions. */
#define configKERNEL_INTERRUPT_PRIORITY 		( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
	
/* Normal assert() semantics without relying on the provision of an assert.h
header file. */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }	
	
/* Definitions that map the FreeRTOS port interrupt handlers to their CMSIS
standard names. */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler LPTIM1_Handler



#endif
