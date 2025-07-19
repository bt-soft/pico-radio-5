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
    /**
     * Konstruktor: minden állapotot alaphelyzetbe állít
     */
    CwDecoder();

    /**
     * Minden állapot és változó alaphelyzetbe állítása
     */
    void clear();

    /**
     * Fő jelfeldolgozó függvény: FFT adatokból morze jelek detektálása és állapotgép futtatása
     * @param fftData FFT amplitúdó tömb
     * @param fftSize FFT méret
     * @param binWidth Frekvencia bin szélesség (Hz)
     */
    void processFftData(const float *fftData, uint16_t fftSize, float binWidth);

    /**
     * Visszaadja az eddig dekódolt szöveget
     */
    String getDecodedText();

  private:
    // --- Jelfeldolgozás ---
    float peakFrequencyHz_; // Detektált csúcsfrekvencia
    float peakMagnitude_;   // Detektált csúcs amplitúdó
    float noiseLevel_;      // Becsült zajszint
    float signalThreshold_; // Jeldetektálási küszöb
};

#endif // __CW_DECODER_H