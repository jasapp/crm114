#define F_CPU 8000000UL
#define EEPSIZE 128
#define V_REF REFS1
#define BOGOMIPS (F_CPU/4000)
#define PHASE 0xA1          // phase-correct PWM both channels

#define PWM_PIN PB1
#define PWM_LVL OCR0B

#define ALT_PWM_PIN PB0
#define ALT_PWM_LVL OCR0A

#define CAP_PIN     PB3     // pin 2, OTC
#define CAP_CHANNEL 0x03    
#define CAP_DIDR    ADC3D   // Digital input disable bit corresponding with PB3

#define CAP_SHORT 190       // represents our fast press for changing modes
#define CAP_MEDIUM 90       

#define VOLTAGE_PIN PB2     // pin 7, voltage ADC
#define ADC_CHANNEL 0x01    // MUX 01 corresponds with PB2
#define ADC_DIDR    ADC1D   // Digital input disable bit corresponding with PB2
#define ADC_PRSCL   0x06    // clk/64

#define TEMP_CHANNEL 0x0F   // mux[3:0] in ADMUX to 1111

