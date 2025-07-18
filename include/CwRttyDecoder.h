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
    static constexpr float MIN_CW_FREQ_HZ = 700.0f; // Kicsit tágabbra vesszük a sávot
    static constexpr float MAX_CW_FREQ_HZ = 1000.0f;
    // A statikus SIGNAL_THRESHOLD helyett dinamikus küszöböt használunk
    static constexpr float NOISE_FLOOR_FACTOR = 8.0f;      // A küszöb a zajszint ennyiszerese lesz (kísérletezést igényel)
    static constexpr float MINIMUM_THRESHOLD = 150.0f;     // Abszolút minimum küszöb a téves detektálások ellen
    static constexpr float NOISE_SMOOTHING_FACTOR = 0.05f; // A zajszint simítási faktora

    // Időzítési konstansok (kezdeti értékek, később lehet dinamikus)
    static constexpr uint16_t WPM = 15;                   // Words Per Minute
    static constexpr uint16_t DIT_LENGTH_MS = 1200 / WPM; // A "PARIS" standard alapján
    static constexpr uint16_t DAH_LENGTH_MS = 3 * DIT_LENGTH_MS;
    static constexpr float DIT_DAH_THRESHOLD_MS = (DIT_LENGTH_MS + DAH_LENGTH_MS) / 2.0f;
    static constexpr uint16_t CHAR_SPACE_MS = 3 * DIT_LENGTH_MS;
    static constexpr uint16_t WORD_SPACE_MS = 7 * DIT_LENGTH_MS;

    // Belső állapotváltozók a CW dekódoláshoz
    String decodedText;
    DecoderState currentState;
    uint32_t lastStateChangeTime;
    bool signalPresent;
    float peakFrequencyHz;
    float peakMagnitude;
    float noiseLevel_;
    float signalThreshold_;

    // Morse dekódoláshoz szükséges változók (későbbi lépésekhez)
    String currentMorseChar;
    uint32_t lastMarkTime;

    void updateState(bool signalIsOn, uint32_t now);
    char morseToChar(const String &morseCode);
};

#endif // __CW_RTTY_DECODER_H