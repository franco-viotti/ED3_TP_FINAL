/*
===============================================================================
 Name        : TP_Final.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#ifdef __USE_CMSIS
#include <lpc17xx.h>
#include <lpc17xx_adc.h>
#include <lpc17xx_dac.h>
#include <lpc17xx_gpdma.h>
#include <lpc17xx_pinsel.h>
#include <lpc17xx_timer.h>
#include <lpc17xx_exti.h>
#include <lpc17xx_gpio.h>
#include <lpc17xx_uart.h>
#include <lpc17xx_clkpwr.h>
#endif


void configADC();
void configPins();
void configEXTI();
void configTimer();
void calibrate();
void check_interval(int);
void configDMA_DAC(); 		//configuracion  LLI DMA
void configDAC(); 			//configuracion DAC para sonido
void startAlarm(int); 		//comienza transmision M2P
void configUART();
void configDMA_UART();
//void sendMessage(uint8_t*);
void sendMessage();
void clearMessage();
void configUARTTimer();
//void delay();


int sample_number = 0;
int calibration_on = 0;
int first_value = 0;
int calib_flag = 0;
uint16_t adc_values[10];
int prom = 0;
int check = 0;
uint8_t message0[7];

/----------------------------DMA VARIABLES----------------------------/
GPDMA_Channel_CFG_Type GPDMACfg_DAC;
GPDMA_LLI_Type DMA_LLI_Struct_DAC;
GPDMA_LLI_Type DMA_LLI_Struct_UART;
GPDMA_Channel_CFG_Type GPDMACfg_UART;


uint32_t signal[60]={32768, 36160, 39552, 42880, 46080, 49152, 51968, 54656, 57088, 59264
, 61120, 62656, 63872, 64768, 65344, 65472, 65344, 64768, 63872, 62656, 61120, 59264
, 57088, 54656, 51968, 49152, 46080, 42880, 39552, 36160, 32768, 29376, 25984, 22656
, 19456, 16384, 13568, 10880, 8448, 6272, 4416, 2880, 1664, 768, 192, 0, 192, 768, 1664
, 2880, 4416, 6272, 8448, 10880, 13568, 16384, 19456, 22656, 25984, 29376};

int main(void) {
	configPins();
	GPDMA_Init();
	configADC();
	configEXTI();
	configTimer();
	configDAC();
	configUART();

	while(1);

    return 0;
}


void configPins(){
	PINSEL_CFG_Type ad_pin; //adc pin
	ad_pin.Portnum = 0;
	ad_pin.Pinnum = 24;
	ad_pin.Pinmode = PINSEL_PINMODE_TRISTATE;
	ad_pin.Funcnum = 1;
	ad_pin.OpenDrain = PINSEL_PINMODE_NORMAL;
	PINSEL_ConfigPin(&ad_pin);

	PINSEL_CFG_Type dac_pin; //dac pin
	dac_pin.Portnum = 0;
	dac_pin.Pinnum = 26;
	dac_pin.Pinmode = PINSEL_PINMODE_TRISTATE;
	dac_pin.Funcnum = 2;
	dac_pin.OpenDrain = PINSEL_PINMODE_NORMAL;
	PINSEL_ConfigPin(&dac_pin);

	PINSEL_CFG_Type calib_pin; //calibration pin
	calib_pin.Portnum = 2;
	calib_pin.Pinnum = 10;
	calib_pin.Pinmode = PINSEL_PINMODE_PULLUP;
	calib_pin.Funcnum = 1;
	calib_pin.OpenDrain = PINSEL_PINMODE_NORMAL;
	PINSEL_ConfigPin(&calib_pin);

	PINSEL_CFG_Type leds;
	leds.Portnum = 2;
	leds.Pinmode = PINSEL_PINMODE_TRISTATE;
	leds.OpenDrain = PINSEL_PINMODE_NORMAL;
	leds.Funcnum = 0;
	for (int i = 0; i < 6 ; i++)
	{
		leds.Pinnum = i;
		PINSEL_ConfigPin(&leds);
	}

	GPIO_SetDir(2, 0x3F, 1);

	PINSEL_CFG_Type uart_pin;
	uart_pin.Portnum = 0;
	uart_pin.Pinnum = 2;
	uart_pin.Funcnum = 1;
	uart_pin.OpenDrain = PINSEL_PINMODE_NORMAL;
	uart_pin.Pinmode = PINSEL_PINMODE_PULLUP;
	PINSEL_ConfigPin(&uart_pin);
	uart_pin.Pinnum = 3;
	PINSEL_ConfigPin(&uart_pin);
}

void configADC(){
	ADC_Init(LPC_ADC, 200000);
	ADC_ChannelCmd(LPC_ADC, ADC_CHANNEL_1, ENABLE);
	ADC_IntConfig(LPC_ADC, ADC_ADINTEN1, ENABLE);

	NVIC_EnableIRQ(ADC_IRQn);
}

void configEXTI(){
	EXTI_InitTypeDef exti_init;
	exti_init.EXTI_Line = 0;
	exti_init.EXTI_Mode = EXTI_MODE_EDGE_SENSITIVE;
	exti_init.EXTI_polarity = EXTI_POLARITY_LOW_ACTIVE_OR_FALLING_EDGE;
	EXTI_Config(&exti_init);
	EXTI_Init();

	NVIC_EnableIRQ(EINT0_IRQn);
}

void configTimer(){
	TIM_TIMERCFG_Type tim_cfg;
	tim_cfg.PrescaleOption = TIM_PRESCALE_USVAL;
	tim_cfg.PrescaleValue = 1000;


	TIM_MATCHCFG_Type mtch_cfg;
	mtch_cfg.MatchValue = 50;
	mtch_cfg.ExtMatchOutputType = TIM_EXTMATCH_TOGGLE;
	mtch_cfg.IntOnMatch = DISABLE;
	mtch_cfg.MatchChannel = 1;
	mtch_cfg.ResetOnMatch = ENABLE;
	mtch_cfg.StopOnMatch = DISABLE;

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &tim_cfg);
	TIM_ConfigMatch(LPC_TIM0, &mtch_cfg);
}

void ADC_IRQHandle(){
	if(calibration_on)
		calibrate();
	else
	{
		int temp_value = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_1);

		if(sample_number < 10)
		{
			adc_values[sample_number] = temp_value;
			prom += adc_values[sample_number];
			sample_number++;
		}
		else
		{
			sample_number = 0;
			check = prom/10;
			check_interval(check - first_value);
			prom = 0;
		}
	}
}

void EINT0_IRQHandler(){
	calibration_on = 1;
	TIM_ResetCounter(LPC_TIM0);
	TIM_Cmd(LPC_TIM0, ENABLE);
	ADC_StartCmd(LPC_ADC, ADC_START_ON_MAT01);
	ADC_EdgeStartConfig(LPC_ADC, ADC_START_ON_FALLING);
	EXTI_ClearEXTIFlag(EXTI_EINT0);
	configDMA_UART();
}


void calibrate(){
	if(calib_flag < 10){
		first_value += ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_1);
		calib_flag++;
	}else if(calib_flag == 10){
		first_value = first_value/10;
		calibration_on = 0;
		calib_flag = 0;
	}
}

void check_interval(int new){
	GPIO_ClearValue(2, 0x3F);
	if(new > 100)	{
		startAlarm(1000);
		GPIO_SetValue(2, 1<<0);
		message0[0] = '#';
	}
	else
		message0[0] = ' ';
	if(new > 300){
		GPIO_SetValue(2,1<<1);
		message0[1] = '#';
	}
	else
		message0[1] = ' ';
	if(new > 600){
		GPIO_SetValue(2,1<<2);
		startAlarm(5000);
		message0[2] = '#';
	}
	else
		message0[2] = ' ';
	if(new > 900){
		GPIO_SetValue(2,1<<3);
		message0[3] = '#';
	}
	else
		message0[3] = ' ';
	if(new > 1200){
		GPIO_SetValue(2,1<<4);
		startAlarm(12000);
		message0[4] = '#';
	}
	else
		message0[4] = ' ';
	if(new > 1500){
		GPIO_SetValue(2,1<<5);
		message0[5] = '#';
	}
	else
		message0[5] = ' ';
	message0[6] = '\n';
}


void configDMA_DAC(){
	DMA_LLI_Struct_DAC.SrcAddr= (uint32_t)signal; //el source va a ser el arreglo de la funcion seno
	DMA_LLI_Struct_DAC.DstAddr= (uint32_t)&(LPC_DAC->DACR);//el destination es el registro del DAC
	DMA_LLI_Struct_DAC.NextLLI= (uint32_t)&DMA_LLI_Struct_DAC;
	DMA_LLI_Struct_DAC.Control= 60
			| (2<<18)//Fuente: 32bits
			| (2<<21)//Destino: 32bits
			| (1<<26)//Incremento automático de la fuente
			;

	GPDMACfg_DAC.ChannelNum = 0;
	GPDMACfg_DAC.SrcMemAddr = (uint32_t)signal;
	GPDMACfg_DAC.DstMemAddr = 0;
	GPDMACfg_DAC.TransferSize = 60;
	GPDMACfg_DAC.TransferWidth = 0;
	GPDMACfg_DAC.TransferType = GPDMA_TRANSFERTYPE_M2P;
	GPDMACfg_DAC.SrcConn = 0;
	GPDMACfg_DAC.DstConn = GPDMA_CONN_DAC;
	GPDMACfg_DAC.DMALLI= (uint32_t)&DMA_LLI_Struct_DAC;
	GPDMA_Setup(&GPDMACfg_DAC);
}

void configDAC(){
	DAC_CONVERTER_CFG_Type dacStruc;
	dacStruc.CNT_ENA = 1;
	dacStruc.DMA_ENA = 1;

	DAC_Init(LPC_DAC);// Inicializo controlador de GPDMA
	DAC_ConfigDAConverterControl(LPC_DAC, &dacStruc);
}

void startAlarm(int frecuencia){ //hay que pasarle la frec que queremos que suene
	//stopAlarm();
	uint32_t time_out = 25000000/(frecuencia*60);
	configDMA_DAC();
	DAC_SetDMATimeOut(LPC_DAC, time_out);
	GPDMA_ChannelCmd(0, ENABLE); //Enciende el modulo DMA channel0
}

void configUART(void){
	UART_CFG_Type UARTConfigStruct;
	UART_FIFO_CFG_Type UARTFIFOConfigStruct;

	//Configuracion por defecto:
	UART_ConfigStructInit(&UARTConfigStruct);
	//Inicializa periferico
	UART_Init(LPC_UART0, &UARTConfigStruct);


	//Configuracion de la FIFO para DMA
	UARTFIFOConfigStruct.FIFO_DMAMode = ENABLE;
	UARTFIFOConfigStruct.FIFO_Level = UART_FIFO_TRGLEV0;
	UARTFIFOConfigStruct.FIFO_ResetRxBuf = ENABLE;
	UARTFIFOConfigStruct.FIFO_ResetTxBuf = ENABLE;

	//Inicializa FIFO
	UART_FIFOConfig(LPC_UART0, &UARTFIFOConfigStruct);
	//Habilita transmision
	UART_TxCmd(LPC_UART0, ENABLE);
}

void configDMA_UART(void){
	DMA_LLI_Struct_UART.SrcAddr= (uint32_t)message0;
	DMA_LLI_Struct_UART.DstAddr= (uint32_t)&LPC_UART0->THR;
	DMA_LLI_Struct_UART.NextLLI= &DMA_LLI_Struct_UART;
	DMA_LLI_Struct_UART.Control= sizeof(message0)
		| 	(2<<12)
		| 	(1<<26)
		;

	NVIC_DisableIRQ(DMA_IRQn);

	GPDMACfg_UART.ChannelNum = 1;
	GPDMACfg_UART.SrcMemAddr = (uint32_t)message0;
	GPDMACfg_UART.DstMemAddr = 0;
	GPDMACfg_UART.TransferSize = sizeof(message0);
	GPDMACfg_UART.TransferWidth = 0;
	GPDMACfg_UART.TransferType = GPDMA_TRANSFERTYPE_M2P;
	GPDMACfg_UART.SrcConn = 0;
	GPDMACfg_UART.DstConn = GPDMA_CONN_UART0_Tx;
	GPDMACfg_UART.DMALLI = (uint32_t)&DMA_LLI_Struct_UART;

	GPDMA_Setup(&GPDMACfg_UART);

	GPDMA_ChannelCmd(1, ENABLE);
}

void clearMessage(){
	for(int i = 0; i<7; i++){
			message0[i] = ' ';
		}
}
