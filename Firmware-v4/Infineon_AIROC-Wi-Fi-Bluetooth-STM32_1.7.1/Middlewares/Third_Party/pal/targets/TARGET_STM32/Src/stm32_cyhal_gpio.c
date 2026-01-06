/***************************************************************************************************
 * \file cyhal_gpio.c
 *
 * \brief
 * Provides a high level interface for interacting with the STM32 GPIO. This is
 * a wrapper around the lower level PDL API.
 *
 ***************************************************************************************************
 * \copyright
 * Copyright 2021 Cypress Semiconductor Corporation
 * This software, including source code, documentation and related materials
 * ("Software"), is owned by Cypress Semiconductor Corporation or one of its
 * subsidiaries ("Cypress") and is protected by and subject to worldwide patent
 * protection (United States and foreign), United States copyright laws and
 * international treaty provisions. Therefore, you may use this Software only
 * as provided in the license agreement accompanying the software package from
 * which you obtained this Software ("EULA").
 *
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software source
 * code solely for use in connection with Cypress's integrated circuit products.
 * Any reproduction, modification, translation, compilation, or representation
 * of this Software except as specified above is prohibited without the express
 * written permission of Cypress.
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
 * including Cypress's product in a High Risk Product, the manufacturer of such
 * system or application assumes all risk of such use and in doing so agrees to
 * indemnify Cypress against all liability.
 **************************************************************************************************/

#include "cy_utils.h"
#include "cybsp.h"
#include "cyhal_gpio.h"
#include "cyhal_system.h"
#include "stm32_cyhal_gpio_ex.h"
#include "stm32_cyhal_gpio_pin.h"
#include "cyhal_hw_types.h"
#include "stm32_cyhal_common.h"
#include "stdint.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


/***************************************************************************************************
 *      Private macros
 **************************************************************************************************/

#if defined(GPIOA)
    #define PORT_GPIOA GPIOA
#else
    #define PORT_GPIOA NULL
#endif /* defined(GPIOA) */

#if defined(GPIOB)
    #define PORT_GPIOB GPIOB
#else
    #define PORT_GPIOB NULL
#endif /* defined(GPIOB) */

#if defined(GPIOC)
    #define PORT_GPIOC GPIOC
#else
    #define PORT_GPIOC NULL
#endif /* defined(GPIOC) */

#if defined(GPIOD)
    #define PORT_GPIOD GPIOD
#else
    #define PORT_GPIOD NULL
#endif /* defined(GPIOD) */

#if defined(GPIOE)
    #define PORT_GPIOE GPIOE
#else
    #define PORT_GPIOE NULL
#endif /* defined(GPIOE) */

#if defined(GPIOF)
    #define PORT_GPIOF GPIOF
#else
    #define PORT_GPIOF NULL
#endif /* defined(GPIOF) */

#if defined(GPIOG)
    #define PORT_GPIOG GPIOG
#else
    #define PORT_GPIOG NULL
#endif /* defined(GPIOG) */

#if defined(GPIOH)
    #define PORT_GPIOH GPIOH
#else
    #define PORT_GPIOH NULL
#endif /* defined(GPIOH) */

#if defined(GPIOI)
    #define PORT_GPIOI GPIOI
#else
    #define PORT_GPIOI NULL
#endif /* defined(GPIOI) */

#if defined(GPIOJ)
    #define PORT_GPIOJ GPIOJ
#else
    #define PORT_GPIOJ NULL
#endif /* defined(GPIOJ) */

#if defined(GPIOK)
    #define PORT_GPIOK GPIOK
#else
    #define PORT_GPIOK NULL
#endif /* defined(GPIOK) */

#define CYHAL_MAX_EXTI_NUMBER       (16U)

/* Return pin number by counts the number of leading zeros of a data value.
 * NOTE: __CLZ  returns  32 if no bits are set in the source register, and zero
 * if bit 31 is set. Parameter 'pin' should be in range GPIO_PIN0..GPIO_PIN15. */
#define CYHAL_GET_PIN_NUMBER(pin)   (uint32_t)(31u - __CLZ(pin))

/***************************************************************************************************
 *      Private types
 **************************************************************************************************/

typedef struct
{
    cyhal_gpio_event_callback_t callback;
    void*                       callback_args;
    bool                        enable;
    cyhal_gpio_t                pin;
} cyhal_gpio_event_callback_info_t;


/***************************************************************************************************
 *      Private globals
 **************************************************************************************************/

