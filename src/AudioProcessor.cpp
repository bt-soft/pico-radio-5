#include "AudioProcessor.h"
#include "Config.h"
#include "pins.h"

// Globális közös adat
SharedAudioData g_sharedAudioData;

// Core1 objektumok
static AudioProcessor *g_audioProcessor = nullptr;
static bool g_core1Running = false;

/**
 * @brief AudioProcessor konstruktor
 * @param shared Közös adatstruktúra pointere
 * @param audioInputPin Audio bemenet pin száma
 */
AudioProcessor::AudioProcessor(SharedAudioData *shared, int audioInputPin)
    : fft(), vReal(nullptr), vImag(nullptr), magnitudeData(nullptr), audioPin(audioInputPin), sharedData(shared), smoothedAutoGainFactor(1.0f), binWidthHz(0.0f), lastProcessTime(0) {

    // FFT tömbök allokálása
    if (!allocateFFTArrays()) {
        DEBUG("AudioProcessor: KRITIKUS - FFT tömbök allokálása sikertelen!\n");
        return;
    }

    // Bin szélesség számítása
    calculateBinWidth();

    DEBUG("AudioProcessor: Inicializálva - FFT méret: %d, Bin szélesség: %.2f Hz\n", AudioProcessorConstants::FFT_SIZE, (double)binWidthHz);
}

/**
 * @brief AudioProcessor destruktor
 */
AudioProcessor::~AudioProcessor() { deallocateFFTArrays(); }

/**
 * @brief FFT tömbök allokálása
 */
bool AudioProcessor::allocateFFTArrays() {
    // Meglévő tömbök felszabadítása
    deallocateFFTArrays();

    // Új tömbök allokálása
    vReal = new (std::nothrow) double[AudioProcessorConstants::FFT_SIZE];
    vImag = new (std::nothrow) double[AudioProcessorConstants::FFT_SIZE];
    magnitudeData = new (std::nothrow) double[AudioProcessorConstants::FFT_SIZE];

    if (!vReal || !vImag || !magnitudeData) {
        DEBUG("AudioProcessor: FFT tömbök allokálása sikertelen\n");
        deallocateFFTArrays();
        return false;
    }

    // Tömbök nullázása
    memset(vReal, 0, AudioProcessorConstants::FFT_SIZE * sizeof(double));
    memset(vImag, 0, AudioProcessorConstants::FFT_SIZE * sizeof(double));
    memset(magnitudeData, 0, AudioProcessorConstants::FFT_SIZE * sizeof(double));

    // FFT objektum inicializálása
    fft.setArrays(vReal, vImag, AudioProcessorConstants::FFT_SIZE);

    return true;
}

/**
 * @brief FFT tömbök felszabadítása
 */
void AudioProcessor::deallocateFFTArrays() {
    delete[] vReal;
    delete[] vImag;
    delete[] magnitudeData;

    vReal = nullptr;
    vImag = nullptr;
    magnitudeData = nullptr;
}

/**
 * @brief Frekvencia bin szélesség számítása
 */
void AudioProcessor::calculateBinWidth() { binWidthHz = static_cast<float>(AudioProcessorConstants::SAMPLE_RATE) / AudioProcessorConstants::FFT_SIZE; }

/**
 * @brief Inicializálás
 */
void AudioProcessor::initialize() {
    DEBUG("AudioProcessor: Inicializálás...\n");

    // ADC inicializálás
    analogReadResolution(12); // 12 bites ADC felbontás

    // Shared data inicializálás
    mutex_init(&sharedData->dataMutex);
    sharedData->enabled = false;
    sharedData->mode = AudioVisualizationType::OFF;
    sharedData->gain = 1.0f;

    // Spektrum adatok inicializálása
    memset(sharedData->spectrum.lowResBins, 0, sizeof(sharedData->spectrum.lowResBins));
    memset(sharedData->spectrum.highResBins, 0, sizeof(sharedData->spectrum.highResBins));
    sharedData->spectrum.maxMagnitude = 0.0f;
    sharedData->spectrum.dataReady = false;

    // Oszcilloszkóp adatok inicializálása
    for (int i = 0; i < AudioProcessorConstants::OSCILLOSCOPE_SAMPLES; i++) {
        sharedData->oscilloscope.samples[i] = 2048; // Középérték 12 bites ADC-hez
    }
    sharedData->oscilloscope.rms = 0.0f;
    sharedData->oscilloscope.peak = 0.0f;
    sharedData->oscilloscope.dataReady = false;

    DEBUG("AudioProcessor: Inicializálás befejezve\n");
}

/**
 * @brief Fő feldolgozási ciklus - Core1-en fut
 */
