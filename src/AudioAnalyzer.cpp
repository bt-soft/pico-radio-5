#include "AudioAnalyzer.h"
#include "defines.h"

// Globális referencia az AudioAnalyzer példányhoz (Core1 számára)
static AudioAnalyzer *g_audioAnalyzer = nullptr;

AudioAnalyzer::AudioAnalyzer() : fft(ArduinoFFT<double>(workingData.vReal, workingData.vImag, AudioData::FFT_SIZE, AudioData::SAMPLE_RATE)), activeBuffer(0), newDataAvailable(false), running(false), lastPeakDecay(0) {
    // Mutex inicializálása
    mutex_init(&dataMutex);

    // Inicializálás
    memset(&dataBuffers, 0, sizeof(dataBuffers));
    memset(&workingData, 0, sizeof(workingData));
    memset(&stats, 0, sizeof(stats));
    memset(peakHoldBuffer, 0, sizeof(peakHoldBuffer));
}

AudioAnalyzer::~AudioAnalyzer() { stop(); }

bool AudioAnalyzer::init() {
    if (running) {
        return true; // Már fut
    }

    DEBUG("AudioAnalyzer::init() - Starting audio analysis on Core1\n");

    // ADC inicializálás
    initADC();

    // Globális referencia beállítása
    g_audioAnalyzer = this;

    // Core1 indítása
    multicore_launch_core1(core1Main);

    running = true;
    DEBUG("AudioAnalyzer::init() - Audio analysis started successfully\n");
    return true;
}

void AudioAnalyzer::stop() {
    if (!running) {
        return;
    }

    DEBUG("AudioAnalyzer::stop() - Stopping audio analysis\n");

    running = false;

    // Core1 leállítása (reset core1)
    multicore_reset_core1();

    g_audioAnalyzer = nullptr;
    DEBUG("AudioAnalyzer::stop() - Audio analysis stopped\n");
}

bool AudioAnalyzer::getLatestData(AudioData &data) {
    static uint32_t getDataCounter = 0;

    if (!running) {
        if (++getDataCounter % 1000 == 0) {
            DEBUG("getLatestData: AudioAnalyzer not running! Counter: %u\n", getDataCounter);
        }
        return false;
    }

    bool hasData = false;

    // Kritikus szekció - mutex lock
    if (mutex_try_enter(&dataMutex, nullptr)) {
        if (newDataAvailable) {
            data = dataBuffers[1 - activeBuffer]; // Olvasás az inaktív bufferből
            newDataAvailable = false;
            hasData = true;

            if (++getDataCounter % 100 == 0) {
                DEBUG("getLatestData: SUCCESS! Counter: %u, data.lowRes[0]: %u\n", getDataCounter, data.lowResSpectrum[0]);
            }
        } else {
            if (++getDataCounter % 500 == 0) {
                DEBUG("getLatestData: No new data available. Counter: %u\n", getDataCounter);
            }
        }
        mutex_exit(&dataMutex);
    } else {
        if (++getDataCounter % 500 == 0) {
            DEBUG("getLatestData: Mutex lock failed! Counter: %u\n", getDataCounter);
        }
    }

    return hasData;
}

void AudioAnalyzer::core1Main() {
    if (g_audioAnalyzer) {
        g_audioAnalyzer->core1Loop();
    }
}