static cyhal_gpio_event_callback_info_t _exti_callbacks_info[CYHAL_MAX_EXTI_NUMBER] = { 0u };


/***************************************************************************************************
 *      Private functions
 **************************************************************************************************/

static GPIO_InitTypeDef _stm32_cyhal_gpio_get_configuration(const GPIO_TypeDef* port, uint16_t pin);
static void _stm32_cyhal_gpio_enable_irq(uint32_t pin_number, uint32_t priority, bool en_irq);


/***************************************************************************************************
 * cyhal_gpio_init
 **************************************************************************************************/
cy_rslt_t cyhal_gpio_init(cyhal_gpio_t pin, cyhal_gpio_direction_t direction,
                          cyhal_gpio_drive_mode_t drive_mode, bool init_val)
{
    cy_rslt_t status = CY_RSLT_SUCCESS;

    /* Return Failure if pin is configured as NC */
    if (pin == NC)
    {
        return CY_RSLT_TYPE_ERROR;
    }
    /* Get gpio port and pin from cyhal_gpio_t */
    GPIO_TypeDef* gpio_port = CYHAL_GET_PORT(pin);
    uint16_t      gpio_pin  = CYHAL_GET_PIN(pin);

    /* Enable clock for port */
    _stm32_cyhal_gpio_enable_clock(gpio_port);

    /* Get current configuration for port/pin. We do not modify .Speed and .alternate fields.
     * If the the user wants to set the .Speed, they can configure that in STM33CubeMX. */
    GPIO_InitTypeDef gpio_init = _stm32_cyhal_gpio_get_configuration(gpio_port, gpio_pin);

    /* Configure the drive mode */
    switch (drive_mode)
    {
        /* Digital Hi-Z. Input only. Input init value(s): 0 or 1 */
        case CYHAL_GPIO_DRIVE_NONE:
            gpio_init.Mode = GPIO_MODE_INPUT;
            gpio_init.Pull = GPIO_NOPULL;
            break;

        /* Analog Hi-Z. Use only for analog purpose */
        case CYHAL_GPIO_DRIVE_ANALOG:
            gpio_init.Mode = GPIO_MODE_ANALOG;
            gpio_init.Pull = GPIO_NOPULL;
            break;

        /* Pull-up resistor. Input and output.
         * Input init value(s): 1, output value(s): 0 */
        case CYHAL_GPIO_DRIVE_PULLUP:
            gpio_init.Mode = (direction == CYHAL_GPIO_DIR_INPUT) ? GPIO_MODE_INPUT :
                             GPIO_MODE_OUTPUT_PP;
            gpio_init.Pull = GPIO_PULLUP;
            break;

        /* Pull-down resistor. Input and output.
         * Input init value(s): 0, output value(s): 1 */
        case CYHAL_GPIO_DRIVE_PULLDOWN:
            gpio_init.Mode = (direction == CYHAL_GPIO_DIR_INPUT) ? GPIO_MODE_INPUT :
                             GPIO_MODE_OUTPUT_PP;
            gpio_init.Pull = GPIO_PULLDOWN;
            break;

        /* Open-drain, Drives Low. Input and output.
         * Input init value(s): 1, output value(s): 0 */
        case CYHAL_GPIO_DRIVE_OPENDRAINDRIVESLOW:
            gpio_init.Mode = (direction == CYHAL_GPIO_DIR_INPUT) ? GPIO_MODE_INPUT :
                             GPIO_MODE_OUTPUT_OD;
            gpio_init.Pull = GPIO_PULLDOWN;
            break;

        /* Open-drain, Drives High. Input and output.
         * Input init value(s): 0, output value(s): 1 */
        case CYHAL_GPIO_DRIVE_OPENDRAINDRIVESHIGH:
            gpio_init.Mode = (direction == CYHAL_GPIO_DIR_INPUT) ? GPIO_MODE_INPUT :
                             GPIO_MODE_OUTPUT_OD;
            gpio_init.Pull = GPIO_PULLUP;
            break;

        /* Strong output. Output only. Output init value(s): 0 or 1 */
        case CYHAL_GPIO_DRIVE_STRONG:
            gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
            gpio_init.Pull = GPIO_NOPULL;
            break;

        /* No Pull-up or pull-down resistors. Input and output.
         * Input init value(s): 0 or 1, output value(s): 0 or 1 */
        case CYHAL_GPIO_DRIVE_PULL_NONE:
            if ((direction == CYHAL_GPIO_DIR_OUTPUT) || (direction == CYHAL_GPIO_DIR_BIDIRECTIONAL))
            {
                gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
                gpio_init.Pull = GPIO_NOPULL;
            }
            else
            {
                gpio_init.Mode = GPIO_MODE_INPUT;
                gpio_init.Pull = GPIO_PULLUP;
            }
            break;

        case CYHAL_GPIO_DRIVE_PULLUPDOWN:
        default:
            assert_param(false);
            break;
    }

    /* Initializes the GPIOx peripheral according to the specified parameters in the gpio_init */
    HAL_GPIO_Init(gpio_port, &gpio_init);
    if (direction == CYHAL_GPIO_DIR_OUTPUT)
    {
        GPIO_PinState bitstatus = (init_val) ? GPIO_PIN_SET : GPIO_PIN_RESET;

        HAL_GPIO_WritePin(gpio_port, gpio_pin, bitstatus);

        /* The GPIO clock is significantly slower than the MCU clock.  This means that if the
         * application sets and initial value here, and then immediately calls cyhal_gpio_write()
         * to change that value, the initial value will never be seen on the GPIO.
         *
         * This occurs in the reset_wifi_chip() function in whd-bsp-integration\cybsp_wifi.c,
         * where the code is attempting to send a reset pulse to the wifi chip with the
         * CYBSP_WIFI_WL_REG_ON GPIO. */
        {
            uint32_t timeout = 0xFFFF;

            while ((HAL_GPIO_ReadPin(gpio_port, gpio_pin) != bitstatus) && (timeout != 0u))
            {
                /* wait for the state of the GPIO pin to be updated */
                timeout--;
            }
            assert_param(timeout > 0u);
        }
    }

    return status;
}


