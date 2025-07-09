#include "AudioProcessor.h"
#include "Config.h"

// Globális közös adat
SharedAudioData g_sharedAudioData;

// Core1 objektumok
static AudioProcessor *g_audioProcessor = nullptr;
static bool g_core1Running = false;

/**
 * @brief AudioProcessor konstruktor
 */
AudioProcessor::AudioProcessor(SharedAudioData *shared)
    : fft(fftReal, fftImag, AudioProcessorConstants::FFT_SIZE, AudioProcessorConstants::SAMPLE_RATE), writeIndex(0), readIndex(0), availableSamples(0), lastSampleTime(0), processingStartTime(0), sharedData(shared),
      dcOffset(0.0f), dcOffsetCalibrated(false) {

    // Mintavételi intervallum számítása
    sampleInterval = 1000000 / AudioProcessorConstants::SAMPLE_RATE; // mikroszekundum

    // Bufferek nullázása
    memset(rawSamples, 0, sizeof(rawSamples));
    memset(fftReal, 0, sizeof(fftReal));
    memset(fftImag, 0, sizeof(fftImag));
    memset(magnitudes, 0, sizeof(magnitudes));
}

/**
 * @brief AudioProcessor destruktor
 */
AudioProcessor::~AudioProcessor() {
    // Cleanup ha szükséges
}

/**
 * @brief Inicializálás
 */
void AudioProcessor::initialize() {
    DEBUG("AudioProcessor: Inicializálás...\n");

    // ADC inicializálás
    analogReadResolution(12); // 12 bites ADC felbontás

    // DC offset kalibráció - a feszültségosztó miatt a nyugalmi szint nem 2048
    calibrateDCOffset();

    // FFT már inicializálva van a konstruktorban

    // Shared data inicializálás
    mutex_init(&sharedData->dataMutex);
    sharedData->enabled = true;
    sharedData->dataReady = false;
    sharedData->mode = AudioVisualizationType::SPECTRUM_LOW_RES;

    // Alapértelmezett sávszűrő beállítások (FM)
    sharedData->currentBandLowFreq = 300.0f;    // 300 Hz
    sharedData->currentBandHighFreq = 15000.0f; // 15 kHz

    // Statisztikák nullázása
    memset(&sharedData->statistics, 0, sizeof(AudioStatistics));
    sharedData->statistics.lastUpdateTime = millis();

    // Spektrum adatok inicializálása
    memset(&sharedData->data, 0, sizeof(AudioVisualizationData));

    DEBUG("AudioProcessor: Inicializálás befejezve. Sample rate: %d Hz, FFT size: %d, DC offset: %d.%d\n", AudioProcessorConstants::SAMPLE_RATE, AudioProcessorConstants::FFT_SIZE, (int)(dcOffset * 1000),
          (int)((dcOffset * 1000 - (int)(dcOffset * 1000)) * 1000));
}

/**
 * @brief Fő feldolgozási ciklus - Core1-en fut
 */
