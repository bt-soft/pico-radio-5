#pragma once

#include "arduinoFFT.h"
#include "defines.h"
#include <Arduino.h>

namespace AudioProcessorConstants {
// CwRttyDecoder deklaráció
// Audio input konstansok
const uint16_t MAX_SAMPLING_FREQUENCY = 30000;                         // 30kHz mintavételezés a 15kHz Nyquist limithez
const uint16_t MIN_SAMPLING_FREQUENCY = 2000;                          // 1kHz Minimum mintavételezési frekvencia -> 2kHz Nyquist limit
const uint16_t DEFAULT_AM_SAMPLING_FREQUENCY = 12000;                  // 12kHz AM mintavételezés, 6kHZ AH sávszélességhez
const uint16_t DEFAULT_FM_SAMPLING_FREQUENCY = MAX_SAMPLING_FREQUENCY; // 30kHz FM mintavételezés

// FFT konstansok
const uint16_t MIN_FFT_SAMPLES = 64;
const uint16_t MAX_FFT_SAMPLES = 2048;
const uint16_t DEFAULT_FFT_SAMPLES = 512;

// Auto gain konstansok
const float FFT_AUTO_GAIN_TARGET_PEAK = 1500.0f; // Cél amplitúdó auto gain módban
const float FFT_AUTO_GAIN_MIN_FACTOR = 0.1f;
const float FFT_AUTO_GAIN_MAX_FACTOR = 20.0f;
const float AUTO_GAIN_ATTACK_COEFF = 0.3f;   // Gyors attack
const float AUTO_GAIN_RELEASE_COEFF = 0.01f; // Lassú release

// Oszcilloszkóp konstansok
const uint16_t MAX_INTERNAL_WIDTH = 320;
const uint8_t OSCI_SAMPLE_DECIMATION_FACTOR = 1; // Csökkentve 2-ről 1-re több minta gyűjtéséhez

// Spektrum konstansok
const float LOW_FREQ_ATTENUATION_THRESHOLD_HZ = 500.0f;
const float LOW_FREQ_ATTENUATION_FACTOR = 10.0f;
} // namespace AudioProcessorConstants

class AudioCore1Manager; // Előre deklaráció

/**
 * Audio feldolgozó osztály a radio-2 projekt alapján
 * Egyszerű, közvetlen audio feldolgozás FFT-vel
 */
class AudioProcessor {
  private:
    // FFT objektum
    ArduinoFFT<double> FFT;

    // Konfigurációs referenciák
    float &activeFftGainConfigRef;
    uint8_t audioInputPin;
    uint16_t targetSamplingFrequency_;

    // FFT paraméterek
    float binWidthHz_;
    float smoothed_auto_gain_factor_;
    uint32_t sampleIntervalMicros_;
    uint16_t currentFftSize_;

    // FFT tömbök
    double *vReal;
    double *vImag;
    double *RvReal; // Magnitúdó eredmények

    // Oszcilloszkóp adatok
    int osciSamples[AudioProcessorConstants::MAX_INTERNAL_WIDTH];

    // Belső függvények
    bool allocateFftArrays(uint16_t size);
    void deallocateFftArrays();
    bool validateFftSize(uint16_t size) const;
    void calculateBinWidthHz();

  public:
    /**
     * AudioProcessor konstruktor
     * @param gainConfigRef Referencia a gain konfigurációs értékre
     * @param audioPin Az audio bemenet pin száma
     * @param targetSamplingFrequency Cél mintavételezési frekvencia Hz-ben
     * @param fftSize FFT méret (alapértelmezett: DEFAULT_FFT_SAMPLES)
     */
    AudioProcessor(float &gainConfigRef, uint8_t audioPin, uint16_t targetSamplingFrequency, uint16_t fftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    /**
     * AudioProcessor destruktor
     */
    ~AudioProcessor();

    /**
     * Mintavételezési frekvencia lekérése
     * @return A jelenlegi mintavételezési frekvencia Hz-ben
     */
    uint16_t getSamplingFrequency() const { return targetSamplingFrequency_; }

    /**
     * FFT méret lekérése
     * @return A jelenlegi FFT méret
     * @note A méret mindig 2 hatványa, pl. 64,
     */
    uint16_t getFftSize() const { return currentFftSize_; }

    /**
     * Fő audio feldolgozó függvény - mintavételezés, FFT számítás és spektrum analízis
     * @param collectOsciSamples true ha oszcilloszkóp mintákat is gyűjteni kell
     */
    void process(bool collectOsciSamples);

    /**
     * Spektrum magnitúdó adatok lekérése
     * @return Pointer a magnitúdó adatokra
     */
    const double *getMagnitudeData() const { return RvReal; }

    /**
     * Oszcilloszkóp minták lekérése
     * @return Pointer az oszcilloszkóp adatokra
     */
    const int *getOscilloscopeData() const { return osciSamples; }

    /**
     * FFT bin szélesség lekérése Hz-ben
     */
    float getBinWidthHz() const { return binWidthHz_; }

    /**
     * Jelenlegi auto gain faktor lekérése
     */
    float getCurrentAutoGain() const { return smoothed_auto_gain_factor_; }

  protected:
    /**
     * Mintavételezési frekvencia beállítása futási időben
     * @param newFs Az új mintavételezési frekvencia Hz-ben
     */
    bool setSamplingFrequency(uint16_t newFs);

    /**
     * FFT méret beállítása futásidőben
     * @param newSize Az új FFT méret
     * @return true ha sikeres, false ha hiba történt
     */
    bool setFftSize(uint16_t newSize);

    friend class AudioCore1Manager; // <-- csak ez az osztály férhet hozzá a protected és a private memberekhez

    // const double *getRvReal() const { return RvReal; }
    // int getCurrentFftSize() const { return currentFftSize_; }
    // float getBinWidthHz() const { return binWidthHz_; }
};