/***************************************************************************************************
 * cyhal_gpio_free
 **************************************************************************************************/
void cyhal_gpio_free(cyhal_gpio_t pin)
{
    /* Get pin number */
    uint32_t pin_number = CYHAL_GET_PIN_NUMBER(CYHAL_GET_PIN(pin));

    /* Check the parameters */
    if (pin_number < CYHAL_MAX_EXTI_NUMBER)
    {
        /* Clean callback information */
        uint32_t savedIntrStatus = cyhal_system_critical_section_enter();
        _exti_callbacks_info[pin_number].callback      = NULL;
        _exti_callbacks_info[pin_number].callback_args = NULL;
        _exti_callbacks_info[pin_number].enable        = false;
        _exti_callbacks_info[pin_number].pin           = NC;
        cyhal_system_critical_section_exit(savedIntrStatus);

        /* De-initializes the GPIOx peripheral registers to their default reset values. */
        HAL_GPIO_DeInit(CYHAL_GET_PORT(pin), CYHAL_GET_PIN(pin));
    }
    else
    {
        /* Wrong param */
        assert_param(false);
    }
}


/***************************************************************************************************
 * cyhal_gpio_read
 **************************************************************************************************/
bool cyhal_gpio_read(cyhal_gpio_t pin)
{
    return (bool)HAL_GPIO_ReadPin(CYHAL_GET_PORT(pin), CYHAL_GET_PIN(pin));
}


/***************************************************************************************************
 * cyhal_gpio_write
 **************************************************************************************************/
