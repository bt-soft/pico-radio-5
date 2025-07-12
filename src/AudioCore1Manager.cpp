#include "AudioCore1Manager.h"
#include "defines.h"

// Statikus tagváltozók inicializálása
AudioCore1Manager::SharedAudioData *AudioCore1Manager::pSharedData_ = nullptr;
AudioProcessor *AudioCore1Manager::pAudioProcessor_ = nullptr;
bool AudioCore1Manager::initialized_ = false;
float *AudioCore1Manager::currentGainConfigRef_ = nullptr;

/**
 * @brief Core1 audio manager inicializálása
 */
bool AudioCore1Manager::init(float &gainConfigAmRef, float &gainConfigFmRef, int audioPin, double samplingFreq, uint16_t initialFftSize) {
    if (initialized_) {
        DEBUG("AudioCore1Manager: Már inicializálva!\n");
        return false;
    }

    // Megosztott memória allokálása
    pSharedData_ = new (std::nothrow) SharedAudioData();
    if (!pSharedData_) {
        DEBUG("AudioCore1Manager: SharedAudioData allokálás sikertelen!\n");
        return false;
    }

    // Megosztott adatok inicializálása
    memset(pSharedData_, 0, sizeof(SharedAudioData));
    pSharedData_->spectrumDataReady = false;
    pSharedData_->oscilloscopeDataReady = false;
    pSharedData_->core1Running = false;
    pSharedData_->core1ShouldStop = false;
    pSharedData_->configChanged = false;
    pSharedData_->eepromWriteInProgress = false;
    pSharedData_->core1AudioPaused = false;
    pSharedData_->core1AudioPausedAck = false;
    pSharedData_->spectrumSize = initialFftSize;
    pSharedData_->fftGainConfigAm = gainConfigAmRef;
    pSharedData_->fftGainConfigFm = gainConfigFmRef;

    // Mutex inicializálása
    mutex_init(&pSharedData_->dataMutex);

    // Kezdetben AM módra állítjuk
    currentGainConfigRef_ = &gainConfigAmRef;

    DEBUG("AudioCore1Manager: Core1 indítása audio feldolgozáshoz...\n");

    // Core1 indítása
    multicore_launch_core1(core1Entry);

    // Várakozás amíg a core1 elindul
    uint32_t timeout = millis() + 5000; // 5 másodperc timeout
    while (!pSharedData_->core1Running && millis() < timeout) {
        delay(10);
    }

    if (!pSharedData_->core1Running) {
        DEBUG("AudioCore1Manager: Core1 indítás sikertelen (timeout)!\n");
        shutdown();
        return false;
    }

    initialized_ = true;
    DEBUG("AudioCore1Manager: Sikeresen inicializálva!\n");
    return true;
}

/**
 * @brief Core1 audio manager leállítása
 */
void AudioCore1Manager::shutdown() {
    if (!initialized_)
        return;

    DEBUG("AudioCore1Manager: Core1 leállítása...\n");

    if (pSharedData_) {
        pSharedData_->core1ShouldStop = true;

        // Várakozás a core1 leállására
        uint32_t timeout = millis() + 2000; // 2 másodperc timeout
        while (pSharedData_->core1Running && millis() < timeout) {
            delay(10);
        }

        // Memória felszabadítása
        delete pSharedData_;
        pSharedData_ = nullptr;
    }

    initialized_ = false;
    DEBUG("AudioCore1Manager: Leállítva.\n");
}

/**
 * @brief Core1 belépési pont
 */
void AudioCore1Manager::core1Entry() {
    DEBUG("AudioCore1Manager: Core1 audio szál elindult!\n");

    // AudioProcessor inicializálása core1-en
    pAudioProcessor_ = new (std::nothrow) AudioProcessor(*currentGainConfigRef_, PIN_AUDIO_INPUT, AudioProcessorConstants::DEFAULT_SAMPLING_FREQUENCY, pSharedData_->spectrumSize);

    if (!pAudioProcessor_) {
        DEBUG("AudioCore1Manager: Core1 AudioProcessor inicializálás sikertelen!\n");
        pSharedData_->core1Running = false;
        return;
    }

    DEBUG("AudioCore1Manager: Core1 AudioProcessor inicializálva.\n");
    pSharedData_->core1Running = true;

    // Core1 fő ciklus
    core1AudioLoop();

    // Tisztítás
    if (pAudioProcessor_) {
        delete pAudioProcessor_;
        pAudioProcessor_ = nullptr;
    }

    pSharedData_->core1Running = false;
    DEBUG("AudioCore1Manager: Core1 audio szál leállt.\n");
}