void AudioProcessor::processLoop() {
    uint32_t lastDebugTime = 0;
    uint32_t debugInterval = 5000; // Debug info 5 másodpercenként

    DEBUG("AudioProcessor: Core1 feldolgozási ciklus indult\n");

    while (g_core1Running) {
        processingStartTime = micros();

        // Ellenőrizzük a mute állapotot
        if (rtv::mute) {
            // Mute állapotban nem dolgozunk
            delay(10);
            continue;
        }

        // Ellenőrizzük, hogy engedélyezve van-e az audio feldolgozás
        if (!sharedData->enabled) {
            delay(10);
            continue;
        }

        // Mintavétel
        sampleAudio();

        // Ha elegendő minta van, feldolgozás
        if (getAvailableSamples() >= AudioProcessorConstants::FFT_SIZE) {
            // FFT számítás
            calculateFFT();

            // Vizualizációs adatok feldolgozása a módtól függően
            switch (sharedData->mode) {
                case AudioVisualizationType::SPECTRUM_LOW_RES:
                case AudioVisualizationType::SPECTRUM_HIGH_RES:
                    processSpectrum();
                    break;

                case AudioVisualizationType::OSCILLOSCOPE:
                    processOscilloscope();
                    break;

                case AudioVisualizationType::ENVELOPE:
                    processEnvelope();
                    break;

                case AudioVisualizationType::WATERFALL:
                case AudioVisualizationType::CW_WATERFALL:
                case AudioVisualizationType::RTTY_WATERFALL:
                    processWaterfall();
                    break;
            }

            // Statisztikák frissítése
            updateStatistics();

            // Jelezzük, hogy új adat elérhető
            mutex_enter_blocking(&sharedData->dataMutex);
            sharedData->dataReady = true;
            mutex_exit(&sharedData->dataMutex);
        }

        // Debug információ időszakosan
        uint32_t currentTime = millis();
        if (currentTime - lastDebugTime >= debugInterval) {
            printDebugInfo();
            lastDebugTime = currentTime;
        }

        // Rövid szünet a CPU terhelés csökkentésére
        delayMicroseconds(100);
    }

    DEBUG("AudioProcessor: Core1 feldolgozási ciklus leállt\n");
}

/**
 * @brief Mintavétel és buffer kezelés
 */
void AudioProcessor::sampleAudio() {
    uint32_t currentTime = micros();

    // Ellenőrizzük, hogy itt az ideje a következő mintának
    if (currentTime - lastSampleTime >= sampleInterval) {
        // ADC olvasás PIN_AUDIO_INPUT-ról
        int adcValue = analogRead(PIN_AUDIO_INPUT);

        // DC offset eltávolítása és normalizálás
        // A feszültségosztó miatt a nyugalmi szint nem a középpont
        float sample = ((float)adcValue - dcOffset) / 2048.0f; // Normalizálás -1.0 és 1.0 közé

        // Határértékek ellenőrzése
        if (sample > 1.0f)
            sample = 1.0f;
        if (sample < -1.0f)
            sample = -1.0f;

        // Buffer-be írás
        writeToBuffer(sample);

        lastSampleTime = currentTime;
        sharedData->statistics.samplesProcessed++;
    }
}

/**
 * @brief FFT számítás és spektrum feldolgozás
 */
void AudioProcessor::calculateFFT() {
    // Minták másolása FFT buffer-be
    for (uint16_t i = 0; i < AudioProcessorConstants::FFT_SIZE; i++) {
        fftReal[i] = readFromBuffer();
        fftImag[i] = 0.0f;
    }

    // FFT számítás
    fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(FFTDirection::Forward);
    fft.complexToMagnitude();

    // Nagyságok másolása
    for (uint16_t i = 0; i < AudioProcessorConstants::SPECTRUM_BINS; i++) {
        magnitudes[i] = fftReal[i];
    }

    sharedData->statistics.fftCalculations++;
}

/**
 * @brief Spektrum adatok feldolgozása
 */
void AudioProcessor::processSpectrum() {
    mutex_enter_blocking(&sharedData->dataMutex);

    // Sávszűrő alkalmazása
    applyBandFilter();

    // Alacsony felbontású spektrum létrehozása (binning)
    uint16_t binsPerLowRes = AudioProcessorConstants::SPECTRUM_BINS / AudioProcessorConstants::LOW_RES_BINS;

    for (uint16_t i = 0; i < AudioProcessorConstants::LOW_RES_BINS; i++) {
        float sum = 0.0f;
        for (uint16_t j = 0; j < binsPerLowRes; j++) {
            uint16_t index = i * binsPerLowRes + j;
            if (index < AudioProcessorConstants::SPECTRUM_BINS) {
                sum += magnitudes[index];
            }
        }

        float avgMagnitude = sum / binsPerLowRes;
        float dbValue = magnitudeToDb(avgMagnitude);

        sharedData->data.spectrum.lowResBins[i] = dbValue;
    }

    // Magas felbontású spektrum másolása
    float maxMag = 0.0f;
    for (uint16_t i = 0; i < AudioProcessorConstants::SPECTRUM_BINS; i++) {
        float dbValue = magnitudeToDb(magnitudes[i]);
        sharedData->data.spectrum.highResBins[i] = dbValue;

        if (magnitudes[i] > maxMag) {
            maxMag = magnitudes[i];
        }
    }

    sharedData->data.spectrum.maxMagnitude = magnitudeToDb(maxMag);

    // Peak csökkenés alkalmazása
    applyPeakDecay(sharedData->data.spectrum.lowResPeaks, sharedData->data.spectrum.lowResBins, AudioProcessorConstants::LOW_RES_BINS);
    applyPeakDecay(sharedData->data.spectrum.highResPeaks, sharedData->data.spectrum.highResBins, AudioProcessorConstants::SPECTRUM_BINS);

    mutex_exit(&sharedData->dataMutex);
}

