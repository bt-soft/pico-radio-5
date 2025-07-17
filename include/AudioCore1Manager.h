#pragma once

#include <Arduino.h>
#include <pico/multicore.h>
#include <pico/mutex.h>

#include "AudioProcessor.h"

/**
 * @brief Core1 dedikált audio feldolgozó manager
 *
 * Ez az osztály kezeli az audio feldolgozást a core1-en, biztosítva
 * a real-time spektrum analízist és buffer kommunikációt a core0-val.
 */
class AudioCore1Manager {
  public:
    // Megosztott adatstruktúra a core0↔core1 kommunikációhoz
    struct SharedAudioData {
        volatile bool spectrumDataReady;
        volatile bool oscilloscopeDataReady;
        volatile bool core1Running;
        volatile bool core1ShouldStop;

        // Spektrum adatok
        double spectrumBuffer[2048];         // Max FFT size
        volatile uint16_t samplingFrequency; // Aktuális mintavételezési frekvencia
        volatile uint16_t fftSize;           // Aktuális FFT méret
        volatile float binWidthHz;
        volatile float currentAutoGain;

        // Oszcilloszkóp adatok
        int oscilloscopeBuffer[320]; // MAX_INTERNAL_WIDTH

        // Audio konfiguráció
        float fftGainConfigAm;
        float fftGainConfigFm;
        volatile bool configChanged;

        // EEPROM mutex védelem
        volatile bool eepromWriteInProgress;
        volatile bool core1AudioPaused;
        volatile bool core1AudioPausedAck; // Core1 visszajelzés a szüneteltetésről

        // Mutex a thread-safe hozzáféréshez
        mutex_t dataMutex;
    };

  private:
    static SharedAudioData *pSharedData_;
    static AudioProcessor *pAudioProcessor_;
    static bool initialized_;
    static float *currentGainConfigRef_;
    static bool collectOsci_; // Oszcilloszkóp minták gyűjtése

    // Core1 belső függvények
    static void core1Entry();
    static void core1AudioLoop();
    static void updateAudioConfig();

  public:
    // Statikus számláló a megszakítások számának tárolására
    static volatile int interruptCount;
    static volatile bool canRun;

    /**
     * @brief Megszakítás kezelő az AudioCore1Manager számára
     * @param timer A megszakítás időzítő
     * @return true ha a megszakítás sikeresen kezelve lett
     *
     */
    static bool onTimerInterrupt(struct repeating_timer *);

    /**
     * @brief Core1 audio feldolgozó inicializálása
     * @param fftSampleSize Az FFT méret
     * @param samplingFrequency A mintavételezési frekvencia
     *
     *  Beállítjuk az időzítőt
     *    idő (ms) = (FFT méret / mintavételezési frekvencia) * 1000
     *             = (2048 / 30000) * 1000
     *             ≈ 68,27 ms
     *
     */
    static bool setIsrTimer(uint16_t fftSampleSize, uint16_t samplingFrequency);

    // EEPROM védelem
    static void pauseCore1Audio();
    static void resumeCore1Audio();
    static bool isCore1Paused();
    /**
     * @brief Core1 audio manager inicializálása
     * @param gainConfigAmRef Referencia az AM FFT gain konfigurációra
     * @param gainConfigFmRef Referencia az FM FFT gain konfigurációra
     * @param audioPin Audio bemenet pin száma
     * @param samplingFreq Mintavételezési frekvencia
     * @param initialFftSize Kezdeti FFT méret
     * @return true ha sikeres, false ha hiba történt
     */
    static bool init(float &gainConfigAmRef, float &gainConfigFmRef, int audioPin, uint16_t samplingFreq, uint16_t initialFftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    /**
     * @brief Core1 audio manager leállítása
     */
    static void shutdown();

    /**
     * @brief Spektrum méret lekérése (ha nincs friss adat, cached érték)
     * @param outFftSize Kimeneti FFT méret
     * @return true ha sikeres, false ha hiba történt
     */
    static bool getFftSize(uint16_t *outFftSize);

    /**
     * @brief Core1 mintavételezési frekvencia lekérése (ha nincs friss adat, cached érték)
     * @param outSampleFrequency Kimeneti mintavételezési frekvencia
     * @return true ha sikeres, false ha hiba történt
     */
    static bool getFftSampleFrequency(uint16_t *outSampleFrequency);

    /**
     * @brief Core1 aktuális bin szélesség lekérése (ha nincs friss adat, cached érték)
     * @param outBinWidth Kimeneti bin szélesség Hz-ben
     * @return true ha sikeres, false ha hiba történt
     */
    static bool getFftCurrentBinWidth(float *outBinWidth);

    /**
     * @brief Spektrum adatok lekérése (core0-ból hívható)
     * @param outData Kimeneti buffer a spektrum adatoknak
     * @param outFftSize Kimeneti FFT méret
     * @param outBinWidth Kimeneti bin szélesség Hz-ben
     * @param outAutoGain Jelenlegi auto gain faktor
     * @return true ha friss adat érhető el, false egyébként
     */
    static bool getSpectrumData(const double **outData, uint16_t *outFftSize, float *outBinWidth, float *outAutoGain);

    /**
     * @brief Oszcilloszkóp adatok lekérése (core0-ból hívható)
     * @param outData Kimeneti buffer az oszcilloszkóp adatoknak
     * @return true ha friss adat érhető el, false egyébként
     */
    static bool getOscilloscopeData(const int **outData);

    /**
     * @brief FFT méret váltása (core0-ból hívható)
     * @param newSize Új FFT méret
     * @return true ha sikeres, false egyébként
     */
    static bool setFftSize(uint16_t newSize);

    /**
     * @brief Mintavételezési frekvencia beállítása (core0-ból hívható)
     * @param newSamplingFrequency Az új mintavételezési frekvencia Hz-ben
     */
    static bool setSamplingFrequency(uint16_t newSamplingFrequency);

    /**
     * @brief Core1 állapot lekérése
     * @return true ha a core1 fut és működik
     */
    static bool isRunning();

    /**
     * @brief Core1 Oszcilloszkóp adatok gyűjtésének vezérlése
     */
    static void setCollectOsci(bool collectOsci);
    static bool getCollectOsci();

    /**
     * @brief Debug információk kiírása
     */
    static void debugInfo();
};
