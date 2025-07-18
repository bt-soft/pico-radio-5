#ifndef __CW_RTTY_DECODER_H
#define __CW_RTTY_DECODER_H

#include <Arduino.h>

class CwRttyDecoder {
  public:
    CwRttyDecoder();
    void processFftData(const float *fftData, uint16_t fftSize, float binWidth);
    String getDecodedText();
    void clear();

  private:
    // Konstansok
    static constexpr float NOISE_FLOOR_FACTOR = 8.0f;
    static constexpr float MINIMUM_THRESHOLD = 150.0f;
    static constexpr float NOISE_SMOOTHING_FACTOR = 0.05f;

    // Adaptív időzítési konstansok (a "jó" CwDecoder.cpp-ből)
    static constexpr uint16_t DOT_MIN_MS = 20;
    static constexpr uint16_t DOT_MAX_MS = 250;
    static constexpr uint16_t DASH_MAX_MS = 2500; // Megnövelve, hogy a lassú jeleket is elfogadja
    static constexpr uint16_t MAX_CW_ELEMENTS = 8;
    static constexpr uint32_t MAX_SILENCE_MS = 4000;
    static constexpr uint16_t MIN_ADAPTIVE_DOT_MS = 15;
    static constexpr uint8_t NOISE_THRESHOLD_FACTOR = 5;

    // Dinamikus szünet szorzók
    static constexpr float CHAR_GAP_DOT_MULTIPLIER = 2.8f;
    static constexpr float WORD_GAP_DOT_MULTIPLIER = 7.0f;
    static constexpr uint16_t MIN_CHAR_GAP_MS_FALLBACK = 70;
    static constexpr uint16_t MIN_WORD_GAP_MS_FALLBACK = 200;
    static constexpr uint32_t SPACE_DEBUG_INTERVAL_MS = 1000;

    // Morse Tree konstansok
    static const char MORSE_TREE_SYMBOLS[];
    static constexpr int MORSE_TREE_ROOT_INDEX = 62;
    static constexpr int MORSE_TREE_INITIAL_OFFSET = 32;
    static constexpr int MORSE_TREE_MAX_DEPTH = 6;

    // Belső állapotváltozók a CW dekódoláshoz
    String decodedText;
    float peakFrequencyHz;
    float peakMagnitude;
    float noiseLevel_;
    float signalThreshold_;

    // Adaptív dekódoláshoz szükséges változók
    unsigned long startReferenceMs_;
    unsigned long currentReferenceMs_;
    unsigned long toneMinDurationMs_;
    unsigned long toneMaxDurationMs_;
    unsigned long leadingEdgeTimeMs_;
    unsigned long trailingEdgeTimeMs_;
    unsigned long rawToneDurations_[MAX_CW_ELEMENTS];
    short toneIndex_;
    bool decoderStarted_;
    bool measuringTone_;
    uint32_t lastActivityMs_;
    bool wordSpaceProcessed_;
    char lastDecodedChar_;
    bool inInactiveState;
    uint32_t lastSpaceDebugMs_;

    // Morse Tree változók
    int treeIndex_;
    int treeOffset_;
    int treeCount_;

    void updateReferenceTimings(unsigned long duration);
    char processCollectedElements();
    void resetMorseTree();
    char getCharFromTree();
    void processDot();
    void processDash();
};

#endif // __CW_RTTY_DECODER_H