/**
 * @brief Oszcilloszkóp adatok feldolgozása
 */
void AudioProcessor::processOscilloscope() {
    mutex_enter_blocking(&sharedData->dataMutex);

    // Minták másolása oszcilloszkóp buffer-be
    float rmsSum = 0.0f;
    float peakValue = 0.0f;

    for (uint16_t i = 0; i < AudioProcessorConstants::OSCILLOSCOPE_SAMPLES; i++) {
        float sample = readFromBuffer();

        // 12 bites ADC értékké konvertálás vizualizációhoz
        int16_t displayValue = (int16_t)(sample * 2048.0f);
        sharedData->data.oscilloscope.samples[i] = displayValue;

        // RMS és peak számítás
        rmsSum += sample * sample;
        float absSample = fabs(sample);
        if (absSample > peakValue) {
            peakValue = absSample;
        }
    }

    sharedData->data.oscilloscope.rms = sqrt(rmsSum / AudioProcessorConstants::OSCILLOSCOPE_SAMPLES);
    sharedData->data.oscilloscope.peak = peakValue;

    mutex_exit(&sharedData->dataMutex);
}

/**
 * @brief Burkológörbe feldolgozása
 */
void AudioProcessor::processEnvelope() {
    mutex_enter_blocking(&sharedData->dataMutex);

    // Envelope detection egyszerű algoritmussal
    uint16_t samplesPerEnvelope = AudioProcessorConstants::FFT_SIZE / AudioProcessorConstants::ENVELOPE_SAMPLES;

    for (uint16_t i = 0; i < AudioProcessorConstants::ENVELOPE_SAMPLES; i++) {
        float maxValue = 0.0f;

        for (uint16_t j = 0; j < samplesPerEnvelope; j++) {
            float sample = fabs(readFromBuffer());
            if (sample > maxValue) {
                maxValue = sample;
            }
        }

        sharedData->data.envelope.samples[i] = maxValue;
    }

    // Simított jelszint számítása
    float sum = 0.0f;
    for (uint16_t i = 0; i < AudioProcessorConstants::ENVELOPE_SAMPLES; i++) {
        sum += sharedData->data.envelope.samples[i];
    }
    sharedData->data.envelope.smoothedLevel = sum / AudioProcessorConstants::ENVELOPE_SAMPLES;

    mutex_exit(&sharedData->dataMutex);
}

/**
 * @brief Waterfall feldolgozása
 */
void AudioProcessor::processWaterfall() {
    mutex_enter_blocking(&sharedData->dataMutex);

    // Spektrum adatok másolása waterfall sorba
    for (uint16_t i = 0; i < AudioProcessorConstants::SPECTRUM_BINS; i++) {
        float dbValue = magnitudeToDb(magnitudes[i]);

        // dB érték normalizálása 0-255 tartományba
        float normalizedValue = (dbValue - AudioProcessorConstants::NOISE_FLOOR) / (-AudioProcessorConstants::NOISE_FLOOR) * 255.0f;

        // Határok ellenőrzése
        if (normalizedValue < 0.0f)
            normalizedValue = 0.0f;
        if (normalizedValue > 255.0f)
            normalizedValue = 255.0f;

        sharedData->data.waterfall.bins[i] = dbValue;
        sharedData->data.waterfall.waterfallBuffer[sharedData->data.waterfall.currentRow][i] = (uint8_t)normalizedValue;
    }

    // Következő sorra lépés
    sharedData->data.waterfall.currentRow = (sharedData->data.waterfall.currentRow + 1) % AudioProcessorConstants::WATERFALL_HEIGHT;

    mutex_exit(&sharedData->dataMutex);
}

