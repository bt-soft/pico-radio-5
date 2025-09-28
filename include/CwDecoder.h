#pragma once

#include <Arduino.h>
#include <cstring> // strcmp-hez
#include <map>     // A morze kód táblához

/**
 * @enum CwState
 * @brief A CW dekóder állapotgépének állapotait definiálja.
 *
 * Ez az enumeráció a dekóder különböző állapotait képviseli a bejövő jel feldolgozása során.
 */
enum CwState {
    CW_IDLE,  ///< A dekóder jel (hang) kezdetére vár.
    CW_TONE,  ///< A dekóder hangot észlelt és annak időtartamát méri.
    CW_PAUSE  ///< A dekóder egy hang végét észlelte és az azt követő szünetet méri.
};

/**
 * @class CwDecoder
 * @brief Egy CW (morze) dekóder, amely FFT adatokat dolgoz fel morze jelek dekódolásához.
 *
 * Ez az osztály egy állapotgépet valósít meg, amely a Fast Fourier Transform (FFT) által feldolgozott
 * audio adatokból dekódol morze kódot. Adaptív küszöbértékekkel rendelkezik a jelérzékeléshez
 * és dinamikus időzítési beállításokkal a becsült szavak percenkénti száma (WPM) alapján,
 * ami robusztussá teszi a küldési sebesség és a jelerősség változásaival szemben.
 */
class CwDecoder {
  public:
    /**
     * @brief Létrehoz egy új CwDecoder objektumot.
     */
    CwDecoder();

    /**
     * @brief Visszaállítja a dekódert a kezdeti állapotába.
     * Törli az összes puffert, visszaállítja az időzítés kalibrációját egy alapértelmezettre, és az állapotgépet IDLE állapotba helyezi.
     */
    void clear();

    /**
     * @brief Feldolgoz egy új FFT adatblokkot a CW jelek dekódolásához.
     * Ez a dekóder fő belépési pontja. Érzékeli a hangokat, futtatja az állapotgépet,
     * és visszaadja az újonnan dekódolt karaktereket.
     * @param fftData Mutató az FFT magnitúdó adatokat tartalmazó tömbre.
     * @param fftSize Az FFT adattömb mérete.
     * @param binWidth Az egyes FFT rekeszek frekvenciaszélessége Hz-ben.
     * @return Egy String, amely az ezen feldolgozási lépés során dekódolt karaktereket tartalmazza.
     */
    String processCwFftData(const float *fftData, uint16_t fftSize, float binWidth);

    /**
     * @brief Kalibrálja a dekóder időzítési paramétereit egy adott WPM alapján.
     * A pontok, vonalak és szünetek időtartama a WPM-ből származik a standard formulák szerint.
     * @param wpm A percenkénti szavak száma (Words Per Minute), amire kalibrálni kell.
     */
    void calibrateTimingFromWpm(uint8_t wpm);

  private:
    // --- Privát metódusok ---

    /**
     * @brief Érzékeli, hogy van-e CW hang az FFT adatokban.
     * Egy csúcsjelet keres egy beállított frekvenciaablakon belül, és kiszámítja az SNR-t.
     * @param fftData Mutató az FFT magnitúdó adatokat tartalmazó tömbre.
     * @param fftSize Az FFT adattömb mérete.
     * @param binWidth Az egyes FFT rekeszek frekvenciaszélessége Hz-ben.
     * @return Igaz, ha a hangot az adaptív SNR küszöbérték felett érzékeli, egyébként hamis.
     */
    bool detectTone(const float *fftData, uint16_t fftSize, float binWidth);

    /**
     * @brief A morze kód dekódolásának központi állapotgépe.
     * Ezt a metódust ismételten hívják meg a dekóder állapotának frissítésére attól függően, hogy jelenleg van-e hang.
     * @param tonePresent Igaz, ha jelenleg hangot érzékel, egyébként hamis.
     * @param newChars Referencia egy String-re, ahová az újonnan dekódolt karakterek kerülnek.
     */
    void processCwStateMachine(bool tonePresent, String &newChars);

