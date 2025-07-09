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
    ENVELOPE = 3,          // Burkológörbe
    WATERFALL = 4,         // Waterfall diagram
    CW_WATERFALL = 5,      // CW specifikus waterfall
    RTTY_WATERFALL = 6     // RTTY specifikus waterfall
};

// Audio feldolgozási konstansok
namespace AudioProcessorConstants {
static constexpr uint16_t SAMPLE_RATE = 20000;          // 20 kHz mintavételezési frekvencia
static constexpr uint16_t FFT_SIZE = 1024;              // FFT méret
static constexpr uint16_t BUFFER_SIZE = FFT_SIZE * 2;   // Cirkuláris buffer méret
static constexpr uint16_t SPECTRUM_BINS = FFT_SIZE / 2; // Spektrum vonalak száma
static constexpr uint16_t LOW_RES_BINS = 12;            // Alacsony felbontású spektrum vonalak
static constexpr uint16_t WATERFALL_HEIGHT = 100;       // Waterfall magasság
static constexpr uint16_t OSCILLOSCOPE_SAMPLES = 256;   // Oszcilloszkóp minták száma
static constexpr uint16_t ENVELOPE_SAMPLES = 128;       // Burkológörbe minták száma
static constexpr float PEAK_DECAY_RATE = 0.95f;         // Peak csökkenés sebessége (0.0-1.0)
static constexpr float NOISE_FLOOR = -80.0f;            // Zajszint dB-ben
} // namespace AudioProcessorConstants

// Audio vizualizációs adatok union - csak egy aktív egyszerre
union AudioVisualizationData {
    struct {
        float lowResBins[AudioProcessorConstants::LOW_RES_BINS];    // Alacsony felbontású spektrum
        float lowResPeaks[AudioProcessorConstants::LOW_RES_BINS];   // Peak értékek
        float highResBins[AudioProcessorConstants::SPECTRUM_BINS];  // Magas felbontású spektrum
        float highResPeaks[AudioProcessorConstants::SPECTRUM_BINS]; // Peak értékek
        float maxMagnitude;                                         // Maximális jelszint
    } spectrum;

    struct {
        int16_t samples[AudioProcessorConstants::OSCILLOSCOPE_SAMPLES]; // Nyers minták
        float rms;                                                      // RMS érték
        float peak;                                                     // Peak érték
    } oscilloscope;

    struct {
        float samples[AudioProcessorConstants::ENVELOPE_SAMPLES]; // Burkológörbe értékek
        float smoothedLevel;                                      // Simított jelszint
    } envelope;

    struct {
        float bins[AudioProcessorConstants::SPECTRUM_BINS];                                                         // Spektrum vonalak
        uint8_t waterfallBuffer[AudioProcessorConstants::WATERFALL_HEIGHT][AudioProcessorConstants::SPECTRUM_BINS]; // Waterfall buffer
        uint16_t currentRow;                                                                                        // Aktuális sor
    } waterfall;
};

// Audio statisztikák struktúra
struct AudioStatistics {
    uint32_t samplesProcessed; // Feldolgozott minták száma
    uint32_t fftCalculations;  // FFT számítások száma
    uint32_t bufferOverruns;   // Buffer túlcsordulások száma
    uint32_t processingTimeUs; // Feldolgozási idő mikroszekundumban
    float cpuUsagePercent;     // CPU használat százalékban
    uint32_t lastUpdateTime;   // Utolsó frissítés ideje
};

// Közös adatstruktúra a core-ok között
struct SharedAudioData {
    volatile bool enabled;                // Audio feldolgozás engedélyezve
    volatile bool dataReady;              // Új adat elérhető
    volatile AudioVisualizationType mode; // Aktuális vizualizációs mód

    // Sávszűrő beállítások (képernyők állítják be)
    volatile float currentBandLowFreq;  // Aktuális sáv alsó frekvenciája (Hz)
    volatile float currentBandHighFreq; // Aktuális sáv felső frekvenciája (Hz)

