#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include "ConfigData.h"
#include "defines.h"
#include "pins.h"
#include "rtVars.h"
#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>
#include <pico/multicore.h>
#include <pico/mutex.h>

// Audio vizualizáció típusok
enum class AudioVisualizationType : uint8_t {
    SPECTRUM_LOW_RES = 0,  // Alacsony felbontású spektrum (sáv alapú)
    SPECTRUM_HIGH_RES = 1, // Magas felbontású spektrum
    OSCILLOSCOPE = 2,      // Oszcilloszkóp (időtartomány)
    OFF = 3                // Kikapcsolva
};

// Audio feldolgozási konstansok - a referencia stílusához igazítva
namespace AudioProcessorConstants {
// FFT konfiguráció
static constexpr uint16_t FFT_SIZE = 512;               // FFT méret
static constexpr uint16_t SPECTRUM_BINS = FFT_SIZE / 2; // Spektrum vonalak száma
static constexpr uint16_t LOW_RES_BINS = 12;            // Alacsony felbontású spektrum vonalak
static constexpr uint16_t OSCILLOSCOPE_SAMPLES = 86;    // Oszcilloszkóp minták száma

// Mintavételezés
static constexpr uint16_t SAMPLE_RATE = 20000;                        // 20 kHz mintavételezési frekvencia
static constexpr uint32_t SAMPLE_INTERVAL_US = 1000000 / SAMPLE_RATE; // Mintavételi intervallum

// Erősítés és feldolgozás
static constexpr float AMPLITUDE_SCALE = 40.0f;                    // Skálázási faktor
static constexpr float LOW_FREQ_ATTENUATION_THRESHOLD_HZ = 200.0f; // Alacsony frekvencia csillapítás
static constexpr float LOW_FREQ_ATTENUATION_FACTOR = 10.0f;        // Csillapítás faktor

// Auto Gain konstansok
static constexpr float FFT_AUTO_GAIN_TARGET_PEAK = 500.0f; // Cél csúcsérték
static constexpr float FFT_AUTO_GAIN_MIN_FACTOR = 0.1f;    // Minimális erősítés
static constexpr float FFT_AUTO_GAIN_MAX_FACTOR = 10.0f;   // Maximális erősítés
static constexpr float AUTO_GAIN_ATTACK_COEFF = 0.5f;      // Erősítés csökkentés
static constexpr float AUTO_GAIN_RELEASE_COEFF = 0.05f;    // Erősítés növelés

static constexpr int OSCI_SAMPLE_DECIMATION_FACTOR = 2; // Oszcilloszkóp decimáció
} // namespace AudioProcessorConstants

// Spektrum adatok struktúra
struct SpectrumData {
    float lowResBins[AudioProcessorConstants::LOW_RES_BINS];   // Alacsony felbontású spektrum
    float highResBins[AudioProcessorConstants::SPECTRUM_BINS]; // Magas felbontású spektrum
    float maxMagnitude;                                        // Maximális jelszint
    bool dataReady;                                            // Új adat elérhető
};

// Oszcilloszkóp adatok struktúra
struct OscilloscopeData {
    int samples[AudioProcessorConstants::OSCILLOSCOPE_SAMPLES]; // Nyers minták
    float rms;                                                  // RMS érték
    float peak;                                                 // Peak érték
    bool dataReady;                                             // Új adat elérhető
};

// Közös adatstruktúra a core-ok között
struct SharedAudioData {
    volatile bool enabled;                // Audio feldolgozás engedélyezve
    volatile AudioVisualizationType mode; // Aktuális vizualizációs mód
    volatile float gain;                  // Erősítés faktor (0.0 = auto, -1.0 = off, >0 = manuális)

    SpectrumData spectrum;         // Spektrum adatok
    OscilloscopeData oscilloscope; // Oszcilloszkóp adatok

    mutex_t dataMutex; // Mutex az adatok védelmére
};

/**
 * @brief Audio feldolgozó osztály - Core1-en fut
 * A referencia AudioProcessor logikáját használja
 */
class AudioProcessor {
  private:
    // FFT objektum
    ArduinoFFT<double> fft;

    // Dinamikus tömbök FFT-hez
    double *vReal;
    double *vImag;
    double *magnitudeData;

    // Audio pin
    int audioPin;

    // Referencia a közös adatokra
    SharedAudioData *sharedData;

    // Erősítési faktor simítása
    float smoothedAutoGainFactor;

    // Feldolgozási paraméterek
    float binWidthHz;
    uint32_t lastProcessTime;

  public:
    /**
     * @brief Konstruktor
     * @param shared Közös adatstruktúra pointere
     * @param audioInputPin Audio bemenet pin száma
     */
    AudioProcessor(SharedAudioData *shared, int audioInputPin);

    /**
     * @brief Destruktor
     */
    ~AudioProcessor();

    /**
     * @brief Inicializálás
     */
    void initialize();

    /**
     * @brief Fő feldolgozási ciklus - Core1-en fut
     * A referencia process() függvény logikáját használja
     */
    void processLoop();

    /**
     * @brief Spektrum feldolgozása
     * @param collectOsciSamples Oszcilloszkóp minták gyűjtése is
     */
    void processSpectrum(bool collectOsciSamples);

    /**
     * @brief Alacsony felbontású spektrum számítása
     */
    void calculateLowResSpectrum();

  private:
    /**
     * @brief FFT tömbök allokálása
     */
    bool allocateFFTArrays();

    /**
     * @brief FFT tömbök felszabadítása
     */
    void deallocateFFTArrays();

    /**
     * @brief Frekvencia bin szélesség számítása
     */
    void calculateBinWidth();
};

// Globális függvények Core1 kezeléséhez
namespace AudioProcessorCore1 {
/**
 * @brief Core1 belépési pont
 */
void core1Entry();

/**
 * @brief Core1 inicializálás
 */
void initializeCore1(SharedAudioData *sharedData);

/**
 * @brief Core1 leállítás
 */
void shutdownCore1();

/**
 * @brief Audio feldolgozás engedélyezése/tiltása
 */
void setAudioEnabled(bool enabled);

/**
 * @brief Vizualizációs mód beállítása
 */
void setVisualizationMode(AudioVisualizationType mode);

/**
 * @brief Sávszűrő frekvenciák beállítása
 */
void setBandFilterFrequencies(float lowFreq, float highFreq);

/**
 * @brief Erősítés beállítása
 */
void setGain(float gain);

/**
 * @brief Core1 feldolgozás szüneteltetése (EEPROM műveletek előtt)
 */
void pauseCore1();

/**
 * @brief Core1 feldolgozás folytatása (EEPROM műveletek után)
 */
void resumeCore1();
} // namespace AudioProcessorCore1

// Globális közös adat
extern SharedAudioData g_sharedAudioData;

#endif // AUDIO_PROCESSOR_H
