/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Lab 8 - Pantalla TFT ILI9341 (SPI) - Termometro / EOG / Ritmo cardiaco
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ili9341.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Que pantalla mostrar: 0 = ciclo automatico de las 3, 1 = termometro, 2 = EOG, 3 = ritmo cardiaco */
#define UI_MODE 0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* Bitmaps 16x16 1bpp (bit15 = columna izquierda) */
static const uint16_t bmp_heart[16] = {
  0x0000,0x0C30,0x1E78,0x3FFC,0x7FFE,0x7FFE,0x7FFE,0x7FFE,
  0x3FFC,0x1FF8,0x0FF0,0x07E0,0x03C0,0x0180,0x0000,0x0000
};
static const uint16_t bmp_thermo[16] = {
  0x0180,0x03C0,0x03C0,0x03C0,0x03C0,0x03C0,0x03C0,0x03C0,
  0x03C0,0x03C0,0x07E0,0x0FF0,0x0FF0,0x0FF0,0x07E0,0x03C0
};
static const uint16_t bmp_eye[16] = {
  0x0000,0x0000,0x07E0,0x0FF0,0x1FF8,0x3E7C,0x7C3E,0xF81F,
  0xF81F,0x7C3E,0x3E7C,0x1FF8,0x0FF0,0x07E0,0x0000,0x0000
};

/* Un latido PQRST: 38 muestras de 2 px (desplazamiento vertical respecto a la linea base) */
static const int8_t ecg_beat[38] = {
  0,0,0,0,-3,-6,-6,-3,0,0,0,
  4,-30,-44,12,0,0,0,0,0,
  -3,-6,-9,-11,-11,-9,-6,-3,0,0,0,0,0,0,0,0,0,0
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void Draw_Thermometer(uint16_t t10);              /* t10 = temperatura x10 (368 = 36.8 C) */
static void Draw_EOG(uint16_t ppm, uint8_t comprension);
static void Draw_HeartRate(uint16_t bpm);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Texto centrado horizontalmente en [x, x+w) */
static void text_centered(uint16_t x, uint16_t w, uint16_t y, const char *s, uint8_t sc, uint16_t fg, uint16_t bg)
{
    uint16_t tw = ILI9341_StringWidth(s, sc) - sc;
    ILI9341_DrawString(x + (w - tw) / 2, y, s, sc, fg, bg);
}

/* Segmento vertical de 2 px de ancho entre dos alturas (para dibujar ondas) */
static void plot_sample(uint16_t x, int16_t y0, int16_t y1, uint16_t col)
{
    int16_t a = (y0 < y1) ? y0 : y1;
    int16_t b = (y0 < y1) ? y1 : y0;
    ILI9341_FillRect(x, a, 2, b - a + 2, col);
}

/* Cuadricula de fondo para las zonas de ondas */
static void draw_grid(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t col)
{
    for (uint16_t gx = x + 20; gx < x + w; gx += 20) ILI9341_FillRect(gx, y, 1, h, col);
    for (uint16_t gy = y + 20; gy < y + h; gy += 20) ILI9341_FillRect(x, gy, w, 1, col);
}

/* ===================== 1) TERMOMETRO DIGITAL ===================== */
#define TEMP_X(t10)  (16 + (((int32_t)(t10) - 340) * 288) / 80)   /* 34.0..42.0 C -> x 16..304 */

static void Draw_Thermometer(uint16_t t10)
{
    char buf[12];
    const uint16_t TEAL   = RGB565(0, 150, 136);
    const uint16_t PANEL  = RGB565(0, 40, 50);
    const uint16_t ORANGE = RGB565(255, 90, 40);

    if (t10 < 340) t10 = 340;
    if (t10 > 420) t10 = 420;

    ILI9341_FillScreen(COL_BG);

    /* Barra de titulo */
    ILI9341_FillRect(8, 8, 304, 36, TEAL);
    ILI9341_DrawBitmap1bpp16(16, 14, bmp_thermo, 2, COL_WHITE);
    ILI9341_DrawString(62, 16, "TERMOMETRO", 3, COL_WHITE, TEAL);

    /* ---- Panel izquierdo: termometro vertical ---- */
    ILI9341_FillRect(8, 54, 90, 130, PANEL);
    ILI9341_DrawRoundRect(8, 54, 90, 130, 3, TEAL);
    ILI9341_FillRect(43, 60, 24, 76, COL_WHITE);          /* tubo */
    ILI9341_FillRect(46, 63, 18, 70, 0x2104);             /* interior del tubo */
    ILI9341_FillRect(33, 136, 44, 40, COL_WHITE);         /* bulbo */
    ILI9341_FillRect(36, 139, 38, 34, ORANGE);
    uint16_t h = ((uint32_t)(t10 - 340) * 70) / 80;       /* altura del liquido */
    ILI9341_FillRect(46, 133 - h, 18, h + 8, ORANGE);     /* liquido + union con el bulbo */
    /* marcas de escala 35, 37, 39, 41 */
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t deg = 35 + i * 2;
        uint16_t ty = 133 - ((deg - 34) * 35) / 4;        /* 8.75 px por grado */
        ILI9341_FillRect(70, ty, 8, 2, COL_WHITE);
        sprintf(buf, "%u", deg);
        ILI9341_DrawString(80, ty - 3, buf, 1, COL_WHITE, PANEL);
    }

    /* ---- Panel derecho: valor ---- */
    ILI9341_FillRect(106, 54, 206, 130, PANEL);
    ILI9341_DrawRoundRect(106, 54, 206, 130, 3, TEAL);
    ILI9341_DrawString(116, 64, "TEMPERATURA", 2, TEAL, PANEL);
    sprintf(buf, "%u.%u", t10 / 10, t10 % 10);
    ILI9341_DrawString(116, 92, buf, 6, COL_WHITE, PANEL);
    ILI9341_DrawRect(264, 98, 8, 8, 2, COL_WHITE);        /* simbolo de grado */
    ILI9341_DrawString(276, 100, "C", 4, COL_WHITE, PANEL);

    /* Estado segun el valor */
    const char *txt; uint16_t col;
    if      (t10 < 361) { txt = "BAJA";    col = COL_CYAN;   }
    else if (t10 < 373) { txt = "NORMAL";  col = COL_GREEN;  }
    else if (t10 < 380) { txt = "ELEVADA"; col = COL_YELLOW; }
    else                { txt = "FIEBRE";  col = COL_RED;    }
    ILI9341_FillRect(116, 148, 186, 28, col);
    text_centered(116, 186, 154, txt, 2, COL_BLACK, col);

    /* ---- Barra inferior con zonas y marcador ---- */
    ILI9341_FillRect(8, 194, 304, 38, PANEL);
    ILI9341_DrawRoundRect(8, 194, 304, 38, 2, TEAL);
    ILI9341_FillRect(TEMP_X(340), 206, TEMP_X(361) - TEMP_X(340), 12, COL_CYAN);
    ILI9341_FillRect(TEMP_X(361), 206, TEMP_X(373) - TEMP_X(361), 12, COL_GREEN);
    ILI9341_FillRect(TEMP_X(373), 206, TEMP_X(380) - TEMP_X(373), 12, COL_YELLOW);
    ILI9341_FillRect(TEMP_X(380), 206, TEMP_X(420) - TEMP_X(380), 12, COL_RED);
    ILI9341_FillRect(TEMP_X(t10) - 2, 199, 4, 22, COL_WHITE);
    ILI9341_DrawString(16, 222, "34", 1, COL_WHITE, PANEL);
    ILI9341_DrawString(292, 222, "42", 1, COL_WHITE, PANEL);
}

