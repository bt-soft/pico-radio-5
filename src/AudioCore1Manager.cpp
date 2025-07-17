#include "AudioCore1Manager.h"
#include "defines.h"
#include "utils.h"

#include <RPi_Pico_TimerInterrupt.h>
extern RPI_PICO_Timer audioCore1ManagerTimer;

// Statikus tagváltozók inicializálása
AudioCore1Manager::SharedAudioData *AudioCore1Manager::pSharedData_ = nullptr;
AudioProcessor *AudioCore1Manager::pAudioProcessor_ = nullptr;
bool AudioCore1Manager::initialized_ = false;
float *AudioCore1Manager::currentGainConfigRef_ = nullptr;
bool AudioCore1Manager::collectOsci_ = false;

// ISR változók a Core1 audio feldolgozás vezérléséhez
volatile bool AudioCore1Manager::isTimerRunning = false;
volatile bool AudioCore1Manager::canRun = false;
volatile float AudioCore1Manager::fftTimerInterval = 0.0f;
volatile uint32_t AudioCore1Manager::interruptCount = 0;

/**
 * @brief Megszakítás kezelő az AudioCore1Manager számára
 * @param timer A megszakítás időzítő
 */
bool AudioCore1Manager::onFftTimerInterrupt(struct repeating_timer *) {
    canRun = true;
    interruptCount++;
    return true;
}

/**
 * @brief Core1 audio feldolgozó inicializálása
 * @param fftSampleSize Az FFT méret
 * @param fftSamplingFrequency A mintavételezési frekvencia
 *
 *  Beállítjuk az időzítőt
 *    idő (ms) = (FFT méret / mintavételezési frekvencia) * 1000
 *             = (2048 / 30000) * 1000
 *             ≈ 68,27 ms
 *
 */
bool AudioCore1Manager::setFFtIsrTimer(uint16_t fftSampleSize, uint16_t fftSamplingFrequency) {

    if (!initialized_) {
        DEBUG("AudioCore1Manager: Core1 még nincs inicializálva!\n");
        return false;
    }

    // Ellenőrizzük a mintavételezési frekvenciát
    if (fftSamplingFrequency == 0) {
        DEBUG("AudioCore1Manager::setIsrTimer -> Érvénytelen mintavételezési frekvencia!\n");
        return false;
    }

    // megszakítás letiltása (ha már futna)
    if (isTimerRunning) {
        ::audioCore1ManagerTimer.detachInterrupt();
    }

    fftTimerInterval = (static_cast<float>(fftSampleSize) / static_cast<float>(fftSamplingFrequency)) * 1000.0f;
    // Megszakítás időzítő beállítása
    bool success = ::audioCore1ManagerTimer.attachInterruptInterval(fftTimerInterval * 1000, AudioCore1Manager::onFftTimerInterrupt);
    if (!success) {
        DEBUG("AudioCore1Manager::setIsrTimer -> HIBA: timer interrupt beállítása sikertelen!\n");
        Utils::beepError();
    } else {
        DEBUG("AudioCore1Manager::setIsrTimer -> Core1 Audio Manager interval: %s ms\n", Utils::floatToString(fftTimerInterval).c_str());
    }

    isTimerRunning = success;

    return success;
}

/**
 * @brief Core1 audio manager inicializálása
 * @param gainConfigAmRef Referencia az AM FFT gain konfigurációra
 * @param gainConfigFmRef Referencia az FM FFT gain konfigurációra
 * @param audioPin Audio bemenet pin száma
 * @param initialSamplingFrequency Kezdeti mintavételezési frekvencia
 * @param initialFftSize Kezdeti FFT méret
 * @return true ha sikeres, false ha hiba történt
 *
 */