/**
 * @brief Core1 audio feldolgozó ciklus
 */
void AudioCore1Manager::core1AudioLoop() {
    uint32_t lastProcessTime = 0;
    const uint32_t processInterval = 50; // 50ms = 20Hz frissítés

    // Audio feldolgozási loop
    while (!pSharedData_->core1ShouldStop) {
        // EEPROM írás esetén várakozás
        if (pSharedData_->eepromWriteInProgress || pSharedData_->core1AudioPaused) {
            // Jelezzük vissza, hogy szüneteltettük az audio feldolgozást
            if (!pSharedData_->core1AudioPausedAck) {
                pSharedData_->core1AudioPausedAck = true;
            }
            delay(1); // Rövid várakozás EEPROM művelet alatt
            continue;
        }

        // Ha kilépünk a pause állapotból, töröljük az ACK flag-et
        if (pSharedData_->core1AudioPausedAck) {
            pSharedData_->core1AudioPausedAck = false;
        }

        uint32_t now = millis();

        // Konfiguráció frissítése szükség esetén
        if (pSharedData_->configChanged) {
            updateAudioConfig();
        }

        // Audio feldolgozás időzített végrehajtása
        if (now - lastProcessTime >= processInterval) {
            // Oszcilloszkóp adatok gyűjtése minden feldolgozásnál
            bool collectOsci = true;

            // Audio feldolgozás végrehajtása
            if (pAudioProcessor_) {
                pAudioProcessor_->process(collectOsci);

                // Thread-safe adatmásolás
                if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
                    // Spektrum adatok másolása
                    const double *magnitudeData = pAudioProcessor_->getMagnitudeData();
                    if (magnitudeData) {
                        uint16_t fftSize = pAudioProcessor_->getFftSize();
                        memcpy(pSharedData_->spectrumBuffer, magnitudeData, fftSize * sizeof(double));
                        pSharedData_->spectrumSize = fftSize;
                        pSharedData_->binWidthHz = pAudioProcessor_->getBinWidthHz();
                        pSharedData_->currentAutoGain = pAudioProcessor_->getCurrentAutoGain();
                        pSharedData_->spectrumDataReady = true;
                    }

                    // Oszcilloszkóp adatok másolása
                    if (collectOsci) {
                        const int *osciData = pAudioProcessor_->getOscilloscopeData();
                        if (osciData) {
                            memcpy(pSharedData_->oscilloscopeBuffer, osciData, AudioProcessorConstants::MAX_INTERNAL_WIDTH * sizeof(int));
                            pSharedData_->oscilloscopeDataReady = true;
                        }
                    }

                    mutex_exit(&pSharedData_->dataMutex);
                }
            }

            lastProcessTime = now;
        }

        // Rövid szünet más szálak számára
        sleep_us(100);
    }
}

/**
 * @brief Audio konfiguráció frissítése
 */
void AudioCore1Manager::updateAudioConfig() {
    if (!pAudioProcessor_)
        return;

    // FFT méret frissítése ha szükséges
    if (pAudioProcessor_->getFftSize() != pSharedData_->spectrumSize) {
        DEBUG("AudioCore1Manager: FFT méret váltása %d-re\n", pSharedData_->spectrumSize);
        pAudioProcessor_->setFftSize(pSharedData_->spectrumSize);
    }

    pSharedData_->configChanged = false;
}

/**
 * @brief Spektrum adatok lekérése (core0-ból hívható)
 */
bool AudioCore1Manager::getSpectrumData(const double **outData, uint16_t *outSize, float *outBinWidth, float *outAutoGain) {
    if (!initialized_ || !pSharedData_)
        return false;

    bool dataAvailable = false;

    if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
        if (pSharedData_->spectrumDataReady) {
            *outData = pSharedData_->spectrumBuffer;
            *outSize = pSharedData_->spectrumSize;
            *outBinWidth = pSharedData_->binWidthHz;
            *outAutoGain = pSharedData_->currentAutoGain;
            pSharedData_->spectrumDataReady = false; // Adat felhasználva
            dataAvailable = true;
        }
        mutex_exit(&pSharedData_->dataMutex);
    }

    return dataAvailable;
}

/**
 * @brief Oszcilloszkóp adatok lekérése (core0-ból hívható)
 */
