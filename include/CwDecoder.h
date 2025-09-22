#pragma once

#include <Arduino.h>
#include <cstring> // For strcmp
#include <map>     // For the Morse code table

/**
 * CW (Morse) dekóder osztály
 * FFT alapú morze dekódolás, adaptív küszöbökkel és WPM követéssel.
 */
class CwDecoder {
  public:
    CwDecoder();
    void clear();
    void processCwFftData(const float *fftData, uint16_t fftSize, float binWidth);
    String getDecodedText();

  private:
    bool detectTone(const float *fftData, uint16_t fftSize, float binWidth);
};