/**
 * @brief Statisztikák frissítése
 */
void AudioProcessor::updateStatistics() {
    uint32_t processingTime = micros() - processingStartTime;
    sharedData->statistics.processingTimeUs = processingTime;

    // CPU használat becslése - korlátozzuk 100%-ra
    // FFT méret / sample rate = egy FFT-hez szükséges idő
    float fftIntervalTime = (float)AudioProcessorConstants::FFT_SIZE / AudioProcessorConstants::SAMPLE_RATE * 1000000.0f; // mikroszekundum
    float rawCpuUsage = ((float)processingTime / fftIntervalTime) * 100.0f;

    // Korlátozzuk 100%-ra, mert nem lehet több mint 100%
    if (rawCpuUsage > 100.0f) {
        sharedData->statistics.cpuUsagePercent = 100.0f;
    } else {
        sharedData->statistics.cpuUsagePercent = rawCpuUsage;
    }

    // Buffer túlcsordulás ellenőrzése
    if (availableSamples >= AudioProcessorConstants::BUFFER_SIZE - 1) {
        sharedData->statistics.bufferOverruns++;
    }

    sharedData->statistics.lastUpdateTime = millis();
}

/**
 * @brief Debug információk kiírása
 */
void AudioProcessor::printDebugInfo() {
    // Jelstatisztikák számítása
    uint16_t nonZeroSamples = 0;
    float peakSample = 0.0f;
    float rmsSum = 0.0f;
    uint16_t sampleCount = getAvailableSamples();

    // Buffer elemzése (utolsó 128 minta)
    uint16_t analyzeCount = (sampleCount > 128) ? 128 : sampleCount;
    uint16_t tempReadIndex = (readIndex + sampleCount - analyzeCount) % AudioProcessorConstants::BUFFER_SIZE;

    for (uint16_t i = 0; i < analyzeCount; i++) {
        float sample = rawSamples[(tempReadIndex + i) % AudioProcessorConstants::BUFFER_SIZE];
        float absSample = fabs(sample);

        if (absSample > 0.001f) { // 0.1% threshold
            nonZeroSamples++;
        }

        if (absSample > peakSample) {
            peakSample = absSample;
        }

        rmsSum += sample * sample;
    }

    float rmsValue = (analyzeCount > 0) ? sqrt(rmsSum / analyzeCount) : 0.0f;

    // Spektrum peak keresése
    float maxSpectrumValue = -120.0f;
    uint16_t peakFreqIndex = 0;

    for (uint16_t i = 1; i < AudioProcessorConstants::SPECTRUM_BINS / 2; i++) { // DC komponens kihagyása
        if (sharedData->data.spectrum.highResBins[i] > maxSpectrumValue) {
            maxSpectrumValue = sharedData->data.spectrum.highResBins[i];
            peakFreqIndex = i;
        }
    }

    float peakFrequency = indexToFrequency(peakFreqIndex);

    DEBUG("=== Audio Processor Stats ===\n");
    DEBUG("Samples processed: %lu\n", (unsigned long)sharedData->statistics.samplesProcessed);
    DEBUG("FFT calculations: %lu\n", (unsigned long)sharedData->statistics.fftCalculations);
    DEBUG("Buffer overruns: %lu\n", (unsigned long)sharedData->statistics.bufferOverruns);
    DEBUG("Processing time: %lu us\n", (unsigned long)sharedData->statistics.processingTimeUs);
    char buffer[10];
    dtostrf(sharedData->statistics.cpuUsagePercent, 4, 2, buffer); // 4: minimum szélesség, 2: tizedesjegyek száma
    DEBUG("CPU usage: %s%%\n", buffer);
    DEBUG("Available samples: %d\n", (int)getAvailableSamples());
    DEBUG("--- Audio Signal Analysis ---\n");
    DEBUG("Non-zero samples: %d/%d (%d%%)\n", (int)nonZeroSamples, (int)analyzeCount, (int)(nonZeroSamples * 100 / (analyzeCount > 0 ? analyzeCount : 1)));
    DEBUG("Peak amplitude: %d.%d (%d dB)\n", (int)(peakSample * 10000), (int)((peakSample * 10000 - (int)(peakSample * 10000)) * 10), (int)(20.0f * log10f(peakSample + 1e-10f)));
    DEBUG("RMS level: %d.%d (%d dB)\n", (int)(rmsValue * 10000), (int)((rmsValue * 10000 - (int)(rmsValue * 10000)) * 10), (int)(20.0f * log10f(rmsValue + 1e-10f)));
    DEBUG("Spectrum peak: %d Hz @ %d dB\n", (int)peakFrequency, (int)maxSpectrumValue);
    DEBUG("Max magnitude: %d dB\n", (int)sharedData->data.spectrum.maxMagnitude);
    DEBUG("Mode: %d, Enabled: %s, Muted: %s\n", (int)sharedData->mode, sharedData->enabled ? "Yes" : "No", rtv::mute ? "Yes" : "No");
    DEBUG("=============================\n");
}

