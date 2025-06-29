#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

#include <stdint.h> // uint8_t, uint16_t, stb.

#include "defines.h"

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

    // MiniAudioFft módok
    uint8_t miniAudioFftModeAm; // MiniAudioFft módja AM képernyőn
    uint8_t miniAudioFftModeFm; // MiniAudioFft módja FM képernyőn    // MiniAudioFft erősítés
    float miniAudioFftConfigAm; // -1.0f: Disabled, 0.0f: Auto, >0.0f: Manual Gain Factor
    float miniAudioFftConfigFm; // -1.0f: Disabled, 0.0f: Auto, >0.0f: Manual Gain Factor

    float miniAudioFftConfigAnalyzer; // MiniAudioFft erősítés konfigurációja az Analyzerhez
    float miniAudioFftConfigRtty;     // MiniAudioFft erősítés konfigurációja az RTTY-hez

    // CW és RTTY beállítások
    uint16_t cwReceiverOffsetHz; // CW vételi eltolás Hz-ben
    // RTTY frekvenciák
    float rttyMarkFrequencyHz; // RTTY Mark frekvencia Hz-ben
    float rttyShiftHz;         // RTTY Shift Hz-ben

    // Audio processing beállítások
    uint8_t audioModeAM;   // Utolsó audio mód AM képernyőn (AudioComponentType)
    uint8_t audioModeFM;   // Utolsó audio mód FM képernyőn (AudioComponentType)
    bool audioEnabled;     // Audio vizualizáció be/ki
    uint16_t audioFftSize; // FFT méret (256, 512, 1024)
    float audioFftGain;    // Audio FFT erősítés (0.1 - 10.0)
};

#endif // CONFIG_DATA_H