/* ===================== 2) EOG (ELECTROOCULOGRAFIA) ===================== */
static void Draw_EOG(uint16_t ppm, uint8_t comprension)
{
    char buf[8];
    const uint16_t PURPLE = RGB565(110, 50, 180);
    const uint16_t PANEL  = RGB565(25, 15, 45);

    ILI9341_FillScreen(COL_BG);

    /* Barra de titulo */
    ILI9341_FillRect(8, 8, 304, 36, PURPLE);
    ILI9341_DrawBitmap1bpp16(16, 14, bmp_eye, 2, COL_WHITE);
    ILI9341_DrawString(62, 16, "EOG", 3, COL_WHITE, PURPLE);
    ILI9341_DrawString(160, 20, "LECTURA", 2, COL_CYAN, PURPLE);

    /* ---- Senal EOG: escalera de fijaciones y barrido de retorno ---- */
    ILI9341_FillRect(8, 54, 304, 76, COL_BLACK);
    draw_grid(8, 54, 304, 76, 0x0320);
    ILI9341_DrawRect(8, 54, 304, 76, 2, PURPLE);
    ILI9341_DrawString(14, 58, "CANAL H", 1, COL_CYAN, COL_BLACK);
    int16_t prev = 124;
    for (uint16_t i = 0; i < 148; i++) {
        uint16_t phase = i % 37;
        int16_t y = 124 - (phase / 6) * 9;
        plot_sample(10 + i * 2, prev, y, COL_GREEN);
        prev = y;
    }

    /* ---- Panel velocidad de lectura ---- */
    ILI9341_FillRect(8, 138, 150, 94, PANEL);
    ILI9341_DrawRoundRect(8, 138, 150, 94, 3, COL_CYAN);
    ILI9341_DrawString(18, 148, "VELOCIDAD", 2, COL_CYAN, PANEL);
    sprintf(buf, "%u", ppm);
    ILI9341_DrawString(18, 174, buf, 5, COL_WHITE, PANEL);
    ILI9341_DrawString(112, 192, "PPM", 2, COL_WHITE, PANEL);

    /* ---- Panel comprension ---- */
    ILI9341_FillRect(162, 138, 150, 94, PANEL);
    ILI9341_DrawRoundRect(162, 138, 150, 94, 3, COL_YELLOW);
    ILI9341_DrawString(170, 148, "COMPRENSION", 2, COL_YELLOW, PANEL);
    sprintf(buf, "%u", comprension);
    ILI9341_DrawString(172, 172, buf, 5, COL_WHITE, PANEL);
    ILI9341_DrawString(240, 176, "%", 4, COL_WHITE, PANEL);
    ILI9341_FillRect(172, 216, 130, 8, 0x2104);
    ILI9341_FillRect(172, 216, (130 * comprension) / 100, 8, COL_GREEN);
}