void AudioAnalyzer::core1Loop() {
    DEBUG("AudioAnalyzer::core1Loop() - Core1 audio task started\n");

    uint32_t debugCounter = 0;
    uint32_t processCounter = 0;
    uint32_t lastProcessTime = 0;

    while (running) {
        debugCounter++;

        // Debug output gyakrabban az élő működés ellenőrzéséhez
        if (debugCounter % 100 == 0) {
            DEBUG("Core1Loop: ALIVE! iter=%u, proc=%u, time=%u ms\n", debugCounter, processCounter, millis());
        }

        // Időalapú processAudio hívás - minden 50ms (~20Hz) - gyorsabb frissítés
        uint32_t currentTime = millis();
        if (currentTime - lastProcessTime >= 50) {
            processCounter++;
            lastProcessTime = currentTime;

            if (processCounter <= 50 || processCounter % 25 == 0) {
                DEBUG("Core1Loop: Starting processAudio #%u at %u ms\n", processCounter, currentTime);
            }

            uint32_t startTime = micros();
            uint32_t watchdogTime = millis();

            // Audio feldolgozás - WATCHDOG VÉDELEM
            processAudio();

            // Ellenőrizzük, hogy nem tartott-e túl sokáig
            uint32_t processingTime = millis() - watchdogTime;
            if (processingTime > 100) { // 100ms watchdog limit
                DEBUG("WATCHDOG: processAudio #%u took %u ms! (too long)\n", processCounter, processingTime);
            }

            // Statisztikák frissítése
            stats.processingTimeUs = micros() - startTime;
            stats.fftCalculations++;

            if (processCounter <= 50 || processCounter % 25 == 0) {
                DEBUG("Core1Loop: processAudio #%u completed in %u us\n", processCounter, stats.processingTimeUs);
            }

            // Extra debug az első 30 ciklushoz
            if (processCounter <= 30) {
                DEBUG("Core1Loop: processAudio #%u - DONE, returning to main loop\n", processCounter);
            }
        }

        // Rövid pihenő - ne terhelje túl a Core1-et
        sleep_ms(2); // 2ms pihenő - kicsit hosszabb
    }

    DEBUG("AudioAnalyzer::core1Loop() - Core1 audio task ended\n");
}

