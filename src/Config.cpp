#include "Config.h"

#include "Si4735Runtime.h"

/**
 * Alapértelmezett readonly konfigurációs adatok
 */
const Config_t DEFAULT_CONFIG = {
    //-- Band
    .currentBandIdx = 0, // Default band, FM

    // BandWidht
    .bwIdxAM = 0,  // BandWidth AM - Band::bandWidthAM index szerint -> "6.0" kHz
    .bwIdxFM = 0,  // BandWidth FM - Band::bandWidthFM[] index szerint -> "AUTO"
    .bwIdxMW = 0,  // BandWidth MW - Band::bandWidthAM index szerint -> "6.0" kHz
    .bwIdxSSB = 0, // BandWidth SSB - Band::bandWidthSSB[] index szerint ->
                   // "1.2" kHz  (the valid values are 0, 1, 2, 3, 4 or 5)

    // Step
    .ssIdxMW = 2, // Band::stepSizeAM[] index szerint -> 9kHz
    .ssIdxAM = 1, // Band::stepSizeAM[] index szerint -> 5kHz
    .ssIdxFM = 1, // Band::stepSizeFM[] index szerint -> 100kHz

    // Squelch
    .currentSquelch = 0,     // Squelch szint (0...50)
    .squelchUsesRSSI = true, // A squlech RSSI alapú legyen?

    // FM RDS
    .rdsEnabled = true,

    // Hangerő
    .currVolume = 50,

    // AGC
    .agcGain = static_cast<uint8_t>(Si4735Runtime::AgcGainMode::Automatic),        // -> 1,
    .currentAGCgain = static_cast<uint8_t>(Si4735Runtime::AgcGainMode::Automatic), // -> 1

    //--- TFT
    //.tftCalibrateData = {0, 0, 0, 0, 0}, // TFT touch kalibrációs adatok
    .tftCalibrateData = {214, 3721, 239, 3606, 7},
    .tftBackgroundBrightness = TFT_BACKGROUND_LED_MAX_BRIGHTNESS, // TFT Háttérvilágítás
    .tftDigitLigth = true,                                        // Inaktív szegmens látszódjon?

    //--- System
    .screenSaverTimeoutMinutes = SCREEN_SAVER_TIMEOUT, // Képernyővédő alapértelmezetten 5 perc
    .beeperEnabled = true,                             // Hangjelzés engedélyezése    // MiniAudioFft módok
                                                       // (kezdetben SpectrumLowRes - 0 érték)
    .rotaryAcceleratonEnabled = true,                  // Rotary gyorsítás engedélyezése

    // MiniAudioFft módok
    .miniAudioFftModeAm = 0,            // SpectrumLowRes mode
    .miniAudioFftModeFm = 0,            // SpectrumLowRes mode    // MiniAudioFft erősítés
    .miniAudioFftConfigAm = 0.0f,       // Auto Gain
    .miniAudioFftConfigFm = 0.0f,       // Auto Gain
    .miniAudioFftConfigAnalyzer = 0.0f, // Analyzerhez alapértelmezetten Auto Gain
    .miniAudioFftConfigRtty = 0.0f,     // RTTY-hez alapértelmezetten Auto Gain

    // CW és RTTY beállítások
    .cwReceiverOffsetHz = 600,      // 600 Hz CW offset
    .rttyMarkFrequencyHz = 2125.0f, // RTTY Mark frequency
    .rttyShiftHz = 170.0f,          // RTTY Shift

    // Audio processing alapértelmezett beállítások
    .audioModeAM = 0,     // AudioComponentType::SPECTRUM_LOW_RES
    .audioModeFM = 0,     // AudioComponentType::SPECTRUM_LOW_RES
    .audioEnabled = true, // Audio vizualizáció engedélyezve
    .audioFftSize = 256,  // Alapértelmezett FFT méret
    .audioFftGain = 1.0f, // Alapértelmezett FFT erősítés
};

// Globális konfiguráció példány
Config config;

// Globális BandStore példány
#include "BandStore.h"
BandStore bandStore;
