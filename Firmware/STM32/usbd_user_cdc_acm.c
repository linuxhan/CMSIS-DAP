#include <RTL.h>
#include <rl_usb.h>

#define  __NO_USB_LIB_C
#include "usb_config.c"

#if (USBD_CDC_ACM_ENABLE == 1)

#include "DAP_Config.h"
#include "usbd_user_cdc_acm.h"

static struct {
				uint8_t  data[USART_BUFFER_SIZE];
	volatile	uint16_t idx_in;
	volatile	uint16_t idx_out;
	volatile	int16_t cnt_in;
	volatile	int16_t cnt_out;
} WrBuffer, RdBuffer;

static USART_InitTypeDef UART_Config;
static uint32_t StatusRegister;
static uint32_t SendBreakFlag;
static uint32_t BreakFlag;
static CDC_LINE_CODING line_coding_current;

/*------------------------------------------------------------------------------
 * UART_Reset:  Reset the Serial module variables
 *----------------------------------------------------------------------------*/

int32_t UART_Reset (void)
{
	uint8_t *ptr;
	int32_t  i;

	NVIC_DisableIRQ(USART_IRQn);	/* Disable USART interrupt */

	SendBreakFlag    = 0;
	BreakFlag        = 0;

	line_coding_current.bCharFormat = UART_STOP_BITS_1;
	line_coding_current.bDataBits = UART_DATA_BITS_8;
	line_coding_current.bParityType = UART_PARITY_NONE;
	line_coding_current.dwDTERate = 9600;
	USBD_CDC_ACM_PortSetLineCoding(&line_coding_current);

	ptr = (uint8_t *)&WrBuffer;
	for (i = 0; i < sizeof(WrBuffer); i++) *ptr++ = 0;
	ptr = (uint8_t *)&RdBuffer;
	for (i = 0; i < sizeof(RdBuffer); i++) *ptr++ = 0;

	NVIC_EnableIRQ(USART_IRQn);		/* Enable USART interrupt */
	return (1);
}

/** \brief  Vitual COM Port initialization

    The function inititalizes the hardware resources of the port used as 
    the Virtual COM Port.

    \return             0        Function failed.
    \return             1        Function succeeded.
 */

int32_t USBD_CDC_ACM_PortInitialize (void)
{
	const GPIO_InitTypeDef rx_init		= {	USART_RX_PIN,	GPIO_Speed_50MHz,	GPIO_Mode_IN_FLOATING	};
	const GPIO_InitTypeDef tx_init		= {	USART_TX_PIN,	GPIO_Speed_50MHz,	GPIO_Mode_AF_PP			};

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

#if   defined ( USART_GPIO_CLK2 )
	RCC_APB2PeriphClockCmd(USART_GPIO_CLK2, ENABLE);
#elif defined ( USART_GPIO_CLK1 )
	RCC_APB1PeriphClockCmd(USART_GPIO_CLK1, ENABLE);
#else
	#error "USART GPIO Clock not define"
#endif

#if   defined ( USART_REMAP )
	GPIO_PinRemapConfig(USART_REMAP, ENABLE);
#endif

	GPIO_INIT(USART_GPIO, rx_init);
	GPIO_INIT(USART_GPIO, tx_init);

#if   defined ( USART_CLK2 )
	RCC_APB2PeriphClockCmd(USART_CLK2, ENABLE); 
#elif defined ( USART_CLK1 )
	RCC_APB1PeriphClockCmd(USART_CLK1, ENABLE);
#else
	#error "USART Clock not define"
#endif

	UART_Reset();
	return (1);
}

/** \brief  Vitual COM Port uninitialization

    The function uninititalizes/releases the hardware resources of the port used 
    as the Virtual COM Port.

    \return             0        Function failed.
    \return             1        Function succeeded.
 */
int32_t USBD_CDC_ACM_PortUninitialize (void)
{ 
	const GPIO_InitTypeDef rx_deinit	= {	USART_RX_PIN,	GPIO_Speed_50MHz,	GPIO_Mode_IN_FLOATING	};
	const GPIO_InitTypeDef tx_deinit	= {	USART_TX_PIN,	GPIO_Speed_50MHz,	GPIO_Mode_IN_FLOATING	};

#if   defined ( USART_CLK2 )
	RCC_APB2PeriphClockCmd(USART_CLK2, DISABLE); 
#elif defined ( USART_CLK1 )
	RCC_APB1PeriphClockCmd(USART_CLK1, DISABLE);
#else
	#error "USART Clock not define"
#endif

	GPIO_INIT(USART_GPIO, rx_deinit);
	GPIO_INIT(USART_GPIO, tx_deinit);
	return (1);
}