bool AudioCore1Manager::getOscilloscopeData(const int **outData) {
    if (!initialized_ || !pSharedData_)
        return false;

    bool dataAvailable = false;

    if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
        if (pSharedData_->oscilloscopeDataReady) {
            *outData = pSharedData_->oscilloscopeBuffer;
            pSharedData_->oscilloscopeDataReady = false; // Adat felhasználva
            dataAvailable = true;
        }
        mutex_exit(&pSharedData_->dataMutex);
    }

    return dataAvailable;
}

/**
 * @brief FFT méret váltása (core0-ból hívható)
 */
bool AudioCore1Manager::setFftSize(uint16_t newSize) {
    if (!initialized_ || !pSharedData_)
        return false;

    if (newSize < AudioProcessorConstants::MIN_FFT_SAMPLES || newSize > AudioProcessorConstants::MAX_FFT_SAMPLES) {
        return false;
    }

    // Biztonságos konfiguráció váltás
    if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
        pSharedData_->spectrumSize = newSize;
        pSharedData_->configChanged = true;
        mutex_exit(&pSharedData_->dataMutex);
        return true;
    }

    return false;
}

/**
 * @brief Core1 állapot lekérése
 */
bool AudioCore1Manager::isRunning() { return initialized_ && pSharedData_ && pSharedData_->core1Running; }

/**
 * @brief Debug információk kiírása
 */
void AudioCore1Manager::debugInfo() {
    if (!initialized_ || !pSharedData_) {
        DEBUG("AudioCore1Manager: Nincs inicializálva.\n");
        return;
    }

    DEBUG("AudioCore1Manager Debug Info:\n");
    DEBUG("  Core1 Running: %s\n", pSharedData_->core1Running ? "YES" : "NO");
    DEBUG("  Spectrum Ready: %s\n", pSharedData_->spectrumDataReady ? "YES" : "NO");
    DEBUG("  Osci Ready: %s\n", pSharedData_->oscilloscopeDataReady ? "YES" : "NO");
    DEBUG("  FFT Size: %d\n", pSharedData_->spectrumSize);
    DEBUG("  Bin Width: %.2f Hz\n", pSharedData_->binWidthHz);
    DEBUG("  Auto Gain: %.2f\n", pSharedData_->currentAutoGain);
}

/**
 * @brief Core1 audio feldolgozás szüneteltetése EEPROM műveletekhez
 */
void AudioCore1Manager::pauseCore1Audio() {
    if (!initialized_ || !pSharedData_)
        return;

    DEBUG("AudioCore1Manager: Core1 audio szüneteltetése EEPROM íráshoz...\n");

    mutex_enter_blocking(&pSharedData_->dataMutex);
    pSharedData_->eepromWriteInProgress = true;
    pSharedData_->core1AudioPaused = true;
    pSharedData_->core1AudioPausedAck = false; // Töröljük az ACK flag-et
    mutex_exit(&pSharedData_->dataMutex);

    // Várjuk meg, hogy a Core1 ténylegesen szüneteltesse az audio feldolgozást
    uint32_t timeout = millis() + 100; // 100ms timeout
    while (millis() < timeout) {
        mutex_enter_blocking(&pSharedData_->dataMutex);
        bool ack = pSharedData_->core1AudioPausedAck;
        mutex_exit(&pSharedData_->dataMutex);

        if (ack) {
            DEBUG("AudioCore1Manager: Core1 audio sikeresen szüneteltetve.\n");
            return;
        }
        delay(1);
    }

    DEBUG("AudioCore1Manager: FIGYELEM - Core1 audio szüneteltetés timeout!\n");
}

/**
 * @brief Core1 audio feldolgozás folytatása EEPROM művelet után
 */
void AudioCore1Manager::resumeCore1Audio() {
    if (!initialized_ || !pSharedData_)
        return;

    DEBUG("AudioCore1Manager: Core1 audio folytatása EEPROM írás után.\n");

    mutex_enter_blocking(&pSharedData_->dataMutex);
    pSharedData_->eepromWriteInProgress = false;
    pSharedData_->core1AudioPaused = false;
    mutex_exit(&pSharedData_->dataMutex);
}

/**
 * @brief Core1 audio szüneteltetési állapot lekérdezése
 * @return true ha a Core1 audio szüneteltetve van
 */
bool AudioCore1Manager::isCore1Paused() {
    if (!initialized_ || !pSharedData_)
        return false;

    mutex_enter_blocking(&pSharedData_->dataMutex);
    bool paused = pSharedData_->core1AudioPaused;
    mutex_exit(&pSharedData_->dataMutex);

    return paused;
}
