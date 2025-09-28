#pragma once

#include <Arduino.h>
#include <cstring> // For strcmp
#include <map>     // For the Morse code table

/**
 * CW dekóder állapotok
 */
enum CwState {
    CW_IDLE, // Várakozás jelre
    CW_TONE, // Jel aktív (pont vagy vonal)
    CW_PAUSE // Szünet (elemek, betűk vagy szavak között)
};

/**
 * CW (Morse) dekóder osztály
 * FFT alapú morze dekódolás, adaptív küszöbökkel és WPM követéssel.
 */
class CwDecoder {
  public:
    CwDecoder();
    void clear();
    String processCwFftData(const float *fftData, uint16_t fftSize, float binWidth);

    // Kalibrálás és konfiguráció
    void calibrateTimingFromWpm(uint8_t wpm);

  private:
    // Jelek detektálása
    bool detectTone(const float *fftData, uint16_t fftSize, float binWidth);

    // CW állapotgép és dekódolás
    void processCwStateMachine(bool tonePresent, String &newChars);
    void updateAdaptiveThreshold(bool toneDetected, float currentSnr);
    char morseToChar(const String &morseCode);

    // Adaptív dekódolás segédfüggvények
    void updateAdaptiveWordGap(unsigned long pauseLength);
    void discardCurrentPattern(const char *reason);

    // === ÁLLAPOTGÉP VÁLTOZÓK ===
    CwState currentState_;
    unsigned long toneStartTime_;
    unsigned long lastToneEndTime_;

    // === IDŐZÍTÉSI KONSTANSOK ===
    uint16_t dotLengthMs_;     // Pont hossza ms-ben
    uint16_t dashLengthMs_;    // Vonal hossza ms-ben
    uint16_t elementGapMs_;    // Elemek közti szünet
    uint16_t letterGapMs_;     // Betűk közti szünet
    uint16_t wordGapMs_;       // Szavak közti szünet (alapértelmezett)
    uint16_t adaptiveWordGap_; // Adaptív szavak közti szünet

    // Tolerancia értékek
    uint16_t dotMinMs_, dotMaxMs_;
    uint16_t dashMinMs_, dashMaxMs_;

    // === ADAPTÍV JELDETEKTÁLÁS ===
    float adaptiveSnrThreshold_;
    uint16_t recentToneCount_;
    uint16_t recentNoiseCount_;

    // === ADAPTÍV SZÓKÖZ MEGHATÁROZÁS ===
    unsigned long lastPauseLength_;
    float averagePauseLength_;
    uint16_t pauseCount_;

    // === DEKÓDOLT ADATOK ===
    String currentMorseBuffer_; // Aktuális morze betű
    bool wordSpaceAdded_;       // Jelzi, hogy az aktuális szünethez már hozzáadtunk szóközt

    // === STATISZTIKÁK ===
    uint32_t detectedDotsCount_;
    uint32_t detectedDashesCount_;
};