/**
 * @brief Frekvencia index számítás
 */
uint16_t AudioProcessor::frequencyToIndex(float frequency) { return (uint16_t)(frequency * AudioProcessorConstants::FFT_SIZE / AudioProcessorConstants::SAMPLE_RATE); }

/**
 * @brief Index frekvencia számítás
 */
float AudioProcessor::indexToFrequency(uint16_t index) { return (float)index * AudioProcessorConstants::SAMPLE_RATE / AudioProcessorConstants::FFT_SIZE; }

/**
 * @brief Sávszűrő alkalmazása
 */
void AudioProcessor::applyBandFilter() {
    // A sávszűrő beállításokat a SharedAudioData-ból olvassuk
    float lowFreq = sharedData->currentBandLowFreq;
    float highFreq = sharedData->currentBandHighFreq;

    uint16_t lowIndex = frequencyToIndex(lowFreq);
    uint16_t highIndex = frequencyToIndex(highFreq);

    // Sávon kívüli frekvenciák nullázása
    for (uint16_t i = 0; i < lowIndex && i < AudioProcessorConstants::SPECTRUM_BINS; i++) {
        magnitudes[i] = 0.0f;
    }

    for (uint16_t i = highIndex; i < AudioProcessorConstants::SPECTRUM_BINS; i++) {
        magnitudes[i] = 0.0f;
    }
}

/**
 * @brief Simítás alkalmazása
 */
void AudioProcessor::applySmoothing(float *data, uint16_t length, float factor) {
    static float lastValues[AudioProcessorConstants::SPECTRUM_BINS];

    for (uint16_t i = 0; i < length && i < AudioProcessorConstants::SPECTRUM_BINS; i++) {
        lastValues[i] = lastValues[i] * factor + data[i] * (1.0f - factor);
        data[i] = lastValues[i];
    }
}

/**
 * @brief Peak csökkenés alkalmazása
 */
void AudioProcessor::applyPeakDecay(float *peaks, const float *current, uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        if (current[i] > peaks[i]) {
            peaks[i] = current[i]; // Új peak
        } else {
            peaks[i] *= AudioProcessorConstants::PEAK_DECAY_RATE; // Csökkenés
        }
    }
}

/**
 * @brief dB konverzió
 */
float AudioProcessor::magnitudeToDb(float magnitude) {
    if (magnitude <= 0.0f) {
        return AudioProcessorConstants::NOISE_FLOOR;
    }

    float db = 20.0f * log10f(magnitude);

    // Zajszint alatt levágás
    if (db < AudioProcessorConstants::NOISE_FLOOR) {
        db = AudioProcessorConstants::NOISE_FLOOR;
    }

    return db;
}

