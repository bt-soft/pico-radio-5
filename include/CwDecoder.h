#ifndef __CW_DECODER_H
#define __CW_DECODER_H

#include <Arduino.h>
#include <cstring> // For strcmp
#include <map>     // For the Morse code table
#include <string>  // For std::string in the map

// Custom comparator for C-style strings (const char*)
struct CStringCompare {
    bool operator()(const char *a, const char *b) const { return std::strcmp(a, b) < 0; }
};

// --- CW (Morse) dekóder osztály ---
/**
 * CW (Morse) dekóder osztály
 * FFT alapú morze dekódolás, adaptív küszöbökkel és WPM követéssel.
 */
class CwDecoder {
  public:
    CwDecoder();
    void clear();
    void processFftData(const float *fftData, uint16_t fftSize, float binWidth);
    String getDecodedText();

  private:
    // --- Jelfeldolgozás ---
    bool freqInRange_;
    float peakFrequencyHz_;
    float peakMagnitude_;
    float noiseLevel_;
    float signalThreshold_;
    bool prevIsToneDetected;
    bool isToneDetected;
    void detectTone(const float *fftData, uint16_t fftSize, float binWidth);

    // --- FIFO mintapuffer ---
    static constexpr int SAMPLE_BUF_SIZE = 128;
    uint8_t sampleBuf[SAMPLE_BUF_SIZE]; // 1: tone, 0: silence
    int sampleHead;
    int sampleCount;

    // --- Dekódolás ---
    String decodedText;
    String currentSymbol;
    unsigned long lastEdgeMs;
    int toneSamples;
    int silenceSamples;
    float dotLenMs;

    // Morse-fa (statikus, csak olvasás)
    char decodeMorse(const String &morse);
    void pushSymbol(char symbol);
    void pushChar(char c);
    void resetSymbol();
};

#endif // __CW_DECODER_H