    /**
     * @brief Frissíti az adaptív SNR küszöbértéket a hangérzékeléshez.
     * A küszöbérték csökken, ha sok hangot érzékel (ami jó jelet jelez), és növekszik,
     * ha sok a zaj, ami robusztusabbá teszi az érzékelést.
     * @param toneDetected Igaz, ha az utolsó feldolgozási keretben hangot észlelt.
     * @param currentSnr Az utolsó keret jel-zaj aránya (dB-ben).
     */
    void updateAdaptiveThreshold(bool toneDetected, float currentSnr);

    /**
     * @brief Átalakít egy morze kód stringet (pl. ".-") a megfelelő karakterré.
     * @param morseCode Az átalakítandó morze kód string.
     * @return A dekódolt karakter, vagy '?' ha a kód ismeretlen.
     */
    char morseToChar(const String &morseCode);

    /**
     * @brief Dinamikusan igazítja a szóköz időzítését a megfigyelt szünetek hossza alapján.
     * Ez segít a dekódernek alkalmazkodni a küldő szóköz-használatának változásaihoz.
     * @param pauseLength Az utoljára mért szünet hossza betűk/szavak között.
     */
    void updateAdaptiveWordGap(unsigned long pauseLength);

    /**
     * @brief Eldobja az aktuálisan pufferelt morze mintát, ha az érvénytelennek minősül.
     * @param reason Egy string, amely leírja, miért dobták el a mintát (hibakereséshez).
     */
    void discardCurrentPattern(const char *reason);

    // --- Állapotgép és időzítési változók ---
    CwState currentState_;          ///< A dekóder állapotgépének aktuális állapota.
    unsigned long toneStartTime_;   ///< Időbélyeg (ms), amikor az aktuális hang elkezdődött.
    unsigned long lastToneEndTime_; ///< Időbélyeg (ms), amikor az utolsó hang véget ért.

    // --- Időzítési konstansok (WPM-ből kalibrálva) ---
    uint16_t dotLengthMs_;  ///< Egy pont alap időtartama ezredmásodpercben.
    uint16_t dashLengthMs_; ///< Egy vonal alap időtartama ezredmásodpercben.
    uint16_t elementGapMs_; ///< Várt szünet az azonos betű elemei (pontok/vonalak) között.
    uint16_t letterGapMs_;  ///< Várt szünet a betűk között.
    uint16_t wordGapMs_;    ///< Alapértelmezett várt szünet a szavak között.

    // --- Adaptív időzítés és küszöbértékek ---
    uint16_t adaptiveWordGap_;      ///< Dinamikusan beállított szóköz a megfigyelt szünetek alapján.
    uint16_t dotMinMs_, dotMaxMs_;   ///< Érvényes pont minimális/maximális időtartama (toleranciával).
    uint16_t dashMinMs_, dashMaxMs_; ///< Érvényes vonal minimális/maximális időtartama (toleranciával).

    float adaptiveSnrThreshold_; ///< Adaptív SNR küszöbérték (dB) a hangérzékeléshez.
    uint16_t recentToneCount_;   ///< A közelmúltban észlelt hangok számlálója (adaptív küszöbhöz).
    uint16_t recentNoiseCount_;  ///< A közelmúltbeli zajos keretek számlálója (adaptív küszöbhöz).

    // --- Adaptív szóköz számítás ---
    unsigned long lastPauseLength_; ///< A legutóbbi szünet hossza.
    float averagePauseLength_;      ///< A közelmúltbeli hosszú szünetek átlagos hossza.
    uint16_t pauseCount_;           ///< Az átlagoláshoz mért szünetek száma.

    // --- Dekódolt adatpufferek ---
    String currentMorseBuffer_; ///< Puffer az aktuális pont- és vonalsorozathoz, amely egy betűt alkot.
    bool wordSpaceAdded_;       ///< Zászló, amely megakadályozza több szóköz hozzáadását egyetlen hosszú szünethez.

    // --- Statisztikák ---
    uint32_t detectedDotsCount_;  ///< A clear() óta észlelt pontok teljes száma.
    uint32_t detectedDashesCount_;///< A clear() óta észlelt vonalak teljes száma.
};