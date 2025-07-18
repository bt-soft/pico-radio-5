#ifndef __CW_RTTY_DECODER_H
#define __CW_RTTY_DECODER_H

#include <Arduino.h>

class CwRttyDecoder {
  public:
    // Dekóder állapotok
    enum class DecoderState { IDLE, MARK, SPACE };

    CwRttyDecoder();
    void processFftData(const float *fftData, uint16_t fftSize, float binWidth);
    String getDecodedText();
    void clear();

  private:
    // Konstansok
    static constexpr float MIN_CW_FREQ_HZ = 800.0f;
    static constexpr float MAX_CW_FREQ_HZ = 900.0f;
    static constexpr float SIGNAL_THRESHOLD = 500.0f; // Ezt kísérletezéssel kell finomhangolni!

    // Időzítési konstansok (kezdeti értékek, később lehet dinamikus)
    static constexpr uint16_t WPM = 15;                   // Words Per Minute
    static constexpr uint16_t DIT_LENGTH_MS = 1200 / WPM; // A "PARIS" standard alapján
    static constexpr uint16_t DAH_LENGTH_MS = 3 * DIT_LENGTH_MS;
    static constexpr float DIT_DAH_THRESHOLD_MS = (DIT_LENGTH_MS + DAH_LENGTH_MS) / 2.0f;

    // Belső állapotváltozók a CW dekódoláshoz
    String decodedText;
    DecoderState currentState;
    uint32_t lastStateChangeTime;
    bool signalPresent;
    float peakFrequencyHz;
    float peakMagnitude;

    // Morse dekódoláshoz szükséges változók (későbbi lépésekhez)
    String currentMorseChar;
    uint32_t lastMarkTime;

    void updateState(bool signalIsOn, uint32_t now);
};

#endif // __CW_RTTY_DECODER_H