#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

#include <stdint.h> // uint8_t, uint16_t, stb.

#include "defines.h"

// Audio vizualizáció típusok enum
enum class AudioComponentType : uint8_t {
    OFF = 0,               // Kikapcsolva
    SPECTRUM_LOW_RES = 1,  // Alacsony felbontású spektrum (sáv alapú)
    SPECTRUM_HIGH_RES = 2, // Magas felbontású spektrum
    OSCILLOSCOPE = 3,      // Oszcilloszkóp (időtartomány)
    ENVELOPE = 4,          // Burkológörbe
    WATERFALL = 5,         // Waterfall diagram
    CW_WATERFALL = 6,      // CW specifikus waterfall
    RTTY_WATERFALL = 7     // RTTY specifikus waterfall
};

// Konfig struktúra típusdefiníció
struct Config_t {
    uint8_t currentBandIdx; // Aktuális sáv indexe

    // BandWidht
    uint8_t bwIdxAM;
    uint8_t bwIdxFM;
    uint8_t bwIdxMW;
    uint8_t bwIdxSSB;

    // Step
    uint8_t ssIdxMW;
    uint8_t ssIdxAM;
    uint8_t ssIdxFM;

    // Squelch
    uint8_t currentSquelch;
    bool squelchUsesRSSI; // A squlech RSSI alapú legyen?

    // FM RDS
    bool rdsEnabled;

    // Hangerő
    uint8_t currVolume;

    // AGC
    uint8_t agcGain;
    uint8_t currentAGCgain; // AGC manual értéke

    //--- TFT
    uint16_t tftCalibrateData[5];    // TFT touch kalibrációs adatok
    uint8_t tftBackgroundBrightness; // TFT Háttérvilágítás
    bool tftDigitLigth;              // Inaktív szegmens látszódjon?

    //--- System
    uint8_t screenSaverTimeoutMinutes; // Képernyővédő ideje percekben (1-30)
    bool beeperEnabled;                // Hangjelzés engedélyezése
    bool rotaryAcceleratonEnabled;     // Rotary gyorsítás engedélyezése

    float miniAudioFftConfigAm; // -1.0f: Disabled, 0.0f: Auto, >0.0f: Manual Gain Factor
    float miniAudioFftConfigFm; // -1.0f: Disabled, 0.0f: Auto, >0.0f: Manual Gain Factor

    float miniAudioFftConfigAnalyzer; // MiniAudioFft erősítés konfigurációja az Analyzerhez
    float miniAudioFftConfigRtty;     // MiniAudioFft erősítés konfigurációja az RTTY-hez

    // CW és RTTY beállítások
    uint16_t cwReceiverOffsetHz; // CW vételi eltolás Hz-ben
    // RTTY frekvenciák
    uint16_t rttyMarkFrequencyHz; // RTTY Mark frekvencia Hz-ben
    uint16_t rttyShiftHz;         // RTTY Shift Hz-ben

    // Audio processing beállítások
    uint8_t audioModeAM; // Utolsó audio mód AM képernyőn (AudioComponentType)
    uint8_t audioModeFM; // Utolsó audio mód FM képernyőn (AudioComponentType)
    // float audioFftGain;    // Audio FFT erősítés (0.1 - 10.0)
};

#endif // CONFIG_DATA_H