/* ===================== 3) RITMO CARDIACO ===================== */
static void Draw_HeartRate(uint16_t bpm)
{
    char buf[8];
    const uint16_t DARKRED = RGB565(130, 15, 25);
    const uint16_t PANEL   = 0x2000;

    ILI9341_FillScreen(COL_BG);

    /* Barra de titulo */
    ILI9341_FillRect(8, 8, 304, 36, DARKRED);
    ILI9341_DrawBitmap1bpp16(16, 14, bmp_heart, 2, COL_WHITE);
    ILI9341_DrawString(56, 16, "RITMO CARDIACO", 3, COL_WHITE, DARKRED);

    /* ---- ECG ---- */
    ILI9341_FillRect(8, 54, 304, 80, COL_BLACK);
    draw_grid(8, 54, 304, 80, 0x0320);
    ILI9341_DrawRect(8, 54, 304, 80, 2, COL_RED);
    int16_t prev = 112;
    for (uint16_t i = 0; i < 148; i++) {
        int16_t y = 112 + ecg_beat[i % 38];
        plot_sample(10 + i * 2, prev, y, COL_GREEN);
        prev = y;
    }

    /* ---- Panel principal ---- */
    ILI9341_FillRect(8, 142, 304, 90, PANEL);
    ILI9341_DrawRoundRect(8, 142, 304, 90, 3, COL_RED);
    ILI9341_DrawBitmap1bpp16(18, 150, bmp_heart, 5, COL_RED);
    sprintf(buf, "%u", bpm);
    ILI9341_DrawString(110, 150, buf, 8, COL_WHITE, PANEL);
    ILI9341_DrawString(206, 160, "BPM", 3, COL_WHITE, PANEL);

    const char *txt; uint16_t col;
    if      (bpm < 60)   { txt = "BRADICARDIA";  col = COL_CYAN;   }
    else if (bpm <= 100) { txt = "NORMAL";       col = COL_GREEN;  }
    else                 { txt = "TAQUICARDIA";  col = COL_YELLOW; }
    ILI9341_FillRect(206, 196, 98, 26, col);
    text_centered(206, 98, 202, txt, (bpm >= 60 && bpm <= 100) ? 2 : 1, COL_BLACK, col);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  ILI9341_Init();
#if   UI_MODE == 1
  Draw_Thermometer(368);
#elif UI_MODE == 2
  Draw_EOG(238, 85);
#elif UI_MODE == 3
  Draw_HeartRate(72);
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if UI_MODE == 0
    Draw_Thermometer(368);   HAL_Delay(4000);
    Draw_EOG(238, 85);       HAL_Delay(4000);
    Draw_HeartRate(72);      HAL_Delay(4000);
#endif
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCDreset_GPIO_Port, LCDreset_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCDdc_GPIO_Port, LCDdc_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCDcs_GPIO_Port, LCDcs_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LCDreset_Pin */
  GPIO_InitStruct.Pin = LCDreset_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCDreset_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCDdc_Pin */
  GPIO_InitStruct.Pin = LCDdc_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCDdc_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCDcs_Pin */
  GPIO_InitStruct.Pin = LCDcs_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCDcs_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */

  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
