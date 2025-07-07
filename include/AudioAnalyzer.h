#ifndef __AUDIO_ANALYZER_H
#define __AUDIO_ANALYZER_H

#include "arduinoFFT.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "pins.h"
#include <Arduino.h>

/**
 * @brief Audio elemzési módok
 */
enum class AudioDisplayMode : uint8_t {
    OFF = 0,           ///< Kikapcsolt
    SPECTRUM_LOW_RES,  ///< Kis felbontású spektrum (bar chart + peak hold)
    SPECTRUM_HIGH_RES, ///< Nagy felbontású spektrum
    OSCILLOSCOPE,      ///< Oszcilloszkóp
    ENVELOPE,          ///< Burkológörbe
    WATERFALL,         ///< Waterfall
    WATERFALL_CW_RTTY, ///< Waterfall CW/RTTY hangolássegéd
    MODE_COUNT         ///< Módok száma
};

/**
 * @brief Audio spektrum adatok strukturája
 */
struct AudioData {
    static constexpr uint16_t FFT_SIZE = 256;               ///< FFT méret
    static constexpr uint16_t SPECTRUM_BINS = FFT_SIZE / 2; ///< Spektrum oszlopok száma
    static constexpr uint16_t LOW_RES_BINS = 16;            ///< Kis felbontású spektrum oszlopok
    static constexpr uint16_t SAMPLE_RATE = 8000;           ///< Mintavételi frekvencia (Hz)

    double vReal[FFT_SIZE];                ///< Valós mintavételi adatok
    double vImag[FFT_SIZE];                ///< Képzetes mintavételi adatok
    uint16_t spectrumData[SPECTRUM_BINS];  ///< Spektrum adatok (0-4095)
    uint16_t lowResSpectrum[LOW_RES_BINS]; ///< Kis felbontású spektrum
    uint16_t peakHold[LOW_RES_BINS];       ///< Peak hold értékek
    uint16_t rawSamples[FFT_SIZE];         ///< Nyers mintavételi adatok oszcilloszkóphoz
    uint32_t timestamp;                    ///< Időbélyeg
    bool isMuted;                          ///< Némítás állapot
};

/**
 * @brief Audio elemző osztály - Core1-en futó audio feldolgozás
 */
class AudioAnalyzer {
  public:
    /**
     * @brief Konstruktor
     */
    AudioAnalyzer();

    /**
     * @brief Destruktor
     */
    ~AudioAnalyzer();

    /**
     * @brief Inicializálás és Core1 task indítása
     * @return true ha sikeres, false egyébként
     */
    bool init();

    /**
     * @brief Leállítás
     */
    void stop();

    /**
     * @brief Legfrissebb audio adatok lekérése (Core0-ról hívható)
     * @param data Kimeneti audio adatok
     * @return true ha sikerült adatot olvasni, false egyébként
     */
    bool getLatestData(AudioData &data);

    /**
     * @brief Aktív-e a feldolgozás
     * @return true ha fut, false egyébként
     */
    bool isRunning() const { return running; }

    /**
     * @brief Statisztikák lekérése
     */
    struct Stats {
        uint32_t samplesProcessed; ///< Feldolgozott minták száma
        uint32_t fftCalculations;  ///< FFT számítások száma
        uint32_t dataUpdates;      ///< Adatfrissítések száma
        uint32_t processingTimeUs; ///< Átlagos feldolgozási idő (μs)
    };

    Stats getStats() const { return stats; }

  private:
    // Multicore mutex az adatok szinkronizációjához
    mutex_t dataMutex;

    // FFT objektum
    ArduinoFFT<double> fft;

    // Dupla pufferelés az adatokhoz
    AudioData dataBuffers[2];
    volatile int activeBuffer;
    volatile bool newDataAvailable;

    // Belső munkaadat
    AudioData workingData;
    Stats stats;
    bool running;

    // Peak hold kezelés
    uint32_t lastPeakDecay;
    uint16_t peakHoldBuffer[AudioData::LOW_RES_BINS];
    static constexpr uint32_t PEAK_DECAY_INTERVAL = 50; // ms
    static constexpr uint16_t PEAK_DECAY_RATE = 10;     // /frame

    /**
     * @brief Core1 main loop function (static wrapper)
     */
    static void core1Main();

    /**
     * @brief Core1 loop implementáció
     */
    void core1Loop();

    /**
     * @brief Audio mintavétel és feldolgozás
     */
    void processAudio();

    /**
     * @brief FFT számítás
     */
    void calculateFFT();

    /**
     * @brief Kis felbontású spektrum számítása
     */
    void calculateLowResSpectrum();

    /**
     * @brief Peak hold frissítése
     */
    void updatePeakHold();

    /**
     * @brief ADC inicializálás
     */
    void initADC();

    /**
     * @brief Gyors ADC olvasás
     * @return ADC érték (0-4095)
     */
    inline uint16_t fastAnalogRead() { return analogRead(PIN_AUDIO_INPUT); }
};

#endif // __AUDIO_ANALYZER_H
