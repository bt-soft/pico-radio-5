#ifndef __CW_DECODER_H
#define __CW_DECODER_H

#include <Arduino.h>
#include <map>    // For the Morse code table
#include <string> // For std::string in the map
#include <cstring> // For strcmp

// Custom comparator for C-style strings (const char*)
struct CStringCompare {
    bool operator()(const char* a, const char* b) const {
        return std::strcmp(a, b) < 0;
    }
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
    /**
     * Minden állapot és változó alaphelyzetbe állítása
     */
    void clear();
    /**
     * Hibakeresési információk összegyűjtése, állapot, jelszintek, morze stb.
     */
    String getDebugInfo() const;

  private:
    // --- Állapotgép (belső állapotok) ---
    enum class DecoderState {
        IDLE,               // Várakozás az első jelre
        TONE,               // Jel (pont/vonal) detektálva
        SILENCE_INTRA_CHAR, // Karakteren belüli szünet
        SILENCE_CHAR_SPACE, // Karakterek közötti szünet
        SILENCE_WORD_SPACE  // Szavak közötti szünet
    };
    DecoderState currentState_;
    unsigned long lastStateChangeMs_; // Utolsó állapotváltás ideje

    // --- Dekódolás ---
    String decodedText_;                             // Eddig dekódolt szöveg
    String currentMorseChar_;                        // Az aktuálisan épülő morze karakter (. vagy - sorozat)
    static const std::map<const char*, char, CStringCompare> morseTable_; // Statikus morze tábla
    /**
     * Az aktuális morze karakter dekódolása és hozzáfűzése a szöveghez
     */
    void decodeMorseChar();

    // --- Jelfeldolgozás ---
    float peakFrequencyHz_; // Detektált csúcsfrekvencia
    float peakMagnitude_;   // Detektált csúcs amplitúdó
    float noiseLevel_;      // Becsült zajszint
    float signalThreshold_; // Jeldetektálási küszöb

    // --- Időzítés & WPM adaptáció ---
    float bitTimeMs_; // Egy pont időtartama (ms)
    float wpm_;       // Szavak per perc

    // Állapotok a bitTime számításhoz
    bool bitTimeInitialized_;
    unsigned int bitTimeInitCount_;
    float bitTimeInitSum_;
    unsigned long bitTimeInitStartMs_; // BitTime inicializációs ablak kezdete

    // Segédfüggvények állapotváltáshoz
    /**
     * Állapotgép: IDLE állapot kezelése
     * @param isToneDetected Detektáltunk-e jelet
     * @param now Aktuális idő (ms)
     */
    void handleIdleState(bool isToneDetected, unsigned long now);
    /**
     * Állapotgép: TONE állapot kezelése
     * @param isToneDetected Detektáltunk-e jelet
     * @param now Aktuális idő (ms)
     */
    void handleToneState(bool isToneDetected, unsigned long now);
    /**
     * Állapotgép: SILENCE állapotok kezelése
     * @param isToneDetected Detektáltunk-e jelet
     * @param now Aktuális idő (ms)
     */
    void handleSilenceState(bool isToneDetected, unsigned long now);

    // Időzítés osztályozás segédek
    enum class MorseTiming {
        TOO_SHORT,        // Túl rövid (zaj)
        DOT,              // Pont
        DASH,             // Vonal
        INTRA_CHAR_SPACE, // Karakteren belüli szünet
        CHAR_SPACE,       // Karakterek közötti szünet
        WORD_SPACE        // Szavak közötti szünet
    };
    /**
     * Egy adott időtartam osztályozása (pont, vonal, szünet stb.)
     * @param duration Időtartam (ms)
     * @param isToneDuration Hang (true) vagy szünet (false)
     * @return Osztályozott morze időzítés
     */
    MorseTiming classifyDuration(unsigned long duration, bool isToneDuration);
    /**
     * Pont idő (bitTime) adaptáció, automatikus WPM követés
     * @param duration Aktuális hang időtartama (ms)
     * @return Frissített bitTime érték (ms)
     */
    float updateAndGetBitTime(unsigned long duration);

    // --- Hibakeresés ---
    /**
     * Időzítés szövegesen (debug célra)
     */
    String getMorseTimingString(MorseTiming timing) const;
};

#endif // __CW_DECODER_H