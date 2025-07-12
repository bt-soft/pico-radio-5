#pragma once
#include <cstddef>
#include <cstdint>

/**
 * @brief Fejlett spektrum-alapú CW/RTTY dekóder (időzítés, karakter, szóköz, puffer)
 * Az AudioProcessor által előállított spektrum (magnitude) tömböt használja.
 */
class CwRttyDecoder {
  public:
    CwRttyDecoder(float sampleRate, int fftSize, float binWidthHz);
    void process(const double *spectrum);
    void setCwFreq(float freqHz);
    void setRttyFreq(float markHz, float spaceHz);
    char getCharacterFromBuffer();
    void resetDecoderState();

  private:
    // Paraméterek
    float sampleRate_;
    int fftSize_;
    float binWidthHz_;
    float cwFreqHz_ = 800.0f;
    float rttyMarkHz_ = 2125.0f, rttySpaceHz_ = 2295.0f;

    // Időzítés, állapotok
    uint64_t lastEdgeMicros_ = 0;
    uint64_t lastToneOnMicros_ = 0;
    uint64_t lastToneOffMicros_ = 0;
    bool lastTone_ = false;
    bool decoderStarted_ = false;
    bool measuringTone_ = false;
    bool wordSpaceProcessed_ = false;
    bool inInactiveState_ = false;
    char lastDecodedChar_ = '\0';

    // Adaptív referencia
    unsigned long startReferenceMs_ = 120;
    unsigned long currentReferenceMs_ = 120;
    unsigned long toneMinDurationMs_ = 9999L;
    unsigned long toneMaxDurationMs_ = 0L;

    // Morse fa
    int treeIndex_ = 0;
    int treeOffset_ = 0;
    int treeCount_ = 0;
    static constexpr int MORSE_TREE_ROOT_INDEX = 0x3F;
    static constexpr int MORSE_TREE_INITIAL_OFFSET = 0x20;
    static constexpr int MORSE_TREE_MAX_DEPTH = 6;
    static const char MORSE_TREE_SYMBOLS[128];

    // Elem időtartamok
    static constexpr int MAX_TONES = 6;
    unsigned long rawToneDurations_[MAX_TONES] = {0};
    int toneIndex_ = 0;

    // Körkörös karakterpuffer
    static constexpr int DECODED_CHAR_BUFFER_SIZE = 16;
    char decodedCharBuffer_[DECODED_CHAR_BUFFER_SIZE] = {0};
    int charBufferReadPos_ = 0;
    int charBufferWritePos_ = 0;
    int charBufferCount_ = 0;

    // Belső segédfüggvények
    void resetMorseTree();
    char getCharFromTree();
    void processDot();
    void processDash();
    void updateReferenceTimings(unsigned long duration);
    char processCollectedElements();
    void addToBuffer(char c);
};