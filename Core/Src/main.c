#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CTRL_REG1       0x20
#define CTRL_REG2       0x21
#define CTRL_REG4       0x23
#define OUT_TEMP        0x26
#define OUT_X_L         0x28

#define SPI_READ_BIT    0x80
#define SPI_MS_BIT      0x40

#define SENSITIVITY_2000DPS  0.070f

int16_t gyro_x = 0;
int16_t gyro_y = 0;
int16_t gyro_z = 0;
int8_t  temp   = 0;

SPI_HandleTypeDef  hspi5;
UART_HandleTypeDef huart1;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI5_Init(void);
static void MX_USART1_UART_Init(void);

#define CS_LOW()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET)

void WriteOneByte(uint8_t reg, uint8_t data)
{
    uint8_t tx[2] = { reg & ~SPI_READ_BIT, data };
    CS_LOW();
    HAL_SPI_Transmit(&hspi5, tx, 2, HAL_MAX_DELAY);
    CS_HIGH();
}

void BurstRead(uint8_t startReg, uint8_t *pBuf, uint8_t len)
{
    uint8_t cmd = startReg | SPI_READ_BIT | SPI_MS_BIT;
    CS_LOW();
    HAL_SPI_Transmit(&hspi5, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive (&hspi5, pBuf, len, HAL_MAX_DELAY);
    CS_HIGH();
}

uint8_t ReadOneByte(uint8_t reg)
{
    uint8_t cmd = reg | SPI_READ_BIT;
    uint8_t rx  = 0;
    CS_LOW();
    HAL_SPI_Transmit(&hspi5, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive (&hspi5, &rx,  1, HAL_MAX_DELAY);
    CS_HIGH();
    return rx;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI5_Init();
    MX_USART1_UART_Init();

    CS_HIGH();
    HAL_Delay(10);


    WriteOneByte(CTRL_REG1, 0xEF);
    WriteOneByte(CTRL_REG2, 0x00);
    WriteOneByte(CTRL_REG4, 0x30);

    char    txBuf[64];
    uint8_t rawBuf[6];

    while (1)
    {

        BurstRead(OUT_X_L, rawBuf, 6);

        gyro_x = (int16_t)((rawBuf[1] << 8) | rawBuf[0]);
        gyro_y = (int16_t)((rawBuf[3] << 8) | rawBuf[2]);
        gyro_z = (int16_t)((rawBuf[5] << 8) | rawBuf[4]);
        temp   = (int8_t)ReadOneByte(OUT_TEMP);

        int16_t x_int = (int16_t)(gyro_x * SENSITIVITY_2000DPS * 100);
        int16_t y_int = (int16_t)(gyro_y * SENSITIVITY_2000DPS * 100);
        int16_t z_int = (int16_t)(gyro_z * SENSITIVITY_2000DPS * 100);

        int len = snprintf(txBuf, sizeof(txBuf),
                           "G,%d.%02d,%d.%02d,%d.%02d,%d\r\n",
                           x_int/100, abs(x_int%100),
                           y_int/100, abs(y_int%100),
                           z_int/100, abs(z_int%100),
                           (int)temp);

        HAL_UART_Transmit(&huart1, (uint8_t *)txBuf, (uint16_t)len, HAL_MAX_DELAY);

        HAL_Delay(10);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 4;
    RCC_OscInitStruct.PLL.PLLN       = 180;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    if (HAL_PWREx_EnableOverDrive() != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) Error_Handler();
}

static void MX_SPI5_Init(void)
{
    hspi5.Instance               = SPI5;
    hspi5.Init.Mode              = SPI_MODE_MASTER;
    hspi5.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi5.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi5.Init.CLKPolarity       = SPI_POLARITY_HIGH;
    hspi5.Init.CLKPhase          = SPI_PHASE_2EDGE;
    hspi5.Init.NSS               = SPI_NSS_SOFT;

    hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi5.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi5.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi5.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi5.Init.CRCPolynomial     = 10;
    if (HAL_SPI_Init(&hspi5) != HAL_OK) Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();


    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif
