#ifndef __PINS_H
#define __PINS_H

// TFT (A TFT_eSPI_User_Setup.h-ban a pinout)

// I2C si4735
#define PIN_SI4735_I2C_SDA 8
#define PIN_SI4735_I2C_SCL 9
#define PIN_SI4735_RESET 10

// Feszültségmérés
#define PIN_VBUS_INPUT A0 // A0/GPIO26 a VBUS bemenethez

// Audio FFT bemenet
#define PIN_AUDIO_INPUT A1 // A1/GPIO27 az audio bemenethez

// Rotary Encoder
#define PIN_ENCODER_CLK 17
#define PIN_ENCODER_DT 16
#define PIN_ENCODER_SW 18

// Others
#define PIN_TFT_BACKGROUND_LED 21
#define PIN_AUDIO_MUTE 20
#define PIN_BEEPER 22

#endif // __PINS_H
