#ifndef USART_DMA_H
#define USART_DMA_H

#include "stm32f0xx_ll_usart.h"
#include "stm32f0xx_ll_gpio.h"
#include "stm32f0xx_ll_dma.h"
#include "stm32f0xx_ll_bus.h"
#include "stm32f0xx_ll_system.h"
#include <stdint.h>

#define UART_TX_BUFFER_SIZE 64
#define UART_RX_BUFFER_SIZE 64

void USART_DMA_Init(void);
void USART_DMA_Send(const char *data);

#endif
