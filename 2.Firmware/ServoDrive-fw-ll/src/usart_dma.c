#include "usart_dma.h"
#include <string.h>

static uint8_t uart_tx_buffer[UART_TX_BUFFER_SIZE];
static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];

static volatile uint8_t tx_busy = 0;

void USART_DMA_Init(void)
{
    // 1. 时钟
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_USART1);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

    // 2. GPIO
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = LL_GPIO_PIN_9 | LL_GPIO_PIN_10;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 3. USART1
    LL_USART_Disable(USART1);
    LL_USART_SetTransferDirection(USART1, LL_USART_DIRECTION_TX_RX);
    LL_USART_SetParity(USART1, LL_USART_PARITY_NONE);
    LL_USART_SetDataWidth(USART1, LL_USART_DATAWIDTH_8B);
    LL_USART_SetStopBitsLength(USART1, LL_USART_STOPBITS_1);
    LL_USART_SetBaudRate(USART1, SystemCoreClock, LL_USART_OVERSAMPLING_16, 115200);
    LL_USART_EnableDMAReq_TX(USART1);
    LL_USART_EnableDMAReq_RX(USART1);
    LL_USART_EnableIT_IDLE(USART1); // IDLE 中断
    LL_USART_Enable(USART1);

    // 4. DMA TX
    LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_2,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH |
        LL_DMA_PRIORITY_LOW |
        LL_DMA_MODE_NORMAL |
        LL_DMA_PERIPH_NOINCREMENT |
        LL_DMA_MEMORY_INCREMENT |
        LL_DMA_PDATAALIGN_BYTE |
        LL_DMA_MDATAALIGN_BYTE);
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)&USART1->TDR);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_2);

    // 5. DMA RX 循环模式
    LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_3,
        LL_DMA_DIRECTION_PERIPH_TO_MEMORY |
        LL_DMA_PRIORITY_LOW |
        LL_DMA_MODE_CIRCULAR |
        LL_DMA_PERIPH_NOINCREMENT |
        LL_DMA_MEMORY_INCREMENT |
        LL_DMA_PDATAALIGN_BYTE |
        LL_DMA_MDATAALIGN_BYTE);
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_3, (uint32_t)&USART1->RDR);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_3, (uint32_t)uart_rx_buffer);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, UART_RX_BUFFER_SIZE);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_3);

    // 6. NVIC
    NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);
}

void USART_DMA_Send(const char *data)
{
    while (tx_busy); // 等待上次发送完成

    size_t len = strlen(data);
    if (len > UART_TX_BUFFER_SIZE) len = UART_TX_BUFFER_SIZE;
    memcpy(uart_tx_buffer, data, len);

    tx_busy = 1;

    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)uart_tx_buffer);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, len);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
}

// DMA1 Channel2_3 中断
void DMA1_Channel2_3_IRQHandler(void)
{
    // TX 完成
    if (LL_DMA_IsActiveFlag_TC2(DMA1))
    {
        LL_DMA_ClearFlag_TC2(DMA1);
        tx_busy = 0;
    }
}

// USART1 中断（IDLE）
void USART1_IRQHandler(void)
{
    if (LL_USART_IsActiveFlag_IDLE(USART1))
    {
        LL_USART_ClearFlag_IDLE(USART1);

        // DMA 当前写入位置
        size_t pos = UART_RX_BUFFER_SIZE - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_3);
        if (pos == 0) return;

        // 倒序到 TX buffer
        for (size_t i = 0; i < pos; i++)
            uart_tx_buffer[i] = uart_rx_buffer[pos - 1 - i];
        // 发回
        USART_DMA_Send((char *)uart_tx_buffer);

        // **清除已经处理的数据**：这里简单清零，避免下次残留
        memset(uart_rx_buffer, 0, pos);

        // **手动更新 DMA 数据长度**，循环模式下保持长度不变即可
        // LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, UART_RX_BUFFER_SIZE); // 可选
    }
}