void AudioProcessor::processLoop() {
    uint32_t lastDebugTime = 0;
    uint32_t debugInterval = 5000; // Debug info 5 másodpercenként

    DEBUG("AudioProcessor: Core1 feldolgozási ciklus indult\n");

    while (g_core1Running) {
        // Debug információ időszakosan
        uint32_t currentTime = millis();
        if (currentTime - lastDebugTime >= debugInterval) {
            DEBUG("AudioProcessor: Core1 aktív - mód: %d, engedélyezve: %s\n", (int)sharedData->mode, sharedData->enabled ? "igen" : "nem");
            lastDebugTime = currentTime;
        }

        // Ha nincs engedélyezve az audio feldolgozás vagy némítás aktív, akkor nem dolgozunk
        if (!sharedData->enabled || rtv::muteStat || sharedData->mode == AudioVisualizationType::OFF) {
            delay(10);
            continue;
        }

        // Audio feldolgozás kezdete - pontos időmérés
        uint32_t processingStartTime = micros();

        // Vizualizációs adatok feldolgozása a módtól függően
        switch (sharedData->mode) {
            case AudioVisualizationType::SPECTRUM_LOW_RES:
            case AudioVisualizationType::SPECTRUM_HIGH_RES:
                processSpectrum(false);
                break;

            case AudioVisualizationType::OSCILLOSCOPE:
                processSpectrum(true); // Oszcilloszkóp mintákkal együtt
                break;

            default:
                break;
        }

        // Rövid szünet a CPU terhelés csökkentésére
        uint32_t processingTime = micros() - processingStartTime;
        if (processingTime < 20000) { // Ha kevesebb mint 20ms
            delay(5);
        }
    }

    DEBUG("AudioProcessor: Core1 feldolgozási ciklus befejeződött\n");
}

/**
 * @brief Spektrum feldolgozása - a referencia AudioProcessor logikával
 * @param collectOsciSamples Oszcilloszkóp minták gyűjtése is
 */
