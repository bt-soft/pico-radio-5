#pragma once

// TFT (A TFT_eSPI_User_Setup.h-ban a pinout)

// I2C si4735
#define PIN_SI4735_I2C_SDA 8
#define PIN_SI4735_I2C_SCL 9
#define PIN_SI4735_RESET 10

// Audio FFT bemenet
#define PIN_AUDIO_INPUT A0 // A0/GPIO26 az FFT audio bemenethez

// Feszültségmérés
#define PIN_VBUS_EXTERNAL_MEASURE_INPUT A1 // A1/GPIO27 a VBUS mérésének feszültségosztós bemenetéhez

// Rotary Encoder
#define PIN_ENCODER_DT 17
#define PIN_ENCODER_CLK 16
#define PIN_ENCODER_SW 18

// Others
#define PIN_TFT_BACKGROUND_LED 1 // A NYÁK miatt áthelyezve a  GPIO1-es lábra
#define PIN_AUDIO_MUTE 19
#define PIN_BEEPER 20