/** \brief  Vitual COM Port reset

    The function resets the internal states of the port used 
    as the Virtual COM Port.

    \return             0        Function failed.
    \return             1        Function succeeded.
 */
int32_t USBD_CDC_ACM_PortReset (void)
{
	return (UART_Reset());
}

/** \brief  Virtual COM Port change communication settings

    The function changes communication settings of the port used as the 
    Virtual COM Port.

    \param [in]         line_coding  Pointer to the loaded CDC_LINE_CODING structure.
    \return             0        Function failed.
    \return             1        Function succeeded.
 */
int32_t USBD_CDC_ACM_PortSetLineCoding (CDC_LINE_CODING *line_coding)
{
	USART_InitTypeDef * config = &UART_Config;
	
	/*	USART supports:
		8bit data; even, odd and none parity; 1, 1.5 and 2 stop bits
	*/
  
	/* Data bits */
	if (line_coding->bDataBits != UART_DATA_BITS_8) return(0);
	config->USART_WordLength	= USART_WordLength_8b;

	/* Parity */
	switch (line_coding->bParityType)
	{
		case UART_PARITY_NONE:		config->USART_Parity = USART_Parity_No;		break;
		case UART_PARITY_EVEN:		config->USART_Parity = USART_Parity_Even;	break;
		case UART_PARITY_ODD:		config->USART_Parity = USART_Parity_Odd;	break;
		default: return (0);
	}

	/* Stop bits */
	switch (line_coding->bCharFormat)
	{
		case UART_STOP_BITS_1:		config->USART_StopBits = USART_StopBits_1;		break;
		case UART_STOP_BITS_2:		config->USART_StopBits = USART_StopBits_2;		break;
		case UART_STOP_BITS_1_5:	config->USART_StopBits = USART_StopBits_1_5;	break;
		default: return (0);
	}

	line_coding_current.bCharFormat = line_coding->bCharFormat;
	line_coding_current.bDataBits = line_coding->bDataBits;
	line_coding_current.bParityType = line_coding->bParityType;
	line_coding_current.dwDTERate = line_coding->dwDTERate;
	
	config->USART_BaudRate		= line_coding->dwDTERate;
	config->USART_Mode			= USART_Mode_Rx | USART_Mode_Tx;
	config->USART_HardwareFlowControl	= USART_HardwareFlowControl_None;
	
	USART_Init(USART_PORT, config);
	USART_ITConfig(USART_PORT, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART_PORT, ENABLE);
	return (1);
}

/** \brief  Vitual COM Port retrieve communication settings

    The function retrieves communication settings of the port used as the 
    Virtual COM Port.

    \param [in]         line_coding  Pointer to the CDC_LINE_CODING structure.
    \return             0        Function failed.
    \return             1        Function succeeded.
 */

int32_t USBD_CDC_ACM_PortGetLineCoding (CDC_LINE_CODING *line_coding)
{
	line_coding->bCharFormat = line_coding_current.bCharFormat;
	line_coding->bDataBits = line_coding_current.bDataBits;
	line_coding->bParityType = line_coding_current.bParityType;
	line_coding->dwDTERate = line_coding_current.dwDTERate;
	return (1);
}

/** \brief  Virtual COM Port set control line state

    The function sets control line state on the port used as the
    Virtual COM Port.

    \param [in]         ctrl_bmp Control line settings bitmap (
                          0. bit - DTR state, 
                          1. bit - RTS state).
    \return             0        Function failed.
    \return             1        Function succeeded.
 */

int32_t USBD_CDC_ACM_PortSetControlLineState (uint16_t ctrl_bmp)
{
	return (1);
}

/*------------------------------------------------------------------------------
 * UART_WriteData:     Write data from buffer
 *----------------------------------------------------------------------------*/
int32_t UART_WriteData (uint8_t *data, uint16_t size)
{
	uint32_t cnt = 0;
	int16_t  len_in_buf;

	if (size == 0) return (0);

	while (size--)
	{
		len_in_buf = WrBuffer.cnt_in - WrBuffer.cnt_out;
		if (len_in_buf < USART_BUFFER_SIZE)
		{
			WrBuffer.data[WrBuffer.idx_in++] = *data++;
			WrBuffer.idx_in &= (USART_BUFFER_SIZE - 1);
			WrBuffer.cnt_in++;
			cnt++;
		}
	}
	USART_ITConfig(USART_PORT, USART_IT_TXE, ENABLE);
	return (cnt);
}


/*------------------------------------------------------------------------------
 * UART_ReadData:     Read data to buffer
 *----------------------------------------------------------------------------*/