void AudioProcessor::processSpectrum(bool collectOsciSamples) {
    if (!vReal || !vImag || !magnitudeData) {
        DEBUG("AudioProcessor: FFT tömbök nem allokálva\n");
        return;
    }

    int osci_sample_idx = 0;
    uint32_t loopStartTimeMicros;
    double max_abs_sample_for_auto_gain = 0.0;

    // Debug info oszcilloszkóp mód esetén
    if (collectOsciSamples) {
        static uint32_t lastOscStartDebugTime = 0;
        if (millis() - lastOscStartDebugTime > 5000) {
            DEBUG("AudioProcessor: Starting oscilloscope sample collection\n");
            lastOscStartDebugTime = millis();
        }
    }

    // Ha a gain -1.0f, akkor töröljük a puffereket és visszatérünk
    if (sharedData->gain == -1.0f) {
        mutex_enter_blocking(&sharedData->dataMutex);
        memset(sharedData->spectrum.highResBins, 0, sizeof(sharedData->spectrum.highResBins));
        memset(sharedData->spectrum.lowResBins, 0, sizeof(sharedData->spectrum.lowResBins));
        if (collectOsciSamples) {
            for (int i = 0; i < AudioProcessorConstants::OSCILLOSCOPE_SAMPLES; i++) {
                sharedData->oscilloscope.samples[i] = 2048;
            }
        }
        mutex_exit(&sharedData->dataMutex);
        return;
    }

    // 1. Mintavételezés és középre igazítás, opcionális oszcilloszkóp mintagyűjtés
    for (int i = 0; i < AudioProcessorConstants::FFT_SIZE; i++) {
        loopStartTimeMicros = micros();

        // 4 minta átlagolása a zajcsökkentés érdekében
        uint32_t sum = 0;
        for (int j = 0; j < 4; j++) {
            sum += analogRead(audioPin);
        }
        double averaged_sample = sum / 4.0;

        // Oszcilloszkóp minta gyűjtése ha szükséges
        if (collectOsciSamples) {
            if (i % AudioProcessorConstants::OSCI_SAMPLE_DECIMATION_FACTOR == 0 && osci_sample_idx < AudioProcessorConstants::OSCILLOSCOPE_SAMPLES) {
                sharedData->oscilloscope.samples[osci_sample_idx] = static_cast<int>(averaged_sample);
                osci_sample_idx++;
            }
        }

        // Középre igazítás (2048 a nulla szint 12 bites ADC-nél)
        vReal[i] = averaged_sample - 2048.0;
        vImag[i] = 0.0;

        // Auto Gain mód esetén a legnagyobb minta keresése
        if (sharedData->gain == 0.0f) { // Auto Gain mód
            if (abs(vReal[i]) > max_abs_sample_for_auto_gain) {
                max_abs_sample_for_auto_gain = abs(vReal[i]);
            }
        }

        // Időzítés a cél mintavételezési frekvencia eléréséhez
        uint32_t processingTimeMicros = micros() - loopStartTimeMicros;
        if (processingTimeMicros < AudioProcessorConstants::SAMPLE_INTERVAL_US) {
            delayMicroseconds(AudioProcessorConstants::SAMPLE_INTERVAL_US - processingTimeMicros);
        }
    }

    // 2. Erősítés alkalmazása (manuális vagy automatikus)
    if (sharedData->gain > 0.0f) { // Manuális erősítés
        for (int i = 0; i < AudioProcessorConstants::FFT_SIZE; i++) {
            vReal[i] *= sharedData->gain;
        }
    } else if (sharedData->gain == 0.0f) { // Automatikus erősítés
        float target_auto_gain_factor = 1.0f;

        if (max_abs_sample_for_auto_gain > 0.001) {
            target_auto_gain_factor = AudioProcessorConstants::FFT_AUTO_GAIN_TARGET_PEAK / max_abs_sample_for_auto_gain;
            target_auto_gain_factor = constrain(target_auto_gain_factor, AudioProcessorConstants::FFT_AUTO_GAIN_MIN_FACTOR, AudioProcessorConstants::FFT_AUTO_GAIN_MAX_FACTOR);
        }

        // Az erősítési faktor simítása (attack/release karakterisztika)
        if (target_auto_gain_factor < smoothedAutoGainFactor) {
            smoothedAutoGainFactor += AudioProcessorConstants::AUTO_GAIN_ATTACK_COEFF * (target_auto_gain_factor - smoothedAutoGainFactor);
        } else {
            smoothedAutoGainFactor += AudioProcessorConstants::AUTO_GAIN_RELEASE_COEFF * (target_auto_gain_factor - smoothedAutoGainFactor);
        }

        // Erősítés alkalmazása
        for (int i = 0; i < AudioProcessorConstants::FFT_SIZE; i++) {
            vReal[i] *= smoothedAutoGainFactor;
        }
    }

    // 3. Ablakozás, FFT számítás, magnitúdó számítás
    fft.windowing(vReal, AudioProcessorConstants::FFT_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    fft.compute(vReal, vImag, AudioProcessorConstants::FFT_SIZE, FFT_FORWARD);
    fft.complexToMagnitude(vReal, vImag, AudioProcessorConstants::FFT_SIZE);

    // Magnitúdók átmásolása
    for (int i = 0; i < AudioProcessorConstants::FFT_SIZE; i++) {
        magnitudeData[i] = vReal[i];
    }

    // 4. Alacsony frekvenciák csillapítása
    const int attenuation_cutoff_bin = static_cast<int>(AudioProcessorConstants::LOW_FREQ_ATTENUATION_THRESHOLD_HZ / binWidthHz);
    for (int i = 0; i < AudioProcessorConstants::SPECTRUM_BINS; i++) {
        if (i < attenuation_cutoff_bin) {
            magnitudeData[i] /= AudioProcessorConstants::LOW_FREQ_ATTENUATION_FACTOR;
        }
    }

    // 5. Adatok másolása a közös struktúrába
    mutex_enter_blocking(&sharedData->dataMutex);

    // Magas felbontású spektrum
    float maxMagnitude = 0.0f;
    for (int i = 0; i < AudioProcessorConstants::SPECTRUM_BINS; i++) {
        sharedData->spectrum.highResBins[i] = static_cast<float>(magnitudeData[i]);
        if (sharedData->spectrum.highResBins[i] > maxMagnitude) {
            maxMagnitude = sharedData->spectrum.highResBins[i];
        }
    }
    sharedData->spectrum.maxMagnitude = maxMagnitude;

    // Alacsony felbontású spektrum számítása
    calculateLowResSpectrum();

    // Oszcilloszkóp adatok RMS és peak számítása
    if (collectOsciSamples) {
        float rms = 0.0f;
        float peak = 0.0f;
        for (int i = 0; i < AudioProcessorConstants::OSCILLOSCOPE_SAMPLES; i++) {
            float sample = sharedData->oscilloscope.samples[i] - 2048.0f;
            rms += sample * sample;
            if (abs(sample) > peak) {
                peak = abs(sample);
            }
        }
        sharedData->oscilloscope.rms = sqrt(rms / AudioProcessorConstants::OSCILLOSCOPE_SAMPLES);
        sharedData->oscilloscope.peak = peak;
        sharedData->oscilloscope.dataReady = true;

        // Debug info minden 3 másodpercben
        static uint32_t lastOscProcessDebugTime = 0;
        if (millis() - lastOscProcessDebugTime > 3000) {
            DEBUG("AudioProcessor OSC: collected_samples=%d, peak=%.1f, rms=%.1f, sample0=%d, sample10=%d\n", osci_sample_idx, peak, sharedData->oscilloscope.rms, sharedData->oscilloscope.samples[0],
                  sharedData->oscilloscope.samples[10]);
            lastOscProcessDebugTime = millis();
        }
    }

    sharedData->spectrum.dataReady = true;

    mutex_exit(&sharedData->dataMutex);
}

/**
 * @brief Alacsony felbontású spektrum számítása
 */
void AudioProcessor::calculateLowResSpectrum() {
    // Alacsony felbontású spektrum: a magas felbontású spektrum bin-jeit csoportosítjuk
    const int binsPerLowResBin = AudioProcessorConstants::SPECTRUM_BINS / AudioProcessorConstants::LOW_RES_BINS;

    for (int i = 0; i < AudioProcessorConstants::LOW_RES_BINS; i++) {
        float sum = 0.0f;
        int count = 0;

        // Egy alacsony felbontású bin = több magas felbontású bin átlaga
        for (int j = i * binsPerLowResBin; j < (i + 1) * binsPerLowResBin && j < AudioProcessorConstants::SPECTRUM_BINS; j++) {
            sum += sharedData->spectrum.highResBins[j];
            count++;
        }

        sharedData->spectrum.lowResBins[i] = count > 0 ? sum / count : 0.0f;
    }
}

// Core1 globális függvények implementációja
namespace AudioProcessorCore1 {

/**
 * @brief Core1 belépési pont
 */
void core1Entry() {
    DEBUG("AudioProcessorCore1: Core1 indult\n");

    if (g_audioProcessor) {
        g_audioProcessor->processLoop();
    }

    DEBUG("AudioProcessorCore1: Core1 leállt\n");
}

/**
 * @brief Core1 inicializálás
 */
void initializeCore1(SharedAudioData *sharedData) {
    DEBUG("AudioProcessorCore1: Core1 inicializálás\n");

    if (g_audioProcessor) {
        delete g_audioProcessor;
        g_audioProcessor = nullptr;
    }

    // AudioProcessor példány létrehozása
    g_audioProcessor = new AudioProcessor(sharedData, PIN_AUDIO_INPUT);
    if (!g_audioProcessor) {
        DEBUG("AudioProcessorCore1: KRITIKUS - AudioProcessor példány létrehozása sikertelen!\n");
        return;
    }

    // Inicializálás
    g_audioProcessor->initialize();

    // Core1 indítása
    g_core1Running = true;
    multicore_launch_core1(core1Entry);

    DEBUG("AudioProcessorCore1: Core1 inicializálás befejezve\n");
}

/**
 * @brief Core1 leállítás
 */
void shutdownCore1() {
    DEBUG("AudioProcessorCore1: Core1 leállítás\n");

    g_core1Running = false;

    // Várjuk meg, hogy Core1 befejeződjön
    delay(100);

    if (g_audioProcessor) {
        delete g_audioProcessor;
        g_audioProcessor = nullptr;
    }

    DEBUG("AudioProcessorCore1: Core1 leállítás befejeződött\n");
}

/**
 * @brief Audio feldolgozás engedélyezése/tiltása
 */
void setAudioEnabled(bool enabled) {
    g_sharedAudioData.enabled = enabled;
    DEBUG("AudioProcessorCore1: Audio feldolgozás %s\n", enabled ? "engedélyezve" : "tiltva");
}

/**
 * @brief Vizualizációs mód beállítása
 */
void setVisualizationMode(AudioVisualizationType mode) {
    g_sharedAudioData.mode = mode;
    DEBUG("AudioProcessorCore1: Vizualizációs mód: %d\n", (int)mode);
}

/**
 * @brief Sávszűrő frekvenciák beállítása
 */
void setBandFilterFrequencies(float lowFreq, float highFreq) {
    // Jelenleg nincs implementálva a sávszűrő, de a függvény létezik a kompatibilitás kedvéért
    DEBUG("AudioProcessorCore1: Sávszűrő frekvenciák: %.1f Hz - %.1f Hz\n", lowFreq, highFreq);
}

/**
 * @brief Erősítés beállítása
 */
void setGain(float gain) {
    g_sharedAudioData.gain = gain;
    DEBUG("AudioProcessorCore1: Erősítés: %.2f\n", gain);
}

/**
 * @brief Core1 feldolgozás szüneteltetése (EEPROM műveletek előtt)
 */
void pauseCore1() {
    DEBUG("AudioProcessorCore1: Core1 szüneteltetése EEPROM művelethez\n");
    g_core1Running = false;
    delay(50); // Várunk, hogy a Core1 ciklus befejeződjön
}

/**
 * @brief Core1 feldolgozás folytatása (EEPROM műveletek után)
 */
void resumeCore1() {
    DEBUG("AudioProcessorCore1: Core1 folytatása EEPROM művelet után\n");
    g_core1Running = true;
}

} // namespace AudioProcessorCore1