bool AudioCore1Manager::init(float &gainConfigAmRef, float &gainConfigFmRef, int audioPin, uint16_t initialSamplingFrequency, uint16_t initialFftSize) {

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
    //
    pSharedData_->fftGainConfigAm = gainConfigAmRef;
    pSharedData_->fftGainConfigFm = gainConfigFmRef;
    pSharedData_->fftSize = initialFftSize;
    pSharedData_->samplingFrequency = initialSamplingFrequency;

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
    pAudioProcessor_ = new (std::nothrow) AudioProcessor(*currentGainConfigRef_, PIN_AUDIO_INPUT, pSharedData_->samplingFrequency, pSharedData_->fftSize);

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

    // Audio feldolgozási loop
    while (!pSharedData_->core1ShouldStop) {
        // EEPROM írás esetén szüneteltetés
        if (pSharedData_->eepromWriteInProgress) {
            mutex_enter_blocking(&pSharedData_->dataMutex);
            pSharedData_->core1AudioPaused = true;
            pSharedData_->core1AudioPausedAck = true;
            mutex_exit(&pSharedData_->dataMutex);
            delay(1);
            continue;
        }

        // Ha kilépünk a pause állapotból, töröljük az ACK flag-et és a paused flag-et
        if (pSharedData_->core1AudioPaused || pSharedData_->core1AudioPausedAck) {
            mutex_enter_blocking(&pSharedData_->dataMutex);
            pSharedData_->core1AudioPaused = false;
            pSharedData_->core1AudioPausedAck = false;
            mutex_exit(&pSharedData_->dataMutex);
        }

        // Konfiguráció frissítése szükség esetén
        if (pSharedData_->configChanged) {
            updateAudioConfig();
            continue;
        }

        // Audio feldolgozás végrehajtása
        if (pAudioProcessor_ && canRun) {

            // DEBUG("AudioCore1Manager: Core1 audio feldolgozás futtatása, interruptCount: %d\n", interruptCount);

            uint32_t startTime = micros();

            // Minták gyújtése, oszcilloszkóp minták gyűjtése, ha engedélyezve van
            pAudioProcessor_->process(collectOsci_);
            uint32_t processTime = micros();

            // Thread-safe adatmásolás
            if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
                // Spektrum adatok másolása
                const double *magnitudeData = pAudioProcessor_->getMagnitudeData();
                if (magnitudeData) {
                    uint16_t fftSize = pAudioProcessor_->getFftSize();
                    memcpy(pSharedData_->spectrumBuffer, magnitudeData, fftSize * sizeof(double));
                    pSharedData_->binWidthHz = pAudioProcessor_->getBinWidthHz();
                    pSharedData_->currentAutoGain = pAudioProcessor_->getCurrentAutoGain();
                    pSharedData_->spectrumDataReady = true;

                    // Ha nincs épp aktív konfigurációs beállítás, akkor lekérjük az aktuális beállításokat is
                    if (!pSharedData_->configChanged) {
                        pSharedData_->fftSize = fftSize;
                        pSharedData_->samplingFrequency = pAudioProcessor_->getSamplingFrequency();
                    }
                }

                // Oszcilloszkóp adatok másolása, ha engedélyezve van
                if (collectOsci_) {
                    const int *osciData = pAudioProcessor_->getOscilloscopeData();
                    int osciSampleCount = pAudioProcessor_->getOscilloscopeSampleCount();
                    if (osciData && osciSampleCount > 0) {
                        memcpy(pSharedData_->oscilloscopeBuffer, osciData, osciSampleCount * sizeof(int));
                        pSharedData_->oscilloscopeSampleCount = osciSampleCount;
                        pSharedData_->oscilloscopeDataReady = true;
                    }
                }

                mutex_exit(&pSharedData_->dataMutex);
            }

            uint32_t copyTime = micros();

            if (interruptCount % 100 == 0) {
                DEBUG("AudioCore1Manager::core1AudioLoop ->  fftTimerInterval: %s ms, processTime: %s, copyTime: %s, allTime: %s\n", //
                      Utils::floatToString(fftTimerInterval).c_str(),                                                                //
                      Utils::elapsedUSecStr(startTime, processTime).c_str(),                                                         //
                      Utils::elapsedUSecStr(processTime, copyTime).c_str(),                                                          //
                      Utils::elapsedUSecStr(startTime).c_str());
            }

            // Futtatás flag resetelése
            canRun = false;
        }

        // Rövid szünet más szálak számára
        sleep_us(100);
    }
}

/**
 * @brief Core1 Oszcilloszkóp adatok gyűjtésének vezérlése
 * @param collectOsci true ha oszcilloszkóp mintákat kell gyűjteni, false ha nem
 */
void AudioCore1Manager::setCollectOsci(bool collectOsci) {
    if (!initialized_) {
        return;
    }
    collectOsci_ = collectOsci;
}

/**
 * @brief  Core1 Oszcilloszkóp adatok gyűjtésének lekérése
 * @return true ha oszcilloszkóp mintákat gyűjtünk, false ha nem
 */
bool AudioCore1Manager::getCollectOsci() {
    if (!initialized_) {
        return false;
    }
    return collectOsci_;
}

/**
 * @brief FFT mintavételezési frekvencia  váltása (core0-ból hívható)
 * @param newSamplingFrequency Az új FFT mintavételezési frekvencia
 * @return true ha sikeres, false egyébként
 *
 * A ScreenAM::handleAfBWButton()-ból van hívva
 */
