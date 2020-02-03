#include "stm32h7xx_hal.h"
#include "wm8731.h"
#include "functions.h"
#include <stdlib.h>
#include "math.h"
#include "trx_manager.h"
#include "settings.h"
#include "lcd.h"

IRAM2 int32_t CODEC_Audio_Buffer_RX[CODEC_AUDIO_BUFFER_SIZE] = {0};
IRAM2 int32_t CODEC_Audio_Buffer_TX[CODEC_AUDIO_BUFFER_SIZE] = {0};
volatile uint32_t WM8731_DMA_samples = 0;
volatile bool WM8731_DMA_state = true; //true - compleate ; false - half
volatile bool WM8731_Buffer_underrun = false;
static bool WM8731_Beeping = false;

static uint8_t WM8731_SendI2CCommand(uint8_t reg, uint8_t value);
HAL_StatusTypeDef HAL_I2S_TXRX_DMA(I2S_HandleTypeDef *hi2s, uint16_t *txData, uint16_t *rxData, uint16_t Size);

void WM8731_start_i2s_and_dma(void)
{
	if (HAL_I2S_GetState(&hi2s3) == HAL_I2S_STATE_READY)
	{
		//HAL_I2S_Receive_DMA(&hi2s3, (uint16_t *)&CODEC_Audio_Buffer_TX[0], CODEC_AUDIO_BUFFER_SIZE);
		//HAL_I2S_Transmit_DMA(&hi2s3, (uint16_t *)&CODEC_Audio_Buffer_RX[0], CODEC_AUDIO_BUFFER_SIZE);
		HAL_I2S_TXRX_DMA(&hi2s3, (uint16_t *)&CODEC_Audio_Buffer_TX[0], (uint16_t *)&CODEC_Audio_Buffer_RX[0], CODEC_AUDIO_BUFFER_SIZE);
	}
}

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
	if (hi2s->Instance == SPI3)
	{
		if (WM8731_Beeping)
			return;
		if (Processor_NeedRXBuffer)
			WM8731_Buffer_underrun = true;
		WM8731_DMA_state = false;
		Processor_NeedRXBuffer = true;
		if (TRX_getMode(CurrentVFO()) == TRX_MODE_LOOPBACK)
			Processor_NeedTXBuffer = true;
		WM8731_DMA_samples += (CODEC_AUDIO_BUFFER_SIZE/2);
	}
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
	if (hi2s->Instance == SPI3)
	{
		if (WM8731_Beeping)
			return;
		if (Processor_NeedRXBuffer)
			WM8731_Buffer_underrun = true;
		WM8731_DMA_state = true;
		Processor_NeedRXBuffer = true;
		if (TRX_getMode(CurrentVFO()) == TRX_MODE_LOOPBACK)
			Processor_NeedTXBuffer = true;
		WM8731_DMA_samples += (CODEC_AUDIO_BUFFER_SIZE/2);
	}
}

void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
	
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
	
}

void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s)
{
	//sendToDebug_strln("I2S Error");
}

void WM8731_CleanBuffer(void)
{
	memset(CODEC_Audio_Buffer_RX, 0x00, sizeof CODEC_Audio_Buffer_RX);
	memset(CODEC_Audio_Buffer_TX, 0x00, sizeof CODEC_Audio_Buffer_TX);
}

void WM8731_Beep(void)
{
	WM8731_Beeping = true;
	for (uint16_t i = 0; i < CODEC_AUDIO_BUFFER_SIZE; i++)
		CODEC_Audio_Buffer_RX[i] = ((float32_t)TRX.Volume / 100.0f) * 2000.0f * arm_sin_f32(((float32_t)i / (float32_t)TRX_SAMPLERATE) * PI * 2.0f * 500.0f);
	HAL_Delay(50);
	WM8731_Beeping = false;
}

static uint8_t WM8731_SendI2CCommand(uint8_t reg, uint8_t value)
{
	uint8_t st = 2;
	uint8_t repeats = 0;
	while (st != 0 && repeats < 3)
	{
		i2c_begin();
		i2c_beginTransmission_u8(B8(0011010)); //I2C_ADDRESS_WM8731 00110100
		i2c_write_u8(reg);					   // MSB
		i2c_write_u8(value);				   // MSB
		st = i2c_endTransmission();
		if (st != 0)
			repeats++;
		HAL_Delay(1);
	}
	return st;
}

