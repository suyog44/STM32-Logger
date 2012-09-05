/******************** (C) COPYRIGHT 2011 STMicroelectronics ********************
* File Name          : memory.c
* Author             : MCD Application Team
* Version            : V3.3.0
* Date               : 21-March-2011
* Description        : Memory management layer
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/

#include "memory.h"
#include "usb_scsi.h"
#include "usb_bot.h"
#include "usb_regs.h"
#include "usb_mem.h"
#include "usb_conf.h"
#include "hw_config.h"
#include "mass_mal.h"
#include "usb_lib.h"
#include "stm32f10x.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
__IO uint32_t Block_Read_count = 0;
__IO uint32_t Block_offset;
__IO uint32_t Counter = 0;
uint32_t  Idx;
//uint32_t Data_Buffer[MAX_DMA_BUFF_SIZE/4]; /*convert to bytes - make this as large as possible*/ /*** extern from mass_mal*/
uint8_t TransferState = TXFR_IDLE;
/* Extern variables ----------------------------------------------------------*/
extern uint8_t Bulk_Data_Buff[BULK_MAX_PACKET_SIZE];  /* data buffer*/
extern uint16_t Data_Len;
extern uint8_t Bot_State;
extern Bulk_Only_CBW CBW;
extern Bulk_Only_CSW CSW;
extern uint32_t Mass_Memory_Size[2];
extern uint32_t Mass_Block_Size[2];

/* Private function prototypes -----------------------------------------------*/
/* Extern function prototypes ------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
* Function Name  : Read_Memory
* Description    : Handle the Read operation from the microSD card.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Read_Memory(uint8_t lun, uint32_t Memory_Offset, uint32_t Transfer_Length)
{
	static uint32_t Length,Transfer_counter;
	if (TransferState == TXFR_IDLE ){
		Length = Transfer_Length * Mass_Block_Size[lun];
		Transfer_counter = Memory_Offset * Mass_Block_Size[lun];
		TransferState = TXFR_ONGOING;
		while(Sd_Spi_Called_From_USB_MSC){;}
		Sd_Spi_Called_From_USB_MSC = 1;			//Set this to stop the SD card driver blocking
		while(MAL_Read(lun, Transfer_counter, (volatile uint8_t *)Data_Buffer,512)){//Read and discard CRC
			release_spi();
			MAL_Init(0);
		}
		Block_offset=0;
	}
	if (TransferState == TXFR_ONGOING ){
		while(MAL_TRANSFER_INDEX>(512-Block_offset-BULK_MAX_PACKET_SIZE)){;}//Wait for enough to be transferred
		USB_SIL_Write(EP1_IN, (volatile uint8_t *)Data_Buffer + Block_offset, BULK_MAX_PACKET_SIZE);	
		SetEPTxCount(ENDP1, BULK_MAX_PACKET_SIZE);
		#ifndef USE_STM3210C_EVAL
		SetEPTxStatus(ENDP1, EP_TX_VALID);
		#endif
		CSW.dDataResidue -= BULK_MAX_PACKET_SIZE;
		Block_offset += BULK_MAX_PACKET_SIZE; 
		if(512 == Block_offset) {			//If we have finished the DMA transfer 
			if(Length>BULK_MAX_PACKET_SIZE)	{	//Data remains
				while(Sd_Spi_Called_From_USB_MSC){;}//Wait for ready - shouldnt happen
				Sd_Spi_Called_From_USB_MSC=1;
				while(MAL_Read(lun,Transfer_counter+BULK_MAX_PACKET_SIZE,(volatile uint8_t*)Data_Buffer,512)){//Try and recover from error
					release_spi();
					MAL_Init(0);
				}
			}
			Block_offset=0;				//Reset this here
		}
		Length -= BULK_MAX_PACKET_SIZE;
		Transfer_counter += BULK_MAX_PACKET_SIZE;
		Led_RW_ON();
	}				
	if (Length == 0){
		Block_offset = 0;
		Transfer_counter=0;
		Bot_State = BOT_DATA_IN_LAST;
		TransferState = TXFR_IDLE;
		Led_RW_OFF();
	}
}

/*******************************************************************************
* Function Name  : Write_Memory
* Description    : Handle the Write operation to the microSD card.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Write_Memory (uint8_t lun, uint32_t Memory_Offset, uint32_t Transfer_Length)
{

  static uint32_t W_Offset, W_Length;

  uint32_t temp =  Counter + 64;

  if (TransferState == TXFR_IDLE )
  {
    W_Offset = Memory_Offset * Mass_Block_Size[lun];
    W_Length = Transfer_Length * Mass_Block_Size[lun];
    TransferState = TXFR_ONGOING;
  }

  if (TransferState == TXFR_ONGOING )
  {
    while(Sd_Spi_Called_From_USB_MSC){while(1){;}}
    for (Idx = 0 ; Counter < temp; Counter++)
    {
      *((uint8_t *)Data_Buffer + Counter ) = Bulk_Data_Buff[Idx++];
    }

    W_Offset += Data_Len;
    W_Length -= Data_Len;

    if (Counter>=MAX_DMA_BUFF_SIZE || !W_Length)
    {
     MAL_Write(lun ,
                W_Offset - Mass_Block_Size[lun],
                Data_Buffer,
                Counter);
     Counter=0;
    }

    CSW.dDataResidue -= Data_Len;
  #ifndef STM32F10X_CL
    SetEPRxStatus(ENDP2, EP_RX_VALID); /* enable the next transaction*/   
  #endif /* STM32F10X_CL */

    Led_RW_ON();
  }

  if ((W_Length == 0) || (Bot_State == BOT_CSW_Send))
  {
    Counter = 0;
    Set_CSW (CSW_CMD_PASSED, SEND_CSW_ENABLE);
    TransferState = TXFR_IDLE;
    Led_RW_OFF();
  }
}
/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/