int32_t UART_ReadData (uint8_t *data, uint16_t size)
{
	uint32_t cnt = 0;
	if (size == 0) return (0);
  
	while (size--)
	{
		if (RdBuffer.cnt_in != RdBuffer.cnt_out)
		{
			*data++ = RdBuffer.data[RdBuffer.idx_out++];
			RdBuffer.idx_out &= (USART_BUFFER_SIZE - 1);
			RdBuffer.cnt_out++;
			cnt++;
		}
	}
	return (cnt);
}

/*------------------------------------------------------------------------------
 * UART_GetChar:     Read a received character
 *----------------------------------------------------------------------------*/
int32_t UART_GetChar (void)
{
	uint8_t ch;
	if ((UART_ReadData (&ch, 1)) == 1) return ((int32_t)ch);
	else return (-1);
}

/*------------------------------------------------------------------------------
 * UART_PutChar:     Write a character
 *----------------------------------------------------------------------------*/
int32_t UART_PutChar (uint8_t ch)
{
	if ((UART_WriteData (&ch, 1)) == 1) return ((uint32_t) ch);
	else return (-1);
}

/*------------------------------------------------------------------------------
 * UART_DataAvailable: returns number of bytes available in RdBuffer
 *----------------------------------------------------------------------------*/
int32_t UART_DataAvailable (void)
{
	return (RdBuffer.cnt_in - RdBuffer.cnt_out);
}

/*------------------------------------------------------------------------------
 * UART_GetStatusLineState:   Get state of status lines
 *----------------------------------------------------------------------------*/

int32_t UART_GetStatusLineState (void)
{
	return (0);
}

/*------------------------------------------------------------------------------
 * UART_GetCommunicationErrorStatus:  Get error status
 *----------------------------------------------------------------------------*/

int32_t UART_GetCommunicationErrorStatus (void)
{
	int32_t err = 0;

	if (StatusRegister & USART_SR_PE)
		err |= UART_PARITY_ERROR_Msk;
	if (StatusRegister & USART_SR_NE)
		err |= UART_OVERRUN_ERROR_Msk;
	if (BreakFlag == 0 && (StatusRegister & USART_SR_FE))
		err |= UART_PARITY_ERROR_Msk;
	return (err);
}

/*------------------------------------------------------------------------------
 * UART_SetBreak :  set break condition
 *----------------------------------------------------------------------------*/
int32_t UART_SetBreak (void)
{
	SendBreakFlag = 1;				/* Set send break flag and	*/
	USART_SendBreak(USART_PORT);	/* send 1 break character	*/
	return (1);
}

/*------------------------------------------------------------------------------
 * UART_GetBreak:   if break character received, return 1
 *----------------------------------------------------------------------------*/

int32_t UART_GetBreak (void)
{
	return (BreakFlag);
}

/*------------------------------------------------------------------------------
 * UART_IRQ:        Serial interrupt handler routine
 *----------------------------------------------------------------------------*/

void USART_IRQHandler(void)
{
	uint8_t  ch;
	int16_t  len_in_buf;

	StatusRegister = USART_PORT->SR;

	/* Transmit data register empty interrupt */
	if (USART_GetITStatus(USART_PORT, USART_IT_TXE) != RESET)
	{
		if (SendBreakFlag == 1)
		{
			USART_SendBreak(USART_PORT);
		}
		else if (WrBuffer.cnt_in != WrBuffer.cnt_out)
		{
			USART_SendData(USART_PORT, WrBuffer.data[WrBuffer.idx_out++]);
			WrBuffer.idx_out &= (USART_BUFFER_SIZE - 1);
			WrBuffer.cnt_out++;
		}
		else
		{
			USART_ITConfig(USART_PORT, USART_IT_TXE, DISABLE);
		}
	}

	/* Read data register not empty interrupt */
	if(USART_GetITStatus(USART_PORT, USART_IT_RXNE) != RESET)
	{
		len_in_buf = RdBuffer.cnt_in - RdBuffer.cnt_out;
		if (len_in_buf < USART_BUFFER_SIZE)
		{
			ch = (uint8_t)USART_ReceiveData(USART_PORT);
			RdBuffer.data[RdBuffer.idx_in++] = ch;
			if ((ch == 0)
			&&	USART_GetFlagStatus(USART_PORT, USART_SR_FE)	/* framing error */
				)
			{	/* break character */
				BreakFlag = 1;
			}
			else
				BreakFlag = 0;
			RdBuffer.idx_in &= (USART_BUFFER_SIZE - 1);
			RdBuffer.cnt_in++;
		}
	}
}

#endif