void WM8731_TX_mode(void)
{
	FPGA_stop_audio_clock();
	WM8731_SendI2CCommand(B8(00000100), B8(00000000)); //R2 Left Headphone Out
	WM8731_SendI2CCommand(B8(00000110), B8(00000000)); //R3 Right Headphone Out
	WM8731_SendI2CCommand(B8(00001010), B8(00001111)); //R5 Digital Audio Path Control
	if (TRX.InputType_LINE)							   //line
	{
		WM8731_SendI2CCommand(B8(00000000), B8(00010111)); //R0 Left Line In
		WM8731_SendI2CCommand(B8(00000010), B8(00010111)); //R1 Right Line In
		WM8731_SendI2CCommand(B8(00001000), B8(00000010)); //R4 Analogue Audio Path Control
		WM8731_SendI2CCommand(B8(00001100), B8(01101010)); //R6 Power Down Control
	}
	if (TRX.InputType_MIC) //mic
	{
		WM8731_SendI2CCommand(B8(00000001), B8(10000000)); //R0 Left Line In
		WM8731_SendI2CCommand(B8(00000011), B8(10000000)); //R1 Right Line In
		WM8731_SendI2CCommand(B8(00001000), B8(00000101)); //R4 Analogue Audio Path Control
		WM8731_SendI2CCommand(B8(00001100), B8(01101001)); //R6 Power Down Control
	}
	FPGA_start_audio_clock();
}

void WM8731_RX_mode(void)
{
	FPGA_stop_audio_clock();
	WM8731_SendI2CCommand(B8(00000000), B8(10000000)); //R0 Left Line In
	WM8731_SendI2CCommand(B8(00000010), B8(10000000)); //R1 Right Line In
	WM8731_SendI2CCommand(B8(00000100), B8(01111001)); //R2 Left Headphone Out
	WM8731_SendI2CCommand(B8(00000110), B8(01111001)); //R3 Right Headphone Out
	WM8731_SendI2CCommand(B8(00001000), B8(00010110)); //R4 Analogue Audio Path Control
	WM8731_SendI2CCommand(B8(00001010), B8(00000111)); //R5 Digital Audio Path Control
	WM8731_SendI2CCommand(B8(00001100), B8(01100111)); //R6 Power Down Control
	FPGA_start_audio_clock();
}

void WM8731_TXRX_mode(void) //loopback
{
	FPGA_stop_audio_clock();
	WM8731_SendI2CCommand(B8(00000100), B8(01111001)); //R2 Left Headphone Out
	WM8731_SendI2CCommand(B8(00000110), B8(01111001)); //R3 Right Headphone Out
	WM8731_SendI2CCommand(B8(00001010), B8(00000111)); //R5 Digital Audio Path Control
	if (TRX.InputType_LINE)							   //line
	{
		WM8731_SendI2CCommand(B8(00000000), B8(00010111)); //R0 Left Line In
		WM8731_SendI2CCommand(B8(00000010), B8(00010111)); //R1 Right Line In
		WM8731_SendI2CCommand(B8(00001000), B8(00010010)); //R4 Analogue Audio Path Control
		WM8731_SendI2CCommand(B8(00001100), B8(01100010)); //R6 Power Down Control, internal crystal
	}
	if (TRX.InputType_MIC) //mic
	{
		WM8731_SendI2CCommand(B8(00000001), B8(10000000)); //R0 Left Line In
		WM8731_SendI2CCommand(B8(00000011), B8(10000000)); //R1 Right Line In
		WM8731_SendI2CCommand(B8(00001000), B8(00010101)); //R4 Analogue Audio Path Control
		WM8731_SendI2CCommand(B8(00001100), B8(01100001)); //R6 Power Down Control, internal crystal
	}
	FPGA_start_audio_clock();
}

void WM8731_Init(void)
{
	bool err = false;
	FPGA_stop_audio_clock();
	if (WM8731_SendI2CCommand(B8(00011110), B8(00000000)) != 0) //R15 Reset Chip
	{
		sendToDebug_strln("[ERR] Audio codec not found");
		LCD_showError("Audio codec init error", true);
		err = true;
	}
	WM8731_SendI2CCommand(B8(00001110), B8(00000010)); //R7 Digital Audio Interface Format, Codec Slave, I2S Format, MSB-First left-1 justified , 16bits
	WM8731_SendI2CCommand(B8(00010000), B8(00000000)); //R8 Sampling Control normal mode, 256fs, SR=0 (MCLK@12.288Mhz, fs=48kHz))
	WM8731_SendI2CCommand(B8(00010010), B8(00000001)); //R9 reactivate digital audio interface
	WM8731_RX_mode();
	if (!err)
		sendToDebug_strln("[OK] Audio codec inited");
}