/**
 * @brief Buffer elérhető minták számítása
 */
uint16_t AudioProcessor::getAvailableSamples() { return availableSamples; }

/**
 * @brief Buffer írása
 */
void AudioProcessor::writeToBuffer(float sample) {
    rawSamples[writeIndex] = sample;
    writeIndex = (writeIndex + 1) % AudioProcessorConstants::BUFFER_SIZE;

    if (availableSamples < AudioProcessorConstants::BUFFER_SIZE) {
        availableSamples++;
    } else {
        // Buffer tele, régi adatok felülírása
        readIndex = (readIndex + 1) % AudioProcessorConstants::BUFFER_SIZE;
        sharedData->statistics.bufferOverruns++;
    }
}

/**
 * @brief Buffer olvasása
 */
float AudioProcessor::readFromBuffer() {
    if (availableSamples == 0) {
        return 0.0f; // Nincs adat
    }

    float sample = rawSamples[readIndex];
    readIndex = (readIndex + 1) % AudioProcessorConstants::BUFFER_SIZE;
    availableSamples--;

    return sample;
}

/**
 * @brief DC offset kalibráció - a feszültségosztó miatt szükséges
 * @details 10k-10k feszültségosztó + 10nF kondenzátor miatt a nyugalmi szint
 * nem a 12 bites ADC középpontja (2048), hanem körülbelül a fél tápfeszültség
 */
void AudioProcessor::calibrateDCOffset() {
    DEBUG("AudioProcessor: DC offset kalibráció...\n");

    // ADC inicializálás biztosítása
    analogReadResolution(12);

    // Több minta átlagolása a pontos offset meghatározásához
    const uint16_t calibrationSamples = 1000;
    uint32_t sum = 0;

    // Várakozás a kondenzátor feltöltődésére
    delay(100);

    // Minták gyűjtése
    for (uint16_t i = 0; i < calibrationSamples; i++) {
        sum += analogRead(PIN_AUDIO_INPUT);
        delayMicroseconds(100); // Rövid várakozás a minták között
    }

    // Átlag számítása
    dcOffset = (float)sum / calibrationSamples;
    dcOffsetCalibrated = true;

    DEBUG("AudioProcessor: DC offset kalibráció befejezve. Offset: %d ADC egység (%d.%d V)\n", (int)dcOffset, (int)(dcOffset * 3.3f / 4096.0f),
          (int)((dcOffset * 3.3f / 4096.0f - (int)(dcOffset * 3.3f / 4096.0f)) * 1000));
}

// ===================================================================
// Core1 globális függvények
// ===================================================================

