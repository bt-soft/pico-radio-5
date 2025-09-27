#pragma once

#include <Arduino.h>
#include <cstring>
#include <map>

/**
 * RTTY dekóder állapotok
 */
enum RttyState {
    RTTY_IDLE,  // Várakozás jelre
    RTTY_START, // Start bit detektálva
    RTTY_DATA,  // Adat bitek fogadása
    RTTY_STOP   // Stop bitek fogadása
};

/**
 * RTTY baud rátók automatikus felismeréshez
 */
enum class RttyBaudRate : uint16_t { BAUD_45 = 45, BAUD_50 = 50, BAUD_75 = 75, BAUD_110 = 110, BAUD_150 = 150, BAUD_300 = 300 };

/**
 * RTTY (Radio Teletype) dekóder osztály
 * FFT alapú RTTY dekódolás, automatikus baud felismeréssel és adaptív küszöbökkel.
 */
class RttyDecoder {
  public:
    RttyDecoder();
    void clear();
    void processRttyFftData(const float *fftData, uint16_t fftSize, float binWidth);
    String getDecodedText();
    void clearDecodedText(); // Manuális szöveg törlés

    // Konfigurálás
    void setMarkFrequency(uint16_t markHz);
    void setShiftFrequency(uint16_t shiftHz);
    void setBaudRate(RttyBaudRate baud);

    // Automatikus baud felismerés
    void enableAutoBaudDetection(bool enabled);
    RttyBaudRate getDetectedBaudRate() const;

  private:
    // Jelek detektálása
    bool detectMarkTone(const float *fftData, uint16_t fftSize, float binWidth);
    bool detectSpaceTone(const float *fftData, uint16_t fftSize, float binWidth);
    float calculateSnrForFrequency(const float *fftData, uint16_t fftSize, float binWidth, float targetFreq);

    // RTTY állapotgép és dekódolás
    void processRttyStateMachine(bool markPresent, bool spacePresent);
    void updateAdaptiveThreshold(bool toneDetected, float currentSnr);
    char baudotToChar(uint8_t baudotCode);

    // Automatikus baud felismerés
    void updateBaudRateDetection(bool markPresent, bool spacePresent);
    void analyzeBitTiming();
    bool isValidBitLength(unsigned long bitLength, RttyBaudRate testBaud);

    // Adaptív dekódolás segédfüggvények
    void discardCurrentBit(const char *reason);

    // === FREKVENCIA BEÁLLÍTÁSOK ===
    uint16_t markFrequencyHz_;  // Mark frekvencia (1 bit)
    uint16_t spaceFrequencyHz_; // Space frekvencia (0 bit)
    uint16_t shiftHz_;          // Frekvencia eltolás

    // === BAUD RATE BEÁLLÍTÁSOK ===
    RttyBaudRate currentBaudRate_;
    bool autoBaudEnabled_;
    uint16_t bitLengthMs_;        // Egy bit hossza ms-ben
    uint16_t bitLengthTolerance_; // Tolerancia ± ms

    // === ÁLLAPOTGÉP VÁLTOZÓK ===
    RttyState currentState_;
    unsigned long bitStartTime_;
    unsigned long lastTransitionTime_;
    uint8_t currentBitIndex_;    // Aktuális bit pozíció (0-4 adat bit, 5-6 stop bit)
    uint8_t receivedBaudotCode_; // Fogadott 5-bites Baudot kód

    // === ADAPTÍV JELDETEKTÁLÁS ===
    float adaptiveMarkThreshold_;
    float adaptiveSpaceThreshold_;
    uint16_t recentMarkCount_;
    uint16_t recentSpaceCount_;
    uint16_t recentNoiseCount_;

    // === AUTOMATIKUS BAUD FELISMERÉS ===
    struct BaudDetectionData {
        RttyBaudRate baud;
        uint16_t validBitCount;
        unsigned long totalBitLength;
        float averageBitLength;
        float confidence;
    };
    BaudDetectionData baudCandidates_[6]; // 6 különböző baud rate
    uint8_t bitTimingHistory_[32];        // Utolsó 32 bit időzítés tárolása
    uint8_t bitHistoryIndex_;
    bool baudRateDetected_;

    // === KARAKTER SET KEZELÉS ===
    bool figureMode_; // FIGURES (számok/szimbólumok) mód aktív

    // === DEKÓDOLT ADATOK ===
    String decodedText_;     // Dekódolt szöveg (csak új karakterek)
    bool newCharacterAdded_; // Jelzi, hogy új karakter lett hozzáadva

    // === STATISZTIKÁK ===
    uint32_t detectedMarksCount_;
    uint32_t detectedSpacesCount_;
    uint32_t decodedCharactersCount_;
    uint32_t errorCount_;
};