bool AudioCore1Manager::setSamplingFrequency(uint16_t newSamplingFrequency) {
    if (!initialized_ || !pSharedData_) {
        return false;
    }

    if (newSamplingFrequency < AudioProcessorConstants::MIN_SAMPLING_FREQUENCY || newSamplingFrequency > AudioProcessorConstants::MAX_SAMPLING_FREQUENCY) {
        DEBUG("AudioProcessor::setSamplingFrequency: Érvénytelen mintavételezési frekvencia  %d\n", newSamplingFrequency);
        return false;
    }

    // DEBUG("AudioCore1Manager::setSamplingFrequency: Mintavételezési frekvencia beállítása %d Hz-re\n", newSamplingFrequency);

    // Biztonságos konfiguráció váltás
    if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
        pSharedData_->samplingFrequency = newSamplingFrequency;
        pSharedData_->configChanged = true;
        mutex_exit(&pSharedData_->dataMutex);
        return true;
    }

    return false;
}

/**
 * @brief FFT méret váltása (core0-ból hívható)
 * @param newSize Az új FFT méret
 * @return true ha sikeres, false egyébként
 */
bool AudioCore1Manager::setFftSize(uint16_t newSize) {
    if (!initialized_ || !pSharedData_)
        return false;

    // Timer ki- bekapcsolása, a méret 0-ra állítása esetén
    if (newSize == 0) {
        isTimerRunning = false;
        ::audioCore1ManagerTimer.disableTimer();
        DEBUG("AudioCore1Manager::setFftSize: TIMER Kikapcsolva!\n");
        return true;

    } else if (!isTimerRunning && newSize > 0) {
        isTimerRunning = false;
        ::audioCore1ManagerTimer.enableTimer();
        DEBUG("AudioCore1Manager::setFftSize: TIMER Bekapcsolva!\n");
    }

    if (newSize < AudioProcessorConstants::MIN_FFT_SAMPLES || newSize > AudioProcessorConstants::MAX_FFT_SAMPLES) {
        DEBUG("AudioProcessor::setFftSize: Érvénytelen FFT méret %d\n", newSize);
        return false;
    }

    // DEBUG("AudioCore1Manager::setFftSize: FFT méret beállítása %d-re\n", newSize);

    // Biztonságos konfiguráció váltás
    if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
        pSharedData_->fftSize = newSize;
        pSharedData_->configChanged = true;
        mutex_exit(&pSharedData_->dataMutex);
        return true;
    }

    return false;
}

/**
 * @brief Audio konfiguráció frissítése
 */
void AudioCore1Manager::updateAudioConfig() {

    if (!pAudioProcessor_) {
        return;
    }

    // DEBUG("AudioCore1Manager::updateAudioConfig: Audio konfiguráció frissítése...\n");

    // FFT méret frissítése ha szükséges
    if (pSharedData_->fftSize != 0 && pAudioProcessor_->getFftSize() != pSharedData_->fftSize) {
        DEBUG("AudioCore1Manager::updateAudioConfig: FFT méret váltása %d-re\n", pSharedData_->fftSize);
        pAudioProcessor_->setFftSize(pSharedData_->fftSize);
    }

    // FFT mintavételezési frekvencia frissítése ha szükséges
    if (pSharedData_->samplingFrequency != 0 && pAudioProcessor_->getSamplingFrequency() != pSharedData_->samplingFrequency) {
        DEBUG("AudioCore1Manager:updateAudioConfig: FFT frekvencia váltása %d-re\n", pSharedData_->samplingFrequency);
        pAudioProcessor_->setSamplingFrequency(pSharedData_->samplingFrequency);
    }

    pSharedData_->configChanged = false;

    setFFtIsrTimer(pSharedData_->fftSize, pSharedData_->samplingFrequency);
}

/**
 * @brief Spektrum méret lekérése (ha nincs friss adat, cached érték)
 * @param outFftSize Kimeneti FFT méret
 * @return true ha sikeres, false ha hiba történt
 */
bool AudioCore1Manager::getFftSize(uint16_t *outFftSize) {
    if (!initialized_ || !pSharedData_) {
        return false;
    }

    bool dataAvailable = false;

    if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
        if (pSharedData_->spectrumDataReady) {
            *outFftSize = pSharedData_->fftSize;
            pSharedData_->spectrumDataReady = false; // Adat felhasználva
            dataAvailable = true;
        }
        mutex_exit(&pSharedData_->dataMutex);
    }

    return dataAvailable;
}
/**
 * @brief Core1 mintavételezési frekvencia lekérése (ha nincs friss adat, cached érték)
 * @param outSampleFrequency Kimeneti mintavételezési frekvencia
 * @return true ha sikeres, false ha hiba történt
 */
bool AudioCore1Manager::getFftSampleFrequency(uint16_t *outSampleFrequency) {
    if (!initialized_ || !pSharedData_) {
        DEBUG("AudioCore1Manager::getFftSampleFrequency: Nem inicializálva vagy nincs megosztott adat!\n");
        return false;
    }

    bool dataAvailable = false;

    if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
        if (pSharedData_->spectrumDataReady) {
            *outSampleFrequency = pSharedData_->samplingFrequency;
            pSharedData_->spectrumDataReady = false; // Adat felhasználva
            dataAvailable = true;
        }
        mutex_exit(&pSharedData_->dataMutex);
    }
    // DEBUG("AudioCore1Manager::getFftSampleFrequency: Mintavételezési frekvencia lekérdezés %s, érték: %d\n", dataAvailable ? "sikeres" : "nem sikerült", (int)*outSampleFrequency);

    return dataAvailable;
}

