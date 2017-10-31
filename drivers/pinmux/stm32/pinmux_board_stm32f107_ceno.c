/*
 * Copyright (c) 2017 RnDity Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <pinmux.h>
#include <sys_io.h>
#include "pinmux/pinmux.h"

#include "pinmux_stm32.h"

/* pin assignments for STM32F107-CENO board */
static const struct pin_config pinconf[] = {
#ifdef CONFIG_UART_STM32_PORT_1
	{STM32_PIN_PA9, STM32F1_PINMUX_FUNC_PA9_USART2_TX},
	{STM32_PIN_PA10, STM32F1_PINMUX_FUNC_PA10_USART2_RX},
#endif	/* CONFIG_UART_STM32_PORT_2 */
#ifdef CONFIG_SPI_1
        {STM32_PIN_PA4, STM32F1X_PINMUX_FUNC_PA4_SPI1_NSS},
        {STM32_PIN_PA5, STM32F1X_PINMUX_FUNC_PA5_SPI1_SCK},
        {STM32_PIN_PA6, STM32F1X_PINMUX_FUNC_PA6_SPI1_MISO},
        {STM32_PIN_PB5, STM32F1X_PINMUX_FUNC_PB5_SPI1_MOSI},
#endif /* CONFIG_SPI_1 */
};

static int pinmux_stm32_init(struct device *port)
{
	ARG_UNUSED(port);

	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));

	return 0;
}

SYS_INIT(pinmux_stm32_init, PRE_KERNEL_1,
	 CONFIG_PINMUX_STM32_DEVICE_INITIALIZATION_PRIORITY);