namespace AudioProcessorCore1 {

/**
 * @brief Core1 belépési pont
 */
void core1Entry() {
    DEBUG("Core1: Audio processor indítása...\n");

    // Objektum létrehozása
    g_audioProcessor = new AudioProcessor(&g_sharedAudioData);

    // Inicializálás
    g_audioProcessor->initialize();

    // Feldolgozási ciklus indítása
    g_core1Running = true;
    g_audioProcessor->processLoop();

    // Cleanup
    delete g_audioProcessor;
    g_audioProcessor = nullptr;

    DEBUG("Core1: Audio processor leállítva\n");
}

/**
 * @brief Core1 inicializálás
 */
void initializeCore1(SharedAudioData *sharedData) {
    DEBUG("Core1: Inicializálás...\n");

    // Közös adatok inicializálása
    if (sharedData == nullptr) {
        sharedData = &g_sharedAudioData;
    }

    // Core1 indítása
    multicore_launch_core1(core1Entry);

    DEBUG("Core1: Elindítva\n");
}

/**
 * @brief Core1 leállítás
 */
void shutdownCore1() {
    DEBUG("Core1: Leállítás...\n");

    g_core1Running = false;

    // Várunk a core1 leállására
    delay(100);

    DEBUG("Core1: Leállítva\n");
}

/**
 * @brief Audio feldolgozás engedélyezése/tiltása
 */
void setAudioEnabled(bool enabled) {
    mutex_enter_blocking(&g_sharedAudioData.dataMutex);
    g_sharedAudioData.enabled = enabled;
    mutex_exit(&g_sharedAudioData.dataMutex);
}

/**
 * @brief Vizualizációs mód beállítása
 */
void setVisualizationMode(AudioVisualizationType mode) {
    mutex_enter_blocking(&g_sharedAudioData.dataMutex);
    g_sharedAudioData.mode = mode;
    mutex_exit(&g_sharedAudioData.dataMutex);
}

/**
 * @brief Sávszűrő frekvenciák beállítása
 */
void setBandFilterFrequencies(float lowFreq, float highFreq) {
    mutex_enter_blocking(&g_sharedAudioData.dataMutex);
    g_sharedAudioData.currentBandLowFreq = lowFreq;
    g_sharedAudioData.currentBandHighFreq = highFreq;
    mutex_exit(&g_sharedAudioData.dataMutex);
}

/**
 * @brief Statisztikák lekérdezése (Core0-ról hívható)
 */
AudioStatistics getStatistics() {
    AudioStatistics stats;
    mutex_enter_blocking(&g_sharedAudioData.dataMutex);
    stats = g_sharedAudioData.statistics;
    mutex_exit(&g_sharedAudioData.dataMutex);
    return stats;
}

/**
 * @brief Debug információk kiírása (Core0-ról hívható)
 */
void printDebugFromCore0() {
    AudioStatistics stats = getStatistics();

    // Spektrum peak keresése (mutex védelemmel)
    float maxSpectrumValue = -120.0f;
    uint16_t peakFreqIndex = 0;
    float maxMagnitude = 0.0f;

    mutex_enter_blocking(&g_sharedAudioData.dataMutex);

    for (uint16_t i = 1; i < AudioProcessorConstants::SPECTRUM_BINS / 2; i++) { // DC komponens kihagyása
        if (g_sharedAudioData.data.spectrum.highResBins[i] > maxSpectrumValue) {
            maxSpectrumValue = g_sharedAudioData.data.spectrum.highResBins[i];
            peakFreqIndex = i;
        }
    }
    maxMagnitude = g_sharedAudioData.data.spectrum.maxMagnitude;

    mutex_exit(&g_sharedAudioData.dataMutex);

    float peakFrequency = (float)peakFreqIndex * AudioProcessorConstants::SAMPLE_RATE / AudioProcessorConstants::FFT_SIZE;

    DEBUG("=== Audio Processor Stats (from Core0) ===\n");
    DEBUG("Samples processed: %lu\n", (unsigned long)stats.samplesProcessed);
    DEBUG("FFT calculations: %lu\n", (unsigned long)stats.fftCalculations);
    DEBUG("Buffer overruns: %lu\n", (unsigned long)stats.bufferOverruns);
    DEBUG("Processing time: %lu us\n", (unsigned long)stats.processingTimeUs);
    DEBUG("CPU usage: %d.%d %%\n", (int)stats.cpuUsagePercent, (int)((stats.cpuUsagePercent - (int)stats.cpuUsagePercent) * 100));
    DEBUG("--- Spectrum Analysis ---\n");
    DEBUG("Spectrum peak: %d Hz @ %d dB\n", (int)peakFrequency, (int)maxSpectrumValue);
    DEBUG("Max magnitude: %d dB\n", (int)maxMagnitude);
    DEBUG("Mode: %d, Enabled: %s, Muted: %s\n", (int)g_sharedAudioData.mode, g_sharedAudioData.enabled ? "Yes" : "No", rtv::mute ? "Yes" : "No");
    DEBUG("Data ready: %s\n", g_sharedAudioData.dataReady ? "Yes" : "No");
    DEBUG("==========================================\n");
}

} // namespace AudioProcessorCore1
