#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_nvic.h"
#include "inc/hw_timer.h"
#include "inc/hw_gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/adc.h"
#include "driverlib/debug.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/fpu.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "driverlib/fpu.h"

#define MALHA_ABERTA

void main(void)
{
    // Setup the system clock to run at 80 Mhz from PLL with crystal reference
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

    // Necessário habilitar a FPU?
    FPULazyStackingEnable();
    FPUEnable();

    // PF2 e PF3 estão no periférico PWM1
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);

    // Acesso a todas as portas (TODO: remover as portas não utilizadas)
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    // DRIVE_EN
    GPIOPinTypeGPIOOutput(GPIO_PORTC_BASE, GPIO_PIN_6);

    // Ativar o drive (alta)
    GPIOPinWrite(GPIO_PORTC_BASE, GPIO_PIN_6, GPIO_PIN_6);

    // VPWM_1
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);
    GPIOPinConfigure(GPIO_PF2_M1PWM6);

    // VPWM2
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_3);
    GPIOPinConfigure(GPIO_PF3_M1PWM7);

    // PWM CONFIG
    PWMGenConfigure(PWM1_BASE, PWM_GEN_3, PWM_GEN_MODE_UP_DOWN);
    PWMGenPeriodSet(PWM1_BASE, PWM_GEN_3, 1000);
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, 500);
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_7, 500);
    PWMOutputState(PWM1_BASE, PWM_OUT_6_BIT, true);
    PWMOutputState(PWM1_BASE, PWM_OUT_7_BIT, true);
    PWMGenEnable(PWM1_BASE, PWM_GEN_3);

    // ADC0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    // Ativa a função ADC nos pinos
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3);
    ADCHardwareOversampleConfigure(ADC0_BASE, 2);
    // Define a sequencia de amostragem ADC0, definida como sequencia 1 com prioridade 0
    ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);
    // Ler o PE0 - ADC POTENCIOMETRO - Iref
    ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_CH3);
    // Ler o PE2 - ADC REDE
    ADCSequenceStepConfigure(ADC0_BASE, 1, 1, ADC_CTL_CH1);
    // Ler o PE3 - ADC SOMADOR
    ADCSequenceStepConfigure(ADC0_BASE, 1, 2, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
    // Habilita a sequêncixa de amostragem 1
    ADCSequenceEnable(ADC0_BASE, 1);

    int32_t i32VRede = 0;
    int32_t i32SensorCorrente = 0;
    float fAjuste = 0;

    uint32_t ui32ADC0Value[3] = {0, 0, 0}; // [PE0, PE2, PE3]
    uint32_t ui32PWM1Val = 0;
    uint32_t ui32PWM2Val = 0;

    int32_t i32AdcToPwm = 0;

    while (1)
    {
        ADCIntClear(ADC0_BASE, 1); // limpa o flag da interrupção do ADC0
        ADCProcessorTrigger(ADC0_BASE, 1); // trigger do ADC0
        while(!ADCIntStatus(ADC0_BASE, 1, false)); // Aguarda o ADC0 terminar a conversão

        // Guarda na variável os valores coletados na sequência 1
        ADCSequenceDataGet(ADC0_BASE, 1, ui32ADC0Value);

        // Faz a atribuição da sequência
        i32SensorCorrente = ui32ADC0Value[2] - 2048;
        i32VRede = ui32ADC0Value[1] - 2048;
        fAjuste = (float)ui32ADC0Value[0] / 4095.0f;

#ifdef MALHA_ABERTA
        // Aplica a escala para 1000/4095
        i32AdcToPwm = (int32_t)(i32VRede * 0.244140625f);

        // Complementa os sinais
        ui32PWM1Val = (500 + i32AdcToPwm);
        ui32PWM2Val = (500 - i32AdcToPwm);

        // Atribui a largura de pulso
        PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, ui32PWM1Val);
        PWMPulseWidthSet(PWM1_BASE, PWM_OUT_7, ui32PWM2Val);


#else

        //      0.03561 z^2 + 0.006676 z - 0.02894
        //      ----------------------------------
        //            z^2 - 1.376 z + 0.3761

        static const float kn0 = -0.02894f;
        static const float kn1 = 0.006676f;
        static const float kn2 = 0.03561f;

        static const float kd0 = 0.3761f;
        static const float kd1 = -1.376f;
        static const float kd2 = 1.0f;

        static int32_t erro0 = 0;
        static int32_t erro1 = 0;
        static int32_t erro2 = 0;

        static int32_t Vc0 = 0;
        static int32_t Vc1 = 0;
        static int32_t Vc2 = 0;

        erro2 = erro1;
        erro1 = erro0;
        erro0 = i32SensorCorrente * fAjuste; // ?

        Vc2 = Vc1;
        Vc1 = Vc0;
        Vc0 = (kn2 * erro2 + kn1 * erro1 + kn0 * erro0) - (kd2 * Vc2 + kd1 * Vc1);

        if (Vc0 >  500) Vc0 =  500;
        if (Vc0 < -500) Vc0 = -500;

        // Atribui a largura de pulso
        PWMPulseWidthSet(PWM1_BASE, PWM_OUT_6, 500 + Vc0);
        PWMPulseWidthSet(PWM1_BASE, PWM_OUT_7, 500 - Vc0);
#endif
    }
}
