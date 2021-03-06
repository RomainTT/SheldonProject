/**
	* @file sampleAcquisition.c
	* @brief ADC acquisition of an ultrasound sample
	*
	*     This file contains the service for acquire sample from the ADC.
	*			Initialization, configuration and start. The status LED turn on at each acquisition.
	*			
	*
	* @author Romain TAPREST
	* @date 09 jan 2016
	*/


/** \page Clocks configuration
	* \section ADC sampling frequency
	* 
	* \li APB2 Clock = 72MHz
	* \li ADC prescaler = 6
	* \li ==> ADC clock = 12MHz
	* \li 1.5 cycles + 12.5 = 14 cycles per conversion ( \sea Reference Manual p224)
	* \li Max Sampling frequency = 12MHz/15 = 857 kHz
	* \li 8 channels ==> Max Sampling frequency for each channel = 107 kHz
	* \li Current config : sampling frequency = 100 kHz on Timer trigger
	*/

/******************************************************************************
	* 
	*   INCLUDED FILES
	*
	*****************************************************************************/
	
	
#include "typesAndConstants.h"
#include "misc.h"
#include "stm32f10x.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_dma.h"
#include "sampleAcquisition.h"
#include "signalProcessing.h"


/******************************************************************************
	* 
	*   VARIABLES
	*
	*****************************************************************************/
extern t_signalsData g_signalData;

#define SIGNAL_BUFFER_SIZE	NB_OF_SIGNALS
uint16_t adcBuffer[SIGNAL_BUFFER_SIZE];

/******************************************************************************
	*
	*   PRIVATE FUNCTIONS
	*
	*****************************************************************************/
	
	
/**
  * @brief  Configures the different system clocks.
  * @param  None
  * @retval None
  */
void RCC_Configuration(void)
{
	// Defines the ADC clock divider, this clock is derived from the APB2 clock (PCLK2)
	RCC_ADCCLKConfig( RCC_PCLK2_Div6 );
	// Enable DMA1 clock
	RCC_AHBPeriphClockCmd( RCC_AHBPeriph_DMA1 , ENABLE );
	// Enable GPIOC, ADC1 and TIM1 clock
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_ADC1 | RCC_APB2Periph_TIM1 , ENABLE );
}

/**
  * @brief  Configures the different GPIO ports.
  * @param  None
  * @retval None
  */
