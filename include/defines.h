#ifndef DEFINES_H
#define DEFINES_H

#include <Arduino.h>

#include "pins.h"

//---- Program Information ------------------------------------------

#define PROGRAM_NAME "Pico Radio v4"
#define PROGRAM_VERSION "0.0.4"
#define PROGRAM_AUTHOR "bt-soft"

//---- Program Information ------------------------------------------

//--- ScreenNames ----
#define SCREEN_NAME_FM "ScreenFM"
#define SCREEN_NAME_AM "ScreenAM"
#define SCREEN_NAME_SCREENSAVER "SaverScreen"
#define SCREEN_NAME_SETUP "ScreenSetup"
#define SCREEN_NAME_SETUP_SYSTEM "ScreenSetupSystem"
#define SCREEN_NAME_SETUP_SI4735 "ScreenSetupSi4735"

#define SCREEN_NAME_MEMORY "ScreenMemory"
#define SCREEN_NAME_SCAN "ScreenScan"

#define SCREEN_NAME_TEST "TestScreen"
#define SCREEN_NAME_EMPTY "EmptyScreen"

//--- Debug ---
#define __DEBUG // Debug mód vezérlése

#ifdef __DEBUG
// #define SHOW_MEMORY_INFO
#define MEMORY_INFO_INTERVAL 20 * 1000 // 20mp

// Soros portra várakozás a debug üzenetek előtt
#define DEBUG_WAIT_FOR_SERIAL

// Debug keretek rajzolása a UI komponensek köré
#define DRAW_DEBUG_FRAMES

#endif

// Feszültségmérés
#define VBUS_DIVIDER_R1 197.5f // Ellenállás VBUS és A0 között (kOhm)
#define VBUS_DIVIDER_R2 99.5f  // Ellenállás A0 és GND között (kOhm)

// Rotary Encoder
#define __USE_ROTARY_ENCODER_IN_HW_TIMER

// TFT háttérvilágítás max érték
#define TFT_BACKGROUND_LED_MAX_BRIGHTNESS 255
#define TFT_BACKGROUND_LED_MIN_BRIGHTNESS 5

//--- Battery ---
#define MIN_BATTERY_VOLTAGE 270 // Minimum akkumulátor feszültség (V*100)
#define MAX_BATTERY_VOLTAGE 405 // Maximum akkumulátor feszültség (V*100)

//--- ScreenSaver
#define SCREEN_SAVER_TIMEOUT_MIN 1
#define SCREEN_SAVER_TIMEOUT_MAX 60
#define SCREEN_SAVER_TIMEOUT 10 // 1 perc a képernyővédő időzítése - tesztelés

//--- CW Decoder ---
#define CW_DECODER_DEFAULT_FREQUENCY 750 // Alapértelmezett CW dekóder frekvencia (Hz)
#define CW_DECODER_MIN_FREQUENCY 600     // Minimum CW dekóder frekvencia (Hz)
#define CW_DECODER_MAX_FREQUENCY 1500    // Maximum CW dekóder frekvencia (Hz)

//--- RTTY mód adatai
// #define RTTY_DEFAULT_MARKER_FREQUENCY 2295.0f                                      // RTTY jelölő frekvencia (Hz)
// #define RTTY_DEFAULT_SHIFT_FREQUENCY RTTY_DEFAULT_MARKER_FREQUENCY - RTTY_DEFAULT_SPACE_FREQUENCY  // RTTY eltolás frekvencia (170Hz)
// #define RTTY_DEFAULT_SPACE_FREQUENCY 2125.0f                                       // RTTY tér frekvencia (Hz)
#define RTTY_DEFAULT_MARKER_FREQUENCY 1100.0f                                                     // RTTY jelölő frekvencia (Hz)
#define RTTY_DEFAULT_SHIFT_FREQUENCY 425.0f                                                       // RTTY eltolás frekvencia (170Hz)
#define RTTY_DEFAULT_SPACE_FREQUENCY RTTY_DEFAULT_MARKER_FREQUENCY - RTTY_DEFAULT_SHIFT_FREQUENCY // RTTY space frekvencia (Hz)

//--- Array Utils ---
#define ARRAY_ITEM_COUNT(array) (sizeof(array) / sizeof(array[0]))

//--- Band Table ---
#define BANDTABLE_SIZE 30 // A band tábla mérete (bandTable[] tömb)

//--- C String compare -----
#define STREQ(a, b) (strcmp((a), (b)) == 0)

//--- Debug ---
#ifdef __DEBUG
#define DEBUG(fmt, ...) Serial.printf(fmt __VA_OPT__(, ) __VA_ARGS__)
#else
#define DEBUG(fmt, ...) // Üres makró, ha __DEBUG nincs definiálva
#endif

#endif // DEFINES_H