/**
 * @brief Core1 aktuális bin szélesség lekérése (ha nincs friss adat, cached érték)
 * @param outBinWidth Kimeneti bin szélesség Hz-ben
 * @return true ha sikeres, false ha hiba történt
 */
bool AudioCore1Manager::getFftCurrentBinWidth(float *outBinWidth) {
    if (!initialized_ || !pSharedData_) {
        return false;
    }

    bool dataAvailable = false;

    if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
        if (pSharedData_->spectrumDataReady) {
            *outBinWidth = pSharedData_->binWidthHz;
            pSharedData_->spectrumDataReady = false; // Adat felhasználva
            dataAvailable = true;
        }
        mutex_exit(&pSharedData_->dataMutex);
    }

    return dataAvailable;
}

/**
 * @brief Spektrum adatok lekérése (core0-ból hívható)
 * @param outData Kimeneti buffer a spektrum adatoknak
 * @param outFftSize Kimeneti FFT méret
 * @param outBinWidth Kimeneti bin szélesség Hz-ben
 * @param outAutoGain Jelenlegi auto gain faktor
 * @return true ha friss adat érhető el, false egyébként
 */
bool AudioCore1Manager::getSpectrumData(const double **outData, uint16_t *outFftSize, float *outBinWidth, float *outAutoGain) {

    if (!initialized_ || !pSharedData_) {
        return false;
    }

    bool dataAvailable = false;

    if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
        if (pSharedData_->spectrumDataReady) {
            *outData = pSharedData_->spectrumBuffer;
            *outFftSize = pSharedData_->fftSize;
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
bool AudioCore1Manager::getOscilloscopeData(const int **outData, int *outSampleCount) {
    if (!initialized_ || !pSharedData_ || !collectOsci_)
        return false;

    bool dataAvailable = false;

    if (mutex_try_enter(&pSharedData_->dataMutex, nullptr)) {
        if (pSharedData_->oscilloscopeDataReady) {
            *outData = pSharedData_->oscilloscopeBuffer;
            *outSampleCount = pSharedData_->oscilloscopeSampleCount;
            pSharedData_->oscilloscopeDataReady = false; // Adat felhasználva
            dataAvailable = true;
        }
        mutex_exit(&pSharedData_->dataMutex);
    }
    return dataAvailable;
}

/**
 * @brief Core1 állapot lekérése
 */
bool AudioCore1Manager::isRunning() { return initialized_ && pSharedData_ && pSharedData_->core1Running; }

/**
 * @brief Core1 audio feldolgozás szüneteltetése EEPROM műveletekhez
 */
void AudioCore1Manager::pauseCore1Audio() {
    if (!initialized_ || !pSharedData_)
        return;

    DEBUG("AudioCore1Manager: Core1 audio szüneteltetése EEPROM íráshoz...\n");

    // Csak az eepromWriteInProgress flag-et állítsuk itt, a többit Core1 állítja be
    mutex_enter_blocking(&pSharedData_->dataMutex);
    pSharedData_->eepromWriteInProgress = true;
    pSharedData_->core1AudioPausedAck = false; // Töröljük az ACK flag-et
    mutex_exit(&pSharedData_->dataMutex);

    // Várjuk meg, hogy a Core1 ténylegesen szüneteltesse az audio feldolgozást
    uint32_t timeout = millis() + 200; // 200ms timeout
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

/**
 * @brief Debug információk kiírása
 */
void AudioCore1Manager::debugInfo() {
    if (!initialized_ || !pSharedData_) {
        DEBUG("AudioCore1Manager: Nincs inicializálva.\n");
        return;
    }

    DEBUG("AudioCore1Manager Debug Info:\n");
    DEBUG("  Core1 Running: %s, Spectrum Ready: %s, Osci Ready: %s\n", pSharedData_->core1Running ? "YES" : "NO", pSharedData_->spectrumDataReady ? "YES" : "NO", pSharedData_->oscilloscopeDataReady ? "YES" : "NO");
    DEBUG("  FFT Sample Freq: %dkHz\n", pSharedData_->samplingFrequency / 1000);
    DEBUG("  FFT Size: %d\n", pSharedData_->fftSize);
    DEBUG("  Bin Width: %s Hz\n", Utils::floatToString(pSharedData_->binWidthHz).c_str());
    DEBUG("  Auto Gain: %s\n", Utils::floatToString(pSharedData_->currentAutoGain).c_str());
}