void GPIO_Configuration(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit( &GPIO_InitStructure );
	
	// Configure TIM1_CH1 (PA8) as alternate function push-pull
  GPIO_InitStructure.GPIO_Pin = 	GPIO_Pin_8;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = 	GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

	// Configure PB.00 (ADC1 Channel 8) as analog input
  GPIO_InitStructure.GPIO_Pin = 	GPIO_Pin_0;
	// Configure PB.01 (ADC1 Channel 9) as analog input
  GPIO_InitStructure.GPIO_Pin |= 	GPIO_Pin_1;
  
	GPIO_InitStructure.GPIO_Mode = 	GPIO_Mode_AIN;	
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
  // Configure PC.00 (ADC1 Channel 10) as analog input
  GPIO_InitStructure.GPIO_Pin = 	GPIO_Pin_0;
	// Configure PC.01 (ADC1 Channel 11) as analog input
  GPIO_InitStructure.GPIO_Pin |= 	GPIO_Pin_1;
	// Configure PC.02 (ADC1 Channel 12) as analog input
  GPIO_InitStructure.GPIO_Pin |= 	GPIO_Pin_2;
	// Configure PC.03 (ADC1 Channel 13) as analog input
  GPIO_InitStructure.GPIO_Pin |= 	GPIO_Pin_3;
	// Configure PC.04 (ADC1 Channel 14) as analog input
  GPIO_InitStructure.GPIO_Pin |= 	GPIO_Pin_4;
	// Configure PC.05 (ADC1 Channel 15) as analog input
  GPIO_InitStructure.GPIO_Pin |= 	GPIO_Pin_5;
	
	GPIO_InitStructure.GPIO_Mode = 	GPIO_Mode_AIN;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
	
	// LED on the board (A.01)
	GPIO_InitStructure.GPIO_Pin = 	GPIO_Pin_1;
  GPIO_InitStructure.GPIO_Mode = 	GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
  * @brief  Configures the DMA.
  * @param  None
  * @retval None
  */
void DMA_Configuration(void)
{
	DMA_InitTypeDef DMA_InitStructure; // Structure to initialize the DMA

	// Configure DMA1 on channel 1
	DMA_InitStructure.DMA_PeripheralBaseAddr = 	ADC1_DR_Address;//ADC1_DR_Address; // Address of peripheral the DMA must map to
	DMA_InitStructure.DMA_MemoryBaseAddr = 			(uint32_t) &adcBuffer; // Variable to which ADC values will be stored
	DMA_InitStructure.DMA_DIR = 								DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = 					SIGNAL_BUFFER_SIZE; // In data unit (see periph and memory data size fields)
	DMA_InitStructure.DMA_PeripheralInc = 			DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = 					DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = 	DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = 			DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = 								DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = 						DMA_Priority_High;
	DMA_InitStructure.DMA_M2M = 								DMA_M2M_Disable; // Memory to memory
	
	DMA_DeInit( DMA1_Channel1 ); //Set DMA registers to default values
	DMA_Init( DMA1_Channel1, &DMA_InitStructure );
	
	// DMA IT
	DMA_ITConfig( DMA1_Channel1, DMA_IT_TC, ENABLE ); // Transfer complete interrupt
	DMA_ClearITPendingBit( DMA1_IT_TC1 );

	// enable DMA1
	DMA_Cmd( DMA1_Channel1, ENABLE );
}


/**
  * @brief  Configures the ADC.
  * @param  None
  * @retval None
  */
void ADC_Configuration(void)
{
	ADC_InitTypeDef ADC_InitStructure; // Structure to initialize the ADC

	// Common config
	ADC_InitStructure.ADC_Mode = 								ADC_Mode_Independent;
	ADC_InitStructure.ADC_ScanConvMode = 				ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = 	DISABLE; // Conversion on PWM rising edge only
	ADC_InitStructure.ADC_ExternalTrigConv = 		ADC_ExternalTrigConv_T1_CC1; // Timer 1 CC1
	ADC_InitStructure.ADC_DataAlign = 					ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfChannel = 				8;

	ADC_DeInit( ADC1 ); //Set ADC registers to default values
	ADC_Init( ADC1, &ADC_InitStructure );
	
	// Channels config
	// Refer to SignalsRouting.png for the ranks
	ADC_RegularChannelConfig( ADC1, ADC_Channel_8, 2, ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig( ADC1, ADC_Channel_9, 1, ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig( ADC1, ADC_Channel_10, 8, ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig( ADC1, ADC_Channel_11, 7, ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig( ADC1, ADC_Channel_12, 3, ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig( ADC1, ADC_Channel_13, 4, ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig( ADC1, ADC_Channel_14, 6, ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig( ADC1, ADC_Channel_15, 5, ADC_SampleTime_1Cycles5);
	
	// Enable End Of Conversion interrupt
  ADC_ITConfig(ADC1, ADC_IT_EOC, ENABLE);
	
	// Start transferts
  ADC_ExternalTrigConvCmd( ADC1, ENABLE ); // Enable ADC1 external trigger
	ADC_DMACmd( ADC1, ENABLE ); //Enable ADC1 DMA
	ADC_Cmd( ADC1, ENABLE ); //Enable ADC1

	// Calibrate ADC1
	ADC_ResetCalibration( ADC1 );
	while ( ADC_GetResetCalibrationStatus(ADC1) ) {} //Check the end of ADC1 reset calibration register
	ADC_StartCalibration( ADC1 );
	while ( ADC_GetCalibrationStatus(ADC1) ) {} //Check the end of ADC1 calibration
}

/**
  * @brief  Configures the Timer.
  * @param  None
  * @retval None
  */
void TIMER_Configuration(void)
{
	TIM_TimeBaseInitTypeDef   TIM_TimeBaseStructure; // Timer base configuration
	TIM_OCInitTypeDef         TIM_OCInitStructure; // Timer for PWM mode
	
	// Time Base configuration
  TIM_TimeBaseStructInit( &TIM_TimeBaseStructure ); 
  TIM_TimeBaseStructure.TIM_Period = 				720;  // 72MHz / 720 = 100kHz  
  TIM_TimeBaseStructure.TIM_Prescaler = 		0x0;       
  TIM_TimeBaseStructure.TIM_ClockDivision = 0x0;    
  TIM_TimeBaseStructure.TIM_CounterMode = 	TIM_CounterMode_Down;  
  TIM_TimeBaseInit( TIM1, &TIM_TimeBaseStructure );
	
  // TIM1 channel1 configuration in PWM mode
	TIM_OCStructInit( &TIM_OCInitStructure );
  TIM_OCInitStructure.TIM_OCMode = 			TIM_OCMode_PWM1; 
  TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;                
  TIM_OCInitStructure.TIM_OCPolarity = 	TIM_OCPolarity_High;         
  TIM_OC1Init( TIM1, &TIM_OCInitStructure );
	
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);
	TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
	
	// TIM1 counter enable
  TIM_Cmd( TIM1, ENABLE );
  // TIM1 main Output Enable
  TIM_CtrlPWMOutputs( TIM1, ENABLE );
}
	
/**
  * @brief  Configures the ITs.
  * @param  None
  * @retval None
  */
void IT_Configuration(void)
{
  NVIC_InitTypeDef NVIC_InitStructure; // IT

	// Configure and enable TIM1 interrupt
  NVIC_InitStructure.NVIC_IRQChannel = 										TIM1_UP_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 	4;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 				2;
  NVIC_InitStructure.NVIC_IRQChannelCmd = 								ENABLE;
  NVIC_Init( &NVIC_InitStructure );
	
	// Enable the DMA global Interrupt
	NVIC_InitStructure.NVIC_IRQChannel = 										DMA1_Channel1_IRQn; 
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 	3;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 				2;
	NVIC_InitStructure.NVIC_IRQChannelCmd = 								ENABLE;
	NVIC_Init( &NVIC_InitStructure );
	
}

/******************************************************************************
	*
	*   PUBLIC FUNCTIONS
	*
	*****************************************************************************/


/**
 * @brief Interrupt handler of DCMI DMA stream
 */
void DMA1_Channel1_IRQHandler( void )
{
	if ( DMA_GetITStatus( DMA1_IT_TC1 ) != RESET ) // Full buffer
	{
		DMA_ClearITPendingBit( DMA1_IT_TC1 );
		sProcUpdateSignalMaximum(adcBuffer);
	}
}




void TIM1_UP_IRQHandler(void)
{
	(g_signalData.numberOfSamples)++;
	TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
}

/**
	* @brief	Init the sampling routine.
	*					Blocking function.
	* 			
	* @param Void
	*/
void sampleAcquisitionInit( void )
{
	/*****************
	 * SYSTEM CLOCKS *
	 *****************/
	
	RCC_Configuration();
	
	/********
	 * GPIO *
	 ********/

	GPIO_Configuration();
	
	/*******
	 * DMA *
	 *******/

	DMA_Configuration();
	
	/*******
	 * ADC *
	 *******/
	
	ADC_Configuration();

	/*********
	 * TIMER *
	 *********/
	
	TIMER_Configuration();

	/********
	 * NVIC *
	 ********/
	
	IT_Configuration();

	/*********
	 * START *
	 *********/

}