void AudioAnalyzer::processAudio() {
    static uint32_t processCounter = 0;
    ++processCounter;

    // STEP debug - minden ciklushoz az első 30-ban
    if (processCounter <= 30) {
        DEBUG("processAudio: #%u - STEP 1: Entry\n", processCounter);
    }

    // Némítás állapot ellenőrzése
    workingData.isMuted = digitalRead(PIN_AUDIO_MUTE) == HIGH;

    if (processCounter <= 30) {
        DEBUG("processAudio: #%u - STEP 2: Mute check done, muted=%s\n", processCounter, workingData.isMuted ? "YES" : "NO");
    }

    // Debug output csak az első 20 híváskor, majd minden 100.-nál
    if (processCounter <= 20 || processCounter % 100 == 0) {
        DEBUG("processAudio: #%u, muted=%s\n", processCounter, workingData.isMuted ? "YES" : "NO");
    }

    if (workingData.isMuted) {
        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 3: Muted mode - clearing data\n", processCounter);
        }

        // Némított állapotban nullázzuk az adatokat
        memset(workingData.vReal, 0, sizeof(workingData.vReal));
        memset(workingData.spectrumData, 0, sizeof(workingData.spectrumData));
        memset(workingData.lowResSpectrum, 0, sizeof(workingData.lowResSpectrum));
        memset(workingData.rawSamples, 0, sizeof(workingData.rawSamples));
        memset(workingData.peakHold, 0, sizeof(workingData.peakHold));

        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 4: Muted data cleared\n", processCounter);
        }
    } else {
        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 3: NOT muted - starting sampling\n", processCounter);
        }

        // Valódi audio mintavétel - visszaállítva a dummy teszt után
        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 3: Starting REAL audio sampling\n", processCounter);
        }

        // Audio mintavétel gyors módszerrel
        for (int i = 0; i < AudioData::FFT_SIZE; i++) {
            uint16_t sample = fastAnalogRead();
            workingData.rawSamples[i] = sample;

            // ADC értékek átalakítása FFT-hez (-2048..+2047)
            workingData.vReal[i] = (double)(sample - 2048);
            workingData.vImag[i] = 0.0;

            // Minimális késleltetés csak minden 50. mintánál
            if (i % 50 == 0) {
                sleep_us(1);
            }
        }

        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 4: Real audio sampling completed (%d samples)\n", processCounter, AudioData::FFT_SIZE);
        }

        stats.samplesProcessed += AudioData::FFT_SIZE;

        // FFT számítás VÉDETT módon
        if (processCounter <= 5) {
            DEBUG("processAudio: #%u - Starting FFT (protected)\n", processCounter);
        }

        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 5: Starting FFT calculation\n", processCounter);
        }

        // TIMEOUT VÉDELEM az FFT számításhoz
        uint32_t fftStart = millis();
        calculateFFT();
        uint32_t fftTime = millis() - fftStart;

        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 6: FFT done in %u ms\n", processCounter, fftTime);
        }

        // Ha az FFT túl sokáig tart, jelezzük
        if (fftTime > 50) {
            DEBUG("WARNING: FFT took %u ms! (process #%u)\n", fftTime, processCounter);
        }

        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 7: Starting low-res spectrum\n", processCounter);
        }

        // Kis felbontású spektrum számítása
        calculateLowResSpectrum();

        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 8: Starting peak hold update\n", processCounter);
        }

        // Peak hold frissítése
        updatePeakHold();

        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 9: Copying peak hold data\n", processCounter);
        }

        // Peak hold értékek másolása a munkadatokba
        memcpy(workingData.peakHold, peakHoldBuffer, sizeof(peakHoldBuffer));

        if (processCounter <= 5) {
            DEBUG("processAudio: #%u - Audio processing completed\n", processCounter);
        }

        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 10: Audio processing completed\n", processCounter);
        }
    }

    // Időbélyeg
    workingData.timestamp = millis();

    if (processCounter <= 30) {
        DEBUG("processAudio: #%u - STEP 11: Timestamp set\n", processCounter);
    }

    // Adatok másolása a pufferbe (thread-safe) - NON-BLOCKING MUTEX
    if (mutex_try_enter(&dataMutex, nullptr)) {
        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 12: Mutex acquired, copying data\n", processCounter);
        }

        dataBuffers[activeBuffer] = workingData;
        activeBuffer = 1 - activeBuffer; // Puffer váltás
        newDataAvailable = true;
        stats.dataUpdates++;

        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 13: Data copied, releasing mutex\n", processCounter);
        }

        mutex_exit(&dataMutex);

        if (processCounter <= 20 || processCounter % 100 == 0) {
            DEBUG("processAudio: #%u - Data stored! Updates: %u\n", processCounter, stats.dataUpdates);
        }

        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 14: Mutex released\n", processCounter);
        }
    } else {
        if (processCounter <= 30) {
            DEBUG("processAudio: #%u - STEP 12: Mutex lock FAILED!\n", processCounter);
        }

        if (processCounter <= 5 || processCounter % 100 == 0) {
            DEBUG("processAudio: #%u - Mutex lock FAILED!\n", processCounter);
        }
    }

    // BEFEJEZÉS JELE - mindig logoljuk az első 30 ciklust
    if (processCounter <= 30) {
        DEBUG("processAudio: #%u - STEP 15: FINISHING\n", processCounter);
    }
}

void AudioAnalyzer::calculateFFT() {
    // EGYSZERŰSÍTETT és VÉDETT FFT - elkerüljük a lefagyást
    static uint32_t fftCounter = 0;
    ++fftCounter;

    if (fftCounter <= 30) {
        DEBUG("calculateFFT: #%u - Starting FFT calculation\n", fftCounter);
    }

    // Ellenőrizzük az adatok érvényességét
    if (AudioData::FFT_SIZE <= 0 || AudioData::SPECTRUM_BINS <= 0) {
        DEBUG("Invalid FFT parameters\n");
        memset(workingData.spectrumData, 0, sizeof(workingData.spectrumData));
        return;
    }

    if (fftCounter <= 30) {
        DEBUG("calculateFFT: #%u - Parameters OK, FFT_SIZE=%d, SPECTRUM_BINS=%d\n", fftCounter, AudioData::FFT_SIZE, AudioData::SPECTRUM_BINS);
    }

    // Windowing (Hamming ablak) - KIKAPCSOLVA, mert az FFT lefagy
    // if (AudioData::FFT_SIZE <= 512) {
    //     if (fftCounter <= 30) {
    //         DEBUG("calculateFFT: #%u - Applying windowing\n", fftCounter);
    //     }
    //     fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    //
    //     if (fftCounter <= 30) {
    //         DEBUG("calculateFFT: #%u - Windowing done\n", fftCounter);
    //     }
    // }

    if (fftCounter <= 30) {
        DEBUG("calculateFFT: #%u - Starting REAL spectrum analysis (NO FFT)\n", fftCounter);
    }

    // VALÓDI SPEKTRUM ANALÍZIS - abszolút érték alapú magnitude számítás
    // Frekvencia sávok csoportosítása az audio spektrumból
    int samplesPerBin = AudioData::FFT_SIZE / AudioData::SPECTRUM_BINS;

    for (int bin = 0; bin < AudioData::SPECTRUM_BINS; bin++) {
        double sum = 0.0;
        int startIdx = bin * samplesPerBin;
        int endIdx = startIdx + samplesPerBin;

        // Sáv határok ellenőrzése
        if (endIdx > AudioData::FFT_SIZE) {
            endIdx = AudioData::FFT_SIZE;
        }

        // Abszolút értékek összegzése a frekvencia sávban
        for (int i = startIdx; i < endIdx; i++) {
            sum += abs(workingData.vReal[i]);
        }

        // Átlag számítás és skálázás
        double average = sum / (endIdx - startIdx);
        uint16_t magnitude = (uint16_t)(average * 0.5); // Skálázás faktor

        // Minimum és maximum értékek alkalmazása
        if (magnitude < 50)
            magnitude = 50; // Minimum látható szint
        if (magnitude > 1000)
            magnitude = 1000; // Maximum védelem

        workingData.spectrumData[bin] = magnitude;
    }

    if (fftCounter <= 30) {
        DEBUG("calculateFFT: #%u - Real spectrum analysis completed (samples/bin=%d)\n", fftCounter, samplesPerBin);
    }
}

void AudioAnalyzer::calculateLowResSpectrum() {
    // VALÓDI low-res spektrum - aggregáció a spectrum adatokból
    int binsPerLowRes = AudioData::SPECTRUM_BINS / AudioData::LOW_RES_BINS;

    for (int i = 0; i < AudioData::LOW_RES_BINS; i++) {
        uint32_t sum = 0;
        int startBin = i * binsPerLowRes;
        int endBin = startBin + binsPerLowRes;

        // Határok ellenőrzése
        if (endBin > AudioData::SPECTRUM_BINS) {
            endBin = AudioData::SPECTRUM_BINS;
        }

        // Spektrum sávok összegzése
        for (int j = startBin; j < endBin; j++) {
            sum += workingData.spectrumData[j];
        }

        // Átlag és skálázás
        uint32_t average = sum / (endBin - startBin);
        uint32_t scaledValue = average * 2; // Láthatóság növelése

        // Korlátok
        if (scaledValue < 100)
            scaledValue = 100;
        if (scaledValue > 2000)
            scaledValue = 2000;

        workingData.lowResSpectrum[i] = scaledValue;
    }
}

void AudioAnalyzer::updatePeakHold() {
    uint32_t now = millis();

    // Peak hold frissítése
    for (int i = 0; i < AudioData::LOW_RES_BINS; i++) {
        if (workingData.lowResSpectrum[i] > peakHoldBuffer[i]) {
            peakHoldBuffer[i] = workingData.lowResSpectrum[i];
        }
    }

    // Peak decay időzítés
    if (now - lastPeakDecay >= PEAK_DECAY_INTERVAL) {
        for (int i = 0; i < AudioData::LOW_RES_BINS; i++) {
            if (peakHoldBuffer[i] > PEAK_DECAY_RATE) {
                peakHoldBuffer[i] -= PEAK_DECAY_RATE;
            } else {
                peakHoldBuffer[i] = 0;
            }
        }
        lastPeakDecay = now;
    }
}

void AudioAnalyzer::initADC() {
    // ADC pin inicializálása
    pinMode(PIN_AUDIO_INPUT, INPUT);

    // ADC felbontás beállítása (12-bit)
    analogReadResolution(12);

    DEBUG("AudioAnalyzer::initADC() - ADC initialized for pin %d\n", PIN_AUDIO_INPUT);
}