    AudioVisualizationData data; // Union - csak egy aktív vizualizációs mód
    AudioStatistics statistics;  // Statisztikák

    mutex_t dataMutex; // Mutex az adatok védelmére
};

/**
 * @brief Audio feldolgozó osztály - Core1-en fut
 */
class AudioProcessor {
  private:
    // FFT objektumok
    ArduinoFFT<float> fft;

    // Adatpufferek
    float rawSamples[AudioProcessorConstants::BUFFER_SIZE];   // Nyers minták cirkuláris buffer
    float fftReal[AudioProcessorConstants::FFT_SIZE];         // FFT valós rész
    float fftImag[AudioProcessorConstants::FFT_SIZE];         // FFT képzetes rész
    float magnitudes[AudioProcessorConstants::SPECTRUM_BINS]; // Spektrum nagyságok

    // Buffer kezelés
    volatile uint16_t writeIndex;       // Írási index
    volatile uint16_t readIndex;        // Olvasási index
    volatile uint16_t availableSamples; // Elérhető minták száma

    // Időzítés és statisztikák
    uint32_t lastSampleTime;      // Utolsó mintavétel ideje
    uint32_t processingStartTime; // Feldolgozás kezdési ideje
    uint32_t sampleInterval;      // Mintavételi intervallum mikroszek.

    // Referencia a közös adatokra
    SharedAudioData *sharedData;

    // DC offset kalibráció a feszültségosztó miatt
    float dcOffset;          // Mért DC offset ADC egységekben
    bool dcOffsetCalibrated; // Kalibráció állapota

  public:
    /**
     * @brief Konstruktor
     * @param shared Közös adatstruktúra pointere
     */
    AudioProcessor(SharedAudioData *shared);

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
     */
    void processLoop();

    /**
     * @brief Mintavétel és buffer kezelés
     */
    void sampleAudio();

    /**
     * @brief FFT számítás és spektrum feldolgozás
     */
    void calculateFFT();

    /**
     * @brief Spektrum adatok feldolgozása
     */
    void processSpectrum();

    /**
     * @brief Oszcilloszkóp adatok feldolgozása
     */
    void processOscilloscope();

    /**
     * @brief Burkológörbe feldolgozása
     */
    void processEnvelope();

    /**
     * @brief Waterfall feldolgozása
     */
    void processWaterfall();

    /**
     * @brief Statisztikák frissítése
     */
    void updateStatistics();

    /**
     * @brief Debug információk kiírása
     */
    void printDebugInfo();

  private:
    /**
     * @brief Frekvencia index számítás
     */
    uint16_t frequencyToIndex(float frequency);

    /**
     * @brief Index frekvencia számítás
     */
    float indexToFrequency(uint16_t index);

    /**
     * @brief Sávszűrő alkalmazása
     */
    void applyBandFilter();

    /**
     * @brief Simítás alkalmazása
     */
    void applySmoothing(float *data, uint16_t length, float factor);

    /**
     * @brief Peak csökkenés alkalmazása
     */
    void applyPeakDecay(float *peaks, const float *current, uint16_t length);

    /**
     * @brief dB konverzió
     */
    float magnitudeToDb(float magnitude);

    /**
     * @brief Buffer elérhető minták számítása
     */
    uint16_t getAvailableSamples();

    /**
     * @brief Buffer írása
     */
    void writeToBuffer(float sample);

    /**
     * @brief Buffer olvasása
     */
    float readFromBuffer();

    /**
     * @brief DC offset kalibráció - feszültségosztó kompenzálás
     * @details 10k-10k feszültségosztó + 10nF kondenzátor miatt szükséges
     */
    void calibrateDCOffset();
};

// Globális függvények
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
 * @brief Statisztikák lekérdezése (Core0-ról hívható)
 */
AudioStatistics getStatistics();

/**
 * @brief Debug információk kiírása (Core0-ról hívható)
 */
void printDebugFromCore0();
} // namespace AudioProcessorCore1

// Globális közös adat
extern SharedAudioData g_sharedAudioData;

#endif // AUDIO_PROCESSOR_H