static void I2S_DMATxCplt(DMA_HandleTypeDef *hdma)
{
  I2S_HandleTypeDef *hi2s = (I2S_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent;
  HAL_I2S_TxCpltCallback(hi2s);
}

static void I2S_DMATxHalfCplt(DMA_HandleTypeDef *hdma)
{
  I2S_HandleTypeDef *hi2s = (I2S_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent;
  HAL_I2S_TxHalfCpltCallback(hi2s);
}

static void I2S_DMARxCplt(DMA_HandleTypeDef *hdma)
{
  I2S_HandleTypeDef *hi2s = (I2S_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent;
  HAL_I2S_RxCpltCallback(hi2s);
}

static void I2S_DMARxHalfCplt(DMA_HandleTypeDef *hdma)
{
  I2S_HandleTypeDef *hi2s = (I2S_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent;
  HAL_I2S_RxHalfCpltCallback(hi2s);
}

static void I2S_DMAError(DMA_HandleTypeDef *hdma)
{
  I2S_HandleTypeDef *hi2s = (I2S_HandleTypeDef *)((DMA_HandleTypeDef *)hdma)->Parent; /* Derogation MISRAC2012-Rule-11.5 */

  /* Disable Rx and Tx DMA Request */
  CLEAR_BIT(hi2s->Instance->CFG1, (SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN));
  hi2s->TxXferCount = (uint16_t) 0UL;
  hi2s->RxXferCount = (uint16_t) 0UL;

  hi2s->State = HAL_I2S_STATE_READY;

  /* Set the error code and execute error callback*/
  SET_BIT(hi2s->ErrorCode, HAL_I2S_ERROR_DMA);
	
  /* Call user error callback */
  HAL_I2S_ErrorCallback(hi2s);
}

HAL_StatusTypeDef HAL_I2S_TXRX_DMA(I2S_HandleTypeDef *hi2s, uint16_t *txData, uint16_t *rxData, uint16_t Size)
{
  if ((rxData == NULL) || (txData == NULL) || (Size == 0UL))
  {
    return  HAL_ERROR;
  }

  /* Process Locked */
  __HAL_LOCK(hi2s);

  if (hi2s->State != HAL_I2S_STATE_READY)
  {
    __HAL_UNLOCK(hi2s);
    return HAL_BUSY;
  }

  /* Set state and reset error code */
  hi2s->State       = HAL_I2S_STATE_BUSY;
  hi2s->ErrorCode   = HAL_I2S_ERROR_NONE;
  hi2s->pRxBuffPtr  = rxData;
  hi2s->RxXferSize  = Size;
  hi2s->RxXferCount = Size;
	hi2s->pTxBuffPtr  = txData;
  hi2s->TxXferSize  = Size;
  hi2s->TxXferCount = Size;

  hi2s->hdmarx->XferHalfCpltCallback = I2S_DMARxHalfCplt;
  hi2s->hdmarx->XferCpltCallback = I2S_DMARxCplt;
	hi2s->hdmatx->XferHalfCpltCallback = I2S_DMATxHalfCplt;
  hi2s->hdmatx->XferCpltCallback = I2S_DMATxCplt;
  hi2s->hdmatx->XferErrorCallback = I2S_DMAError;

  /* Enable the Rx DMA Stream/Channel */
  if (HAL_OK != HAL_DMA_Start_IT(hi2s->hdmarx, (uint32_t)&hi2s->Instance->RXDR, (uint32_t)hi2s->pRxBuffPtr, hi2s->RxXferSize))
  {
    /* Update SPI error code */
    SET_BIT(hi2s->ErrorCode, HAL_I2S_ERROR_DMA);
    hi2s->State = HAL_I2S_STATE_READY;

    __HAL_UNLOCK(hi2s);
    return HAL_ERROR;
  }
	if (HAL_OK != HAL_DMA_Start_IT(hi2s->hdmatx, (uint32_t)hi2s->pTxBuffPtr, (uint32_t)&hi2s->Instance->TXDR, hi2s->TxXferSize))
  {
    /* Update SPI error code */
    SET_BIT(hi2s->ErrorCode, HAL_I2S_ERROR_DMA);
    hi2s->State = HAL_I2S_STATE_READY;

    __HAL_UNLOCK(hi2s);
    return HAL_ERROR;
  }

  /* Check if the I2S Rx request is already enabled */
  if (HAL_IS_BIT_CLR(hi2s->Instance->CFG1, SPI_CFG1_RXDMAEN))
  {
    /* Enable Rx DMA Request */
    SET_BIT(hi2s->Instance->CFG1, SPI_CFG1_RXDMAEN);
  }
	/* Check if the I2S Tx request is already enabled */
  if (HAL_IS_BIT_CLR(hi2s->Instance->CFG1, SPI_CFG1_TXDMAEN))
  {
    /* Enable Tx DMA Request */
    SET_BIT(hi2s->Instance->CFG1, SPI_CFG1_TXDMAEN);
  }

  /* Check if the I2S is already enabled */
  if (HAL_IS_BIT_CLR(hi2s->Instance->CR1, SPI_CR1_SPE))
  {
    /* Enable I2S peripheral */
    __HAL_I2S_ENABLE(hi2s);
  }

  /* Start the transfer */
  SET_BIT(hi2s->Instance->CR1, SPI_CR1_CSTART);

  __HAL_UNLOCK(hi2s);
  return HAL_OK;
}
