#include "stm32f10x.h"
#include "stm32f10x_adc.h"              // Keil::Device:StdPeriph Drivers:ADC

#include <stdio.h> // For sprintf
#include "vdda.h"

//*****************************************************************************
// Typical Vrefint value in millivolts (1.20 V = 1200 mV)
//#define VREFINT_MV  1200UL 
// Measured for this specific chip
#define VREFINT_MV  1215UL 

//*****************************************************************************
void my_ADC_Vref_Init(void) {
    ADC_InitTypeDef ADC_InitStructure;

    // 1. ADC1 clocking (72 MHz / 6 = 12 MHz, max allowed for F1 is 14 MHz)
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    // 2. ADC configuration
    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // 3. Enable internal reference source Vrefint
    ADC_TempSensorVrefintCmd(ENABLE);

    // 4. Enable ADC and perform calibration
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}
//*****************************************************************************
// Returns supply voltage in millivolts (e.g., 3290 mV)
uint16_t my_Get_Supply_Voltage_mV(void) {
    uint32_t adc_sum = 0;
    const uint8_t samples = 16;

    // Configure channel 17 (Vrefint) with maximum sample time
    ADC_RegularChannelConfig(ADC1, ADC_Channel_17, 1, ADC_SampleTime_239Cycles5);

    for (uint8_t i = 0; i < samples; i++) {
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
        adc_sum += ADC_GetConversionValue(ADC1);
    }

    uint32_t adc_avg = adc_sum / samples;

    if (adc_avg == 0) return 0;

    // Integer calculation in millivolts
    // Use 32-bit integers (UL) to prevent overflow during multiplication
    uint16_t vdda_mv = (uint16_t)((VREFINT_MV * 4095UL) / adc_avg);
    
    return vdda_mv;
}
//*****************************************************************************