void cyhal_gpio_write(cyhal_gpio_t pin, bool value)
{
    HAL_GPIO_WritePin(CYHAL_GET_PORT(pin), CYHAL_GET_PIN(pin),
                      (value) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


/***************************************************************************************************
 * cyhal_gpio_enable_event
 **************************************************************************************************/
void cyhal_gpio_enable_event(cyhal_gpio_t pin, cyhal_gpio_event_t event, uint8_t intr_priority,
                             bool enable)
{
    /* Return if pin is configured as NC */
    if (pin == NC)
    {
        return;
    }

    /* Get gpio port and pin from cyhal_gpio_t */
    GPIO_TypeDef* gpio_port  = CYHAL_GET_PORT(pin);
    uint16_t      gpio_pin   = CYHAL_GET_PIN(pin);
    uint32_t      pin_number = CYHAL_GET_PIN_NUMBER(gpio_pin);

    /* Check the parameters */
    if (pin_number < CYHAL_MAX_EXTI_NUMBER)
    {
        /* Get current configuration */
        GPIO_InitTypeDef gpio_init = _stm32_cyhal_gpio_get_configuration(gpio_port, gpio_pin);

        /* Configure the drive mode to IT if event is enabled / or INPUT if event is disabled */
        if (enable)
        {
            gpio_init.Mode = (event == CYHAL_GPIO_IRQ_RISE) ? GPIO_MODE_IT_RISING :
                             (event == CYHAL_GPIO_IRQ_FALL) ?
                             GPIO_MODE_IT_FALLING : GPIO_MODE_IT_RISING_FALLING;
        }
        else
        {
            gpio_init.Mode = GPIO_MODE_INPUT;
        }

        /* Re-initializes the GPIOx peripheral according to the specified parameters in the
           gpio_init */
        HAL_GPIO_Init(gpio_port, &gpio_init);

        /* Enable/disable IRQ for appropriate EXTI lines */
        _stm32_cyhal_gpio_enable_irq(pin_number, intr_priority, enable);

        /* Update callback info */
        _exti_callbacks_info[pin_number].enable = enable;
    }
    else
    {
        /* Wrong param */
        assert_param(false);
    }
}


/***************************************************************************************************
 * cyhal_gpio_register_callback
 **************************************************************************************************/
void cyhal_gpio_register_callback(cyhal_gpio_t pin, cyhal_gpio_callback_data_t* callback_data)
{
    /* Return if pin is configured as NC */
    if (pin == NC)
    {
        return;
    }
    /* Get pin number */
    uint32_t pin_number = CYHAL_GET_PIN_NUMBER(CYHAL_GET_PIN(pin));

    /* Check the parameters */
    if ((pin_number < CYHAL_MAX_EXTI_NUMBER) && (callback_data->callback != NULL))
    {
        uint32_t savedIntrStatus = cyhal_system_critical_section_enter();
        _exti_callbacks_info[pin_number].callback      = callback_data->callback;
        _exti_callbacks_info[pin_number].callback_args = callback_data->callback_arg;
        _exti_callbacks_info[pin_number].pin           = pin;
        cyhal_system_critical_section_exit(savedIntrStatus);
    }
    else
    {
        /* Wrong param */
        assert_param(false);
    }
}


/***************************************************************************************************
 * stm32_cyhal_gpio_irq_handler
 **************************************************************************************************/
void stm32_cyhal_gpio_irq_handler(uint32_t gpio)
{
    uint32_t gpio_number = CYHAL_GET_PIN_NUMBER(gpio);

    /* Check the parameters */
    assert_param(gpio_number < CYHAL_MAX_EXTI_NUMBER);

    if ((gpio_number < CYHAL_MAX_EXTI_NUMBER) && (_exti_callbacks_info[gpio_number].enable))
    {
        cyhal_gpio_t       pin   = _exti_callbacks_info[gpio_number].pin;
        cyhal_gpio_event_t event = (cyhal_gpio_read(pin) == true) ?
                                   CYHAL_GPIO_IRQ_RISE : CYHAL_GPIO_IRQ_FALL;

        /* Call user's callback */
        (void)(_exti_callbacks_info[gpio_number].callback)
            (_exti_callbacks_info[gpio_number].callback_args, event);
    }
}


/***************************************************************************************************
 * Private functions
 **************************************************************************************************/

/***************************************************************************************************
 * _stm32_cyhal_gpio_get_configuration
 **************************************************************************************************/
static GPIO_InitTypeDef _stm32_cyhal_gpio_get_configuration(const GPIO_TypeDef* port, uint16_t pin)
{
    GPIO_InitTypeDef gpio_init = { 0u };
    uint32_t         temp;
    uint32_t         position = CYHAL_GET_PIN_NUMBER(pin);

    /* Check the parameters */
    if (position < CYHAL_MAX_EXTI_NUMBER)
    {
        /* NOTE: all manipulations with position
         * done because of specific of SMTs Port/Pin
         * registers organization */

        /* Store Pin */
        gpio_init.Pin = pin;

        /* Get Pull */
        temp           = port->PUPDR;
        temp          &= (GPIO_PUPDR_PUPD0 << (position * 2U));
        gpio_init.Pull = temp >> (position * 2U);

        /* Get the IO Speed */
        temp            = port->OSPEEDR;
        temp           &= (GPIO_OSPEEDR_OSPEED0 << (position * 2U));
        gpio_init.Speed = temp >> (position * 2U);

        /* Get Alternate function mapped with the current IO */
        temp                = port->AFR[position >> 3U];
        temp               &= (uint32_t)(0xFU << ((position & 0x07U) * 4U));
        gpio_init.Alternate = temp >> ((position & 0x07U) * 4U);
    }
    else
    {
        /* Wrong param */
        assert_param(false);
    }

    return (gpio_init);
}


/***************************************************************************************************
 * _stm32_cyhal_gpio_get_port
 **************************************************************************************************/
GPIO_TypeDef* _stm32_cyhal_gpio_get_port(cyhal_gpio_t pin)
{
    static GPIO_TypeDef* const gpiox_table[] =
    {
        PORT_GPIOA,
        PORT_GPIOB,
        PORT_GPIOC,
        PORT_GPIOD,
        PORT_GPIOE,
        PORT_GPIOF,
        PORT_GPIOG,
        PORT_GPIOH,
        PORT_GPIOI,
        PORT_GPIOJ,
        PORT_GPIOK
    };

    uint32_t gpiox_table_size = sizeof(gpiox_table) / sizeof(GPIO_TypeDef*);
    uint32_t index            = (((uint32_t)pin) >> 16U) & 0xFFFFUL;

    /* Check index range */
    if (index >= gpiox_table_size)
    {
        assert_param(false);
        index = 0u;
    }

    return (gpiox_table[index]);
}


/***************************************************************************************************
 * _stm32_cyhal_gpio_enable_clock
 **************************************************************************************************/
void _stm32_cyhal_gpio_enable_clock(const GPIO_TypeDef* gpio_port)
{
    #if defined (GPIOA)
    if (gpio_port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    else
    #endif

    #if defined (GPIOB)
    if (gpio_port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    else
    #endif

    #if defined (GPIOC)
    if (gpio_port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
    else
    #endif

    #if defined (GPIOD)
    if (gpio_port == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
    else
    #endif

    #if defined (GPIOE)
    if (gpio_port == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }
    else
    #endif

    #if defined (GPIOF)
    if (gpio_port == GPIOF)
    {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    }
    else
    #endif

    #if defined (GPIOG)
    if (gpio_port == GPIOG)
    {
        __HAL_RCC_GPIOG_CLK_ENABLE();
    }
    else
    #endif

    #if defined (GPIOH)
    if (gpio_port == GPIOH)
    {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    }
    else
    #endif

    #if defined (GPIOI)
    if (gpio_port == GPIOI)
    {
        __HAL_RCC_GPIOI_CLK_ENABLE();
    }
    else
    #endif

    #if defined (GPIOJ)
    if (gpio_port == GPIOJ)
    {
        __HAL_RCC_GPIOJ_CLK_ENABLE();
    }
    else
    #endif

    #if defined (GPIOK)
    if (gpio_port == GPIOK)
    {
        __HAL_RCC_GPIOK_CLK_ENABLE();
    }
    else
    #endif
    {
        assert_param(false);
    }
}


/***************************************************************************************************
 * _stm32_cyhal_gpio_enable_irq
 **************************************************************************************************/
static void _stm32_cyhal_gpio_enable_irq(uint32_t pin_number, uint32_t priority, bool en_irq)
{
    IRQn_Type IRQn = (IRQn_Type)0;
    /* Use the priority, configured by STM32CubeMx if CYHAL_GPIO_USE_HAL_IRQ_PRIOPITY
     * has not been defined */
    #if !defined(CYHAL_GPIO_USE_HAL_IRQ_PRIOPITY)
    (void)priority;
    #endif /* !defined(CYHAL_GPIO_USE_HAL_IRQ_PRIOPITY) */

    /* Check the parameters */
    assert_param(pin_number < CYHAL_MAX_EXTI_NUMBER);

    /* Map table pins number and EXTI IRQ. */
    #if defined (TARGET_STM32H7xx)
    const IRQn_Type exti_table[] =
    {
        EXTI0_IRQn,     /* IRQ for EXTI line 0 */
        EXTI1_IRQn,     /* IRQ for EXTI line 1 */
        EXTI2_IRQn,     /* IRQ for EXTI line 2 */
        EXTI3_IRQn,     /* IRQ for EXTI line 3 */
        EXTI4_IRQn,     /* IRQ for EXTI line 4 */
        EXTI9_5_IRQn,   /* IRQ for EXTI line 5 */
        EXTI9_5_IRQn,   /* IRQ for EXTI line 6 */
        EXTI9_5_IRQn,   /* IRQ for EXTI line 7 */
        EXTI9_5_IRQn,   /* IRQ for EXTI line 8 */
        EXTI9_5_IRQn,   /* IRQ for EXTI line 9 */
        EXTI15_10_IRQn, /* IRQ for EXTI line 10 */
        EXTI15_10_IRQn, /* IRQ for EXTI line 11 */
        EXTI15_10_IRQn, /* IRQ for EXTI line 12 */
        EXTI15_10_IRQn, /* IRQ for EXTI line 13 */
        EXTI15_10_IRQn, /* IRQ for EXTI line 14 */
        EXTI15_10_IRQn, /* IRQ for EXTI line 15 */
    };
    #elif defined (TARGET_STM32L5xx) || defined (TARGET_STM32U5xx)
    const IRQn_Type exti_table[] =
    {
        EXTI0_IRQn,  /* IRQ for EXTI line 0 */
        EXTI1_IRQn,  /* IRQ for EXTI line 1 */
        EXTI2_IRQn,  /* IRQ for EXTI line 2 */
        EXTI3_IRQn,  /* IRQ for EXTI line 3 */
        EXTI4_IRQn,  /* IRQ for EXTI line 4 */
        EXTI5_IRQn,  /* IRQ for EXTI line 5 */
        EXTI6_IRQn,  /* IRQ for EXTI line 6 */
        EXTI7_IRQn,  /* IRQ for EXTI line 7 */
        EXTI8_IRQn,  /* IRQ for EXTI line 8 */
        EXTI9_IRQn,  /* IRQ for EXTI line 9 */
        EXTI10_IRQn, /* IRQ for EXTI line 10 */
        EXTI11_IRQn, /* IRQ for EXTI line 11 */
        EXTI12_IRQn, /* IRQ for EXTI line 12 */
        EXTI13_IRQn, /* IRQ for EXTI line 13 */
        EXTI14_IRQn, /* IRQ for EXTI line 14 */
        EXTI15_IRQn, /* IRQ for EXTI line 15 */
    };
    #elif defined (TARGET_STM32H5xx)
    const IRQn_Type exti_table[] =
    {
        EXTI0_IRQn, /* IRQ for EXTI line 0 */
        EXTI1_IRQn, /* IRQ for EXTI line 1 */
        EXTI2_IRQn, /* IRQ for EXTI line 2 */
        EXTI3_IRQn, /* IRQ for EXTI line 3 */
        EXTI4_IRQn, /* IRQ for EXTI line 4 */
        EXTI5_IRQn, /* IRQ for EXTI line 5 */
        EXTI6_IRQn, /* IRQ for EXTI line 6 */
        EXTI7_IRQn, /* IRQ for EXTI line 7 */
        EXTI8_IRQn, /* IRQ for EXTI line 8 */
        EXTI9_IRQn, /* IRQ for EXTI line 9 */
        EXTI10_IRQn, /* IRQ for EXTI line 10 */
        EXTI11_IRQn, /* IRQ for EXTI line 11 */
        EXTI12_IRQn, /* IRQ for EXTI line 12 */
        EXTI13_IRQn, /* IRQ for EXTI line 13 */
        EXTI14_IRQn, /* IRQ for EXTI line 14 */
        EXTI15_IRQn, /* IRQ for EXTI line 15 */
    };
    #endif /* if defined (TARGET_STM32H7xx) */

    IRQn = exti_table[pin_number];

    /* Enable/Disable IRQ */
    if (en_irq)
    {
        /* Use the priority, configured by STM32CubeMx if CYHAL_GPIO_USE_HAL_IRQ_PRIOPITY
         * has not been defined */
        #if defined(CYHAL_GPIO_USE_HAL_IRQ_PRIOPITY)
        HAL_NVIC_SetPriority(IRQn, priority, 0);
        #endif /* defined(CYHAL_GPIO_USE_HAL_IRQ_PRIOPITY) */

        HAL_NVIC_EnableIRQ(IRQn);
    }
    else
    {
        #if defined (TARGET_STM32H7xx)
        /* Disable only EXTIx..EXTI4 interrupts.
         * Others can be used in application so do not disable EXTI5..EXTI15. */
        if ((IRQn == EXTI0_IRQn) || (IRQn == EXTI1_IRQn) || (IRQn == EXTI2_IRQn) ||
            (IRQn == EXTI3_IRQn) || (IRQn == EXTI4_IRQn))
        #endif /* if defined (TARGET_STM32H7xx) */
        {
            HAL_NVIC_DisableIRQ(IRQn);
        }
    }
}


#if defined(__cplusplus)
}
#endif /* __cplusplus */
