#include "RttyDecoder.h"
#include "Config.h"
#include "defines.h"
#include "utils.h"
#include <cmath>

RttyDecoder::RttyDecoder() {
    pinMode(LED_BUILTIN, OUTPUT); // Initialize LED_BUILTIN
    clear();
}

/**
 * Minden állapot és változó alaphelyzetbe állítása
 */
void RttyDecoder::clear() {
    // Alapértelmezett frekvenciák betöltése konfigból vagy konstansokból
    markFrequencyHz_ = RTTY_DEFAULT_MARKER_FREQUENCY;
    shiftHz_ = RTTY_DEFAULT_SHIFT_FREQUENCY;
    spaceFrequencyHz_ = markFrequencyHz_ - shiftHz_;

    // Alapértelmezett baud rate
    currentBaudRate_ = RttyBaudRate::BAUD_45;
    autoBaudEnabled_ = true;
    setBaudRate(currentBaudRate_);

    // Állapotgép reset
    currentState_ = RTTY_IDLE;
    bitStartTime_ = 0;
    lastTransitionTime_ = 0;
    currentBitIndex_ = 0;
    receivedBaudotCode_ = 0;

    // Jeladaptáció - nagyon alacsony kezdőértékekkel a maximális érzékenységhez
    adaptiveMarkThreshold_ = 1.5f;  // Nagyon alacsony küszöb a maximális érzékenységhez
    adaptiveSpaceThreshold_ = 1.5f; // Nagyon alacsony küszöb a maximális érzékenységhez
    recentMarkCount_ = 0;
    recentSpaceCount_ = 0;
    recentNoiseCount_ = 0;

    // Baudot character set
    figureMode_ = false; // Kezdés LETTERS módban

    // Dekódolt adatok törlése
    decodedText_ = "";
    newCharacterAdded_ = false;

    // Automatikus baud felismerés inicializálása
    for (int i = 0; i < 6; i++) {
        RttyBaudRate baudRates[] = {RttyBaudRate::BAUD_45, RttyBaudRate::BAUD_50, RttyBaudRate::BAUD_75, RttyBaudRate::BAUD_110, RttyBaudRate::BAUD_150, RttyBaudRate::BAUD_300};
        baudCandidates_[i].baud = baudRates[i];
        baudCandidates_[i].validBitCount = 0;
        baudCandidates_[i].totalBitLength = 0;
        baudCandidates_[i].averageBitLength = 0;
        baudCandidates_[i].confidence = 0.0f;
    }
    bitHistoryIndex_ = 0;
    baudRateDetected_ = false;
    memset(bitTimingHistory_, 0, sizeof(bitTimingHistory_));

    // Statisztikák
    detectedMarksCount_ = 0;
    detectedSpacesCount_ = 0;
    decodedCharactersCount_ = 0;
    errorCount_ = 0;

    DEBUG("[RTTY-CLEAR] RTTY dekóder inicializálva: Mark=%u Hz, Space=%u Hz, Shift=%u Hz, Baud=%u\n", markFrequencyHz_, spaceFrequencyHz_, shiftHz_, static_cast<uint16_t>(currentBaudRate_));
}

/**
 * Mark frekvencia beállítása
 */
void RttyDecoder::setMarkFrequency(uint16_t markHz) {
    markFrequencyHz_ = markHz;
    spaceFrequencyHz_ = markFrequencyHz_ - shiftHz_;
    DEBUG("[RTTY-CONFIG] Mark frekvencia beállítva: %u Hz (Space: %u Hz)\n", markFrequencyHz_, spaceFrequencyHz_);
}

/**
 * Shift frekvencia beállítása
 */
void RttyDecoder::setShiftFrequency(uint16_t shiftHz) {
    shiftHz_ = shiftHz;
    spaceFrequencyHz_ = markFrequencyHz_ - shiftHz_;
    DEBUG("[RTTY-CONFIG] Shift frekvencia beállítva: %u Hz (Space: %u Hz)\n", shiftHz_, spaceFrequencyHz_);
}

/**
 * Baud rate beállítása
 */
void RttyDecoder::setBaudRate(RttyBaudRate baud) {
    currentBaudRate_ = baud;
    float bitTimeMs = 1000.0f / static_cast<float>(baud);
    bitLengthMs_ = static_cast<uint16_t>(bitTimeMs);
    bitLengthTolerance_ = static_cast<uint16_t>(bitTimeMs * 0.5f); // ±50% nagyobb tolerancia

    DEBUG("[RTTY-CONFIG] Baud rate beállítva: %u baud (bit idő: %u ms ± %u ms)\n", static_cast<uint16_t>(baud), bitLengthMs_, bitLengthTolerance_);
}

/**
 * Automatikus baud felismerés engedélyezése/letiltása
 */
void RttyDecoder::enableAutoBaudDetection(bool enabled) {
    autoBaudEnabled_ = enabled;
    if (!enabled) {
        baudRateDetected_ = true; // Kikapcsoljuk a keresést
    }
    DEBUG("[RTTY-CONFIG] Auto baud felismerés: %s\n", enabled ? "BE" : "KI");
}

/**
 * Felismert baud rate lekérdezése
 */
RttyBaudRate RttyDecoder::getDetectedBaudRate() const { return currentBaudRate_; }

/**
 * FFT alapú mark tónus detektálás (csak SNR számításra)
 */
bool RttyDecoder::detectMarkTone(const float *fftData, uint16_t fftSize, float binWidth) {
    float snr = calculateSnrForFrequency(fftData, fftSize, binWidth, markFrequencyHz_);
    return snr > adaptiveMarkThreshold_;
}

/**
 * FFT alapú space tónus detektálás (csak SNR számításra)
 */
bool RttyDecoder::detectSpaceTone(const float *fftData, uint16_t fftSize, float binWidth) {
    float snr = calculateSnrForFrequency(fftData, fftSize, binWidth, spaceFrequencyHz_);
    return snr > adaptiveSpaceThreshold_;
}

/**
 * SNR számítás adott frekvenciához
 */
float RttyDecoder::calculateSnrForFrequency(const float *fftData, uint16_t fftSize, float binWidth, float targetFreq) {
    if (binWidth <= 0.0f || fftSize == 0)
        return 0.0f;

    // Cél frekvencia FFT bin indexe
    uint16_t targetBin = static_cast<uint16_t>(targetFreq / binWidth);
    if (targetBin >= fftSize) {
        DEBUG("[RTTY-SNR] HIBA: targetBin (%u) >= fftSize (%u) - targetFreq=%.2f, binWidth=%.2f\n", targetBin, fftSize, targetFreq, binWidth);
        return 0.0f;
    }

    // Jel erősség a cél frekvencián (3 bin átlagával a pontosság érdekében)
    float signalPower = 0.0f;
    uint16_t signalBinCount = 0;
    for (int i = -1; i <= 1; i++) {
        uint16_t bin = targetBin + i;
        if (bin < fftSize) {
            signalPower += fftData[bin];
            signalBinCount++;
        }
    }
    signalPower /= signalBinCount;

    // Zaj erősség környező frekvenciákon (adaptív ablak mérettel)
    float noisePower = 0.0f;
    uint16_t noiseBinCount = 0;

    // Adaptív ablak méret a frekvencia alapján (alacsony frekvenciáknál kisebb ablak)
    int noiseWindow = (targetFreq < 1500) ? 10 : 15;
    int signalGap = (targetFreq < 1500) ? 3 : 5;

    for (int i = -noiseWindow; i <= noiseWindow; i++) {
        if (abs(i) < signalGap)
            continue; // Kihagyjuk a jel környékét
        uint16_t bin = targetBin + i;
        if (bin < fftSize && bin >= 0) { // Biztonsági ellenőrzés
            noisePower += fftData[bin];
            noiseBinCount++;
        }
    }
    if (noiseBinCount > 0) {
        noisePower /= noiseBinCount;
    } else {
        // Ha nincs elegendő zaj minta, használjunk egy minimális értéket
        noisePower = signalPower * 0.1f; // 10%-os zaj feltételezése
    }

    // SNR számítás (dB-ben)
    if (noisePower > 0.0f) {
        float snr = 20.0f * log10f(signalPower / noisePower);

        // Debug: részletes SNR számítás (csak időnként és csak ha érdekes)
        static unsigned long lastSnrDebugTime = 0;
        unsigned long currentTime = millis();
        if (currentTime - lastSnrDebugTime > 5000 && (signalPower > 0.01f || noisePower > 0.01f)) { // 5 másodpercenként
            DEBUG("[RTTY-SNR] Freq=%s Hz, bin=%u, signal=%s, noise=%s, SNR=%s dB\n", Utils::floatToString(targetFreq).c_str(), targetBin, Utils::floatToString(signalPower).c_str(),
                  Utils::floatToString(noisePower).c_str(), Utils::floatToString(snr).c_str());
            lastSnrDebugTime = currentTime;
        }

        return snr;
    }
    return 0.0f;
}

/**
 * Adaptív SNR küszöb frissítése mark/space külön kezeléssel
 */
void RttyDecoder::updateAdaptiveThreshold(bool toneDetected, float currentSnr, bool isMarkTone) {
    static unsigned long lastAdaptiveUpdate = 0;
    unsigned long currentTime = millis();

    if (toneDetected) {
        if (isMarkTone) {
            recentMarkCount_++;
        } else {
            recentSpaceCount_++;
        }

        // Ha jó SNR-rel detektáljuk a jelet, fokozatosan csökkentsük a megfelelő küszöböt
        float &threshold = isMarkTone ? adaptiveMarkThreshold_ : adaptiveSpaceThreshold_;
        if (currentSnr > threshold + 2.0f) {               // Kisebb margó
            if (currentTime - lastAdaptiveUpdate > 1000) { // 1 másodpercenként
                float oldThreshold = threshold;
                threshold = max(1.0f, threshold - 0.1f); // Még alacsonyabb minimum
                DEBUG("[RTTY-ADAPTIVE] %s küszöb csökkentve: %s -> %s dB (jó SNR: %s dB)\n", isMarkTone ? "Mark" : "Space", Utils::floatToString(oldThreshold, 1).c_str(), Utils::floatToString(threshold, 1).c_str(),
                      Utils::floatToString(currentSnr, 1).c_str());
                lastAdaptiveUpdate = currentTime;
            }
        }
    } else {
        recentNoiseCount_++;
        // Ha túl gyakran nincs detektálás alacsony SNR miatt, csökkentsük a megfelelő küszöböt
        float &threshold = isMarkTone ? adaptiveMarkThreshold_ : adaptiveSpaceThreshold_;
        if (currentSnr > 1.5f && currentSnr < threshold) {                           // Még alacsonyabb alsó határ
            if (currentTime - lastAdaptiveUpdate > 1500 && recentNoiseCount_ > 15) { // Gyakoribb és könnyebb aktiválás
                float oldThreshold = threshold;
                threshold = max(1.0f, threshold - 0.2f); // Még alacsonyabb minimum
                DEBUG("[RTTY-ADAPTIVE] %s küszöb csökkentve: %s -> %s dB (gyakori elutasítás, SNR: %s dB)\n", isMarkTone ? "Mark" : "Space", Utils::floatToString(oldThreshold, 1).c_str(),
                      Utils::floatToString(threshold, 1).c_str(), Utils::floatToString(currentSnr, 1).c_str());
                recentMarkCount_ = recentSpaceCount_ = recentNoiseCount_ = 0;
                lastAdaptiveUpdate = currentTime;
            }
        }
        // Csak akkor emeljük a küszöböt, ha valóban sok a zaj
        else if (recentNoiseCount_ > 100 && (recentMarkCount_ + recentSpaceCount_) < 10) {
            if (currentTime - lastAdaptiveUpdate > 5000) { // 5 másodpercenként
                float oldThreshold = threshold;
                threshold = min(12.0f, threshold + 0.3f);
                DEBUG("[RTTY-ADAPTIVE] %s küszöb emelve: %s -> %s dB (sok zaj)\n", isMarkTone ? "Mark" : "Space", Utils::floatToString(oldThreshold, 1).c_str(), Utils::floatToString(threshold, 1).c_str());
                recentMarkCount_ = recentSpaceCount_ = recentNoiseCount_ = 0;
                lastAdaptiveUpdate = currentTime;
            }
        }
    }

    // Számláló reset időnként
    if (currentTime - lastAdaptiveUpdate > 10000) {
        recentMarkCount_ = recentSpaceCount_ = recentNoiseCount_ = 0;
        lastAdaptiveUpdate = currentTime;
    }
}

/**
 * RTTY FFT adatok feldolgozása
 */
void RttyDecoder::processRttyFftData(const float *fftData, uint16_t fftSize, float binWidth) {
    if (!fftData || fftSize == 0 || binWidth <= 0.0f) {
        DEBUG("[RTTY-FFT] HIBA: Érvénytelen FFT adatok (ptr:%p, size:%u, binWidth:%s)\n", fftData, fftSize, Utils::floatToString(binWidth).c_str());
        return;
    }

    // Debug: FFT adatok információ (időnként)
    static unsigned long lastDebugTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastDebugTime > 2000) { // 2 másodpercenként
        DEBUG("[RTTY-FFT] FFT feldolgozás: size=%u, binWidth=%s Hz, Mark=%u Hz, Space=%u Hz\n", fftSize, Utils::floatToString(binWidth).c_str(), markFrequencyHz_, spaceFrequencyHz_);

        // FFT adatok mintavétele (néhány érték)
        if (fftSize >= 10) {
            DEBUG("[RTTY-FFT] FFT minták: [0]=%s, [%u]=%s, [%u]=%s\n", Utils::floatToString(fftData[0]).c_str(), fftSize / 2, Utils::floatToString(fftData[fftSize / 2]).c_str(), fftSize - 1,
                  Utils::floatToString(fftData[fftSize - 1]).c_str());
        }
        lastDebugTime = currentTime;
    }

    // Mark és Space tónusok detektálása
    float markSnr = calculateSnrForFrequency(fftData, fftSize, binWidth, markFrequencyHz_);
    float spaceSnr = calculateSnrForFrequency(fftData, fftSize, binWidth, spaceFrequencyHz_);

    bool rawMarkPresent = markSnr > adaptiveMarkThreshold_;
    bool rawSpacePresent = spaceSnr > adaptiveSpaceThreshold_;

    // Versengési mechanizmus: ha mindkét tónus detektálódik, a magasabb SNR-ű nyer
    // Ez gyakori jelenség közeli frekvenciáknál (1200/1030 Hz, csak 170 Hz távolság)
    bool markPresent = rawMarkPresent;
    bool spacePresent = rawSpacePresent;

    if (rawMarkPresent && rawSpacePresent) {
        // Mindkét tónus aktívnak tűnik - versengés
        constexpr float SNR_MARGIN = 1.0f; // 1 dB különbség kell a egyértelmű győzelemhez (volt 2.0)

        // Debug kimenet korlátozása
        static unsigned long lastCompeteDebugTime = 0;
        static int competeDebugCount = 0;
        bool shouldDebug = false;

        if (millis() - lastCompeteDebugTime > 1000) { // 1 másodpercenként
            shouldDebug = true;
            lastCompeteDebugTime = millis();
            competeDebugCount = 0;
        } else if (competeDebugCount < 3) { // Vagy az első 3 üzenet másodpercenként
            shouldDebug = true;
            competeDebugCount++;
        }

        if (markSnr > spaceSnr + SNR_MARGIN) {
            // Mark nyeri egyértelműen
            spacePresent = false;
            if (shouldDebug) {
                DEBUG("[RTTY-COMPETE] Mark nyer: Mark SNR=%s > Space SNR=%s + %s\n", Utils::floatToString(markSnr).c_str(), Utils::floatToString(spaceSnr).c_str(), Utils::floatToString(SNR_MARGIN).c_str());
            }
        } else if (spaceSnr > markSnr + SNR_MARGIN) {
            // Space nyeri egyértelműen
            markPresent = false;
            if (shouldDebug) {
                DEBUG("[RTTY-COMPETE] Space nyer: Space SNR=%s > Mark SNR=%s + %s\n", Utils::floatToString(spaceSnr).c_str(), Utils::floatToString(markSnr).c_str(), Utils::floatToString(SNR_MARGIN).c_str());
            }
        } else {
            // Túl közeli SNR értékek - válasszuk az erősebbet
            if (markSnr > spaceSnr) {
                spacePresent = false;
                if (shouldDebug) {
                    DEBUG("[RTTY-COMPETE] Kis különbség, Mark választva: Mark=%s > Space=%s\n", Utils::floatToString(markSnr).c_str(), Utils::floatToString(spaceSnr).c_str());
                }
            } else {
                markPresent = false;
                if (shouldDebug) {
                    DEBUG("[RTTY-COMPETE] Kis különbség, Space választva: Space=%s > Mark=%s\n", Utils::floatToString(spaceSnr).c_str(), Utils::floatToString(markSnr).c_str());
                }
            }
        }
    }

    // SNR értékek debug kiírása (időnként)
    static unsigned long lastMarkDebugTime = 0;
    static unsigned long lastSpaceDebugTime = 0;
    if (currentTime - lastMarkDebugTime > 3000) {
        DEBUG("[RTTY-MARK] SNR=%s dB, küszöb=%s dB, detektált=%s\n", Utils::floatToString(markSnr).c_str(), Utils::floatToString(adaptiveMarkThreshold_).c_str(), markPresent ? "IGEN" : "NEM");
        lastMarkDebugTime = currentTime;
    }
    if (currentTime - lastSpaceDebugTime > 3000) {
        DEBUG("[RTTY-SPACE] SNR=%s dB, küszöb=%s dB, detektált=%s\n", Utils::floatToString(spaceSnr).c_str(), Utils::floatToString(adaptiveSpaceThreshold_).c_str(), spacePresent ? "IGEN" : "NEM");
        lastSpaceDebugTime = currentTime;
    }

    // Adaptív küszöb frissítése
    updateAdaptiveThreshold(markPresent, markSnr, true);
    updateAdaptiveThreshold(spacePresent, spaceSnr, false);

    // LED debug jelzés
    if (config.data.cwRttyLedDebugEnabled) {
        if (markPresent) {
            digitalWrite(LED_BUILTIN, HIGH);
        } else if (spacePresent) {
            digitalWrite(LED_BUILTIN, LOW);
        }
    }

    // Statisztikák frissítése
    if (markPresent) {
        recentMarkCount_++;
        detectedMarksCount_++;
    }
    if (spacePresent) {
        recentSpaceCount_++;
        detectedSpacesCount_++;
    }

    // Jel stabilizáció - csak akkor változtassuk az állapotot, ha a jel stabil
    static bool stableMarkPresent = false;
    static bool stableSpacePresent = false;
    static unsigned long lastSignalChangeTime = 0;
    static int markConfirmCount = 0;
    static int spaceConfirmCount = 0;

    const int CONFIRMATION_THRESHOLD = 2;          // 2 egymás utáni megerősítés (RTTY-hoz elegendő)
    const unsigned long MIN_SIGNAL_DURATION = 100; // minimum 100ms stabil jel (FFT időzítéshez igazítva)

    // Megerősítési számláló frissítése
    if (markPresent && !spacePresent) {
        markConfirmCount++;
        spaceConfirmCount = 0;
    } else if (!markPresent && spacePresent) {
        spaceConfirmCount++;
        markConfirmCount = 0;
    } else {
        markConfirmCount = 0;
        spaceConfirmCount = 0;
    }

    // Stabil állapot meghatározása
    unsigned long timeSinceLastChange = currentTime - lastSignalChangeTime;
    bool shouldUpdateStableState = false;

    if (markConfirmCount >= CONFIRMATION_THRESHOLD && timeSinceLastChange >= MIN_SIGNAL_DURATION) {
        if (!stableMarkPresent || stableSpacePresent) {
            shouldUpdateStableState = true;
            stableMarkPresent = true;
            stableSpacePresent = false;
        }
    } else if (spaceConfirmCount >= CONFIRMATION_THRESHOLD && timeSinceLastChange >= MIN_SIGNAL_DURATION) {
        if (stableMarkPresent || !stableSpacePresent) {
            shouldUpdateStableState = true;
            stableMarkPresent = false;
            stableSpacePresent = true;
        }
    } else if (markConfirmCount == 0 && spaceConfirmCount == 0 && timeSinceLastChange >= MIN_SIGNAL_DURATION * 2) {
        if (stableMarkPresent || stableSpacePresent) {
            shouldUpdateStableState = true;
            stableMarkPresent = false;
            stableSpacePresent = false;
        }
    }

    if (shouldUpdateStableState) {
        lastSignalChangeTime = currentTime;
    }

    // Debug: jelenlét státusz (csak stabil állapot változáskor)
    static bool lastStableMarkPresent = false;
    static bool lastStableSpacePresent = false;
    if (stableMarkPresent != lastStableMarkPresent || stableSpacePresent != lastStableSpacePresent) {
        DEBUG("[RTTY-DETECT] Stabil jel állapot változás: Mark=%s, Space=%s\n", stableMarkPresent ? "VAN" : "NINCS", stableSpacePresent ? "VAN" : "NINCS");
        lastStableMarkPresent = stableMarkPresent;
        lastStableSpacePresent = stableSpacePresent;
    }

    // Automatikus baud felismerés
    if (autoBaudEnabled_ && !baudRateDetected_) {
        updateBaudRateDetection(stableMarkPresent, stableSpacePresent);
    }

    // RTTY állapotgép futtatása stabil jelekkel
    processRttyStateMachine(stableMarkPresent, stableSpacePresent);
}

/**
 * RTTY állapotgép feldolgozása
 */
void RttyDecoder::processRttyStateMachine(bool markPresent, bool spacePresent) {
    unsigned long stateMachineTime = millis();

    // Debug: állapotgép hívás
    static unsigned long lastStateMachineDebug = 0;
    if (stateMachineTime - lastStateMachineDebug > 3000 && (markPresent || spacePresent)) {
        DEBUG("[RTTY-STATE] Állapotgép hívás: Mark=%s, Space=%s, State=%d\n", markPresent ? "VAN" : "NINCS", spacePresent ? "VAN" : "NINCS", (int)currentState_);
        lastStateMachineDebug = stateMachineTime;
    }

    // Csak akkor dolgozunk, ha egyértelműen mark vagy space van jelen
    bool validSignal = (markPresent && !spacePresent) || (!markPresent && spacePresent);

    switch (currentState_) {
        case RTTY_IDLE:
            // Start bit keresése (space jel) - minimális időintervallum a start bitek között
            static unsigned long lastStartBitTime = 0;
            if (!markPresent && spacePresent && (stateMachineTime - lastStartBitTime) > bitLengthMs_) {
                currentState_ = RTTY_START;
                bitStartTime_ = stateMachineTime;
                lastTransitionTime_ = stateMachineTime;
                lastStartBitTime = stateMachineTime;
                currentBitIndex_ = 0;
                receivedBaudotCode_ = 0;
                DEBUG("[RTTY-STATE] Start bit detektálva\n");
            }
            break;

        case RTTY_START:
            // Start bit hosszának ellenőrzése
            if (stateMachineTime - bitStartTime_ >= bitLengthMs_) {
                if (!markPresent && spacePresent) {
                    // Érvényes start bit, lépés a data bithez
                    currentState_ = RTTY_DATA;
                    bitStartTime_ = stateMachineTime;
                    currentBitIndex_ = 0;
                    DEBUG("[RTTY-STATE] Érvényes start bit, adatok fogadása\n");
                } else if (markPresent && !spacePresent) {
                    // Mark jel helyett space-t vártunk - ez valószínűleg nem start bit
                    currentState_ = RTTY_IDLE;
                    errorCount_++;
                    DEBUG("[RTTY-STATE] Hibás start bit (mark helyett space), visszatérés idle-ba\n");
                } else {
                    // Bizonytalan jel - adjunk még egy esélyt, ha nem túl sokáig tart
                    if (stateMachineTime - bitStartTime_ >= bitLengthMs_ * 1.5) {
                        currentState_ = RTTY_IDLE;
                        errorCount_++;
                        DEBUG("[RTTY-STATE] Start bit időtúllépés, visszatérés idle-ba\n");
                    }
                    // Egyébként várunk tovább
                }
            }
            break;

        case RTTY_DATA:
            // 5 adat bit fogadása (LSB először) - bit közepén mintavételezve
            if (stateMachineTime - bitStartTime_ >= bitLengthMs_ * 0.7f) { // 70%-nál mintavétel a stabilitás érdekében
                if (validSignal) {
                    // Bit értékének mentése (mark = 1, space = 0)
                    if (markPresent) {
                        receivedBaudotCode_ |= (1 << currentBitIndex_);
                    }

                    currentBitIndex_++;
                    bitStartTime_ = stateMachineTime;

                    if (currentBitIndex_ >= 5) {
                        // Mind az 5 adat bit megérkezett, lépés a stop bithez
                        currentState_ = RTTY_STOP;
                        DEBUG("[RTTY-STATE] 5 adat bit fogadva: 0x%02X\n", receivedBaudotCode_);
                    }
                } else if (!markPresent && !spacePresent) {
                    // Nincs jel - ez lehet átmenet, várjunk egy kicsit
                    if (stateMachineTime - bitStartTime_ >= bitLengthMs_ * 1.8) { // Több időt adunk
                        // Túl sokáig nincs jel, hibás átvitel
                        discardCurrentBit("Túl sokáig nincs jel az adat bitben");
                    }
                }
                // Egyébként várjunk tovább
            } else {
                // Mindkét jel egyszerre - zajos környezet, próbáljunk továbbmenni
                // A domináló jelet választjuk (ha van SNR alapú információ)
                if (stateMachineTime - bitStartTime_ >= bitLengthMs_ * 1.2) {
                    // Időkorlát túllépve, mégis próbáljuk
                    DEBUG("[RTTY-STATE] Zajos jel, space-t feltételezünk\n");
                    // Space-t feltételezünk (0 bit)
                    currentBitIndex_++;
                    bitStartTime_ = stateMachineTime;

                    if (currentBitIndex_ >= 5) {
                        currentState_ = RTTY_STOP;
                        DEBUG("[RTTY-STATE] 5 adat bit fogadva (zajos): 0x%02X\n", receivedBaudotCode_);
                    }
                }
            }
            break;

        case RTTY_STOP:
            // Stop bit(ek) ellenőrzése (mark jel) - engedékeny módban
            if (stateMachineTime - bitStartTime_ >= bitLengthMs_) {
                char decodedChar = baudotToChar(receivedBaudotCode_);
                bool shouldDecode = false;

                if (markPresent && !spacePresent) {
                    // Érvényes stop bit
                    shouldDecode = true;
                    DEBUG("[RTTY-DECODE] Karakter dekódolva: '%c' (0x%02X)\n", decodedChar, receivedBaudotCode_);
                } else if (!markPresent && !spacePresent) {
                    // Nincs jel - lehet idle time
                    shouldDecode = true;
                    DEBUG("[RTTY-DECODE] Karakter dekódolva (nincs stop bit): '%c' (0x%02X)\n", decodedChar, receivedBaudotCode_);
                } else if (decodedChar != '\0' && (decodedChar >= 'A' && decodedChar <= 'Z')) {
                    // Hibás stop bit, de értelmes betű - mentsük meg
                    shouldDecode = true;
                    DEBUG("[RTTY-DECODE] Karakter dekódolva (hibás stop bit): '%c' (0x%02X)\n", decodedChar, receivedBaudotCode_);
                } else if (decodedChar == ' ' || decodedChar == '\n' || decodedChar == '\r') {
                    // Whitespace karakterek esetén is próbáljuk
                    shouldDecode = true;
                    DEBUG("[RTTY-DECODE] Whitespace karakter dekódolva (hibás stop bit): '%c' (0x%02X)\n", decodedChar, receivedBaudotCode_);
                } else {
                    // Tényleg eldobjuk
                    discardCurrentBit("Hibás stop bit - nem menthető karakter");
                    break;
                }

                // Karakter dekódolása és feldolgozása
                if (shouldDecode && decodedChar != '\0') {
                    decodedText_ += decodedChar;

                    // Szóköz hozzáadása bizonyos karakterek után az olvashatóság javítása érdekében
                    static int charsSinceSpace = 0;
                    charsSinceSpace++;

                    // 8-10 karakter után szóköz hozzáadása, ha még nincs
                    if (charsSinceSpace >= 8 && decodedChar != ' ' && decodedChar != '\n' && decodedChar != '\r') {
                        decodedText_ += ' ';
                        charsSinceSpace = 0;
                    }

                    // Reset a számláló ha természetes szóköz vagy sortörés jön
                    if (decodedChar == ' ' || decodedChar == '\n' || decodedChar == '\r') {
                        charsSinceSpace = 0;
                    }

                    newCharacterAdded_ = true;
                    decodedCharactersCount_++;
                }

                // Visszatérés idle állapotba
                currentState_ = RTTY_IDLE;
            }
            break;
    }
}

/**
 * Automatikus baud felismerés frissítése
 */
void RttyDecoder::updateBaudRateDetection(bool markPresent, bool spacePresent) {
    static unsigned long lastTransition = 0;
    unsigned long currentTime = millis();

    // Állapot változás detektálása
    static bool lastMarkState = false;
    static bool lastSpaceState = false;
    bool transitionDetected = (markPresent != lastMarkState) || (spacePresent != lastSpaceState);

    if (transitionDetected && lastTransition > 0) {
        unsigned long bitLength = currentTime - lastTransition;

        // Csak értelmes bit hosszakat vizsgálunk (10-200ms között)
        if (bitLength >= 10 && bitLength <= 200) {
            // Bit hossz hozzáadása a történethez
            if (bitHistoryIndex_ < 32) {
                bitTimingHistory_[bitHistoryIndex_] = bitLength;
                bitHistoryIndex_++;
            } else {
                // Történet telt, elemzés
                analyzeBitTiming();
                bitHistoryIndex_ = 0;
            }
        }

        lastTransition = currentTime;
    }

    lastMarkState = markPresent;
    lastSpaceState = spacePresent;
}

/**
 * Bit időzítés elemzése a baud rate felismeréshez
 */
void RttyDecoder::analyzeBitTiming() {
    if (bitHistoryIndex_ < 8)
        return; // Túl kevés minta

    // Minden baud rate kandidátushoz pontszám számítása
    for (int i = 0; i < 6; i++) {
        float expectedBitTime = 1000.0f / static_cast<float>(baudCandidates_[i].baud);
        uint16_t validBits = 0;
        float totalDeviation = 0.0f;

        for (int j = 0; j < bitHistoryIndex_; j++) {
            float deviation = abs(bitTimingHistory_[j] - expectedBitTime) / expectedBitTime;
            if (deviation <= 0.3f) { // 30% tolerancia
                validBits++;
                totalDeviation += deviation;
            }
        }

        if (validBits > 0) {
            baudCandidates_[i].validBitCount += validBits;
            baudCandidates_[i].totalBitLength += bitHistoryIndex_;
            baudCandidates_[i].averageBitLength = baudCandidates_[i].totalBitLength > 0 ? expectedBitTime : 0;
            baudCandidates_[i].confidence = static_cast<float>(validBits) / bitHistoryIndex_;
        }
    }

    // Legjobb kandidátus keresése
    float bestConfidence = 0.0f;
    int bestIndex = -1;
    for (int i = 0; i < 6; i++) {
        if (baudCandidates_[i].confidence > bestConfidence && baudCandidates_[i].validBitCount >= 5) {
            bestConfidence = baudCandidates_[i].confidence;
            bestIndex = i;
        }
    }

    // Ha találtunk megfelelő kandidátust (legalább 60% konfidencia)
    if (bestIndex >= 0 && bestConfidence >= 0.6f) {
        setBaudRate(baudCandidates_[bestIndex].baud);
        baudRateDetected_ = true;
        DEBUG("[RTTY-AUTO-BAUD] Baud rate felismert: %u (konfidencia: %s%%)\n", static_cast<uint16_t>(currentBaudRate_), Utils::floatToString(bestConfidence * 100.0f, 1).c_str());
    }
}

/**
 * Bit eldobása hiba esetén
 */
void RttyDecoder::discardCurrentBit(const char *reason) {
    currentState_ = RTTY_IDLE;
    errorCount_++;
    DEBUG("[RTTY-ERROR] Bit eldobva: %s\n", reason);
}

/**
 * Baudot kód konvertálása karakterré
 */
char RttyDecoder::baudotToChar(uint8_t baudotCode) {
    // LETTERS mód karakterek
    static const char lettersTable[32] = {
        '\0', 'E', '\n', 'A',  ' ', 'S', 'I', 'U', // 0x00-0x07
        '\r', 'D', 'R',  'J',  'N', 'F', 'C', 'K', // 0x08-0x0F
        'T',  'Z', 'L',  'W',  'H', 'Y', 'P', 'Q', // 0x10-0x17
        'O',  'B', 'G',  '\0', 'M', 'X', 'V', '\0' // 0x18-0x1F
    };

    // FIGURES mód karakterek
    static const char figuresTable[32] = {
        '\0', '3', '\n', '-',  ' ', '\a', '8', '7', // 0x00-0x07 (\a = bell)
        '\r', '$', '4',  '\'', ',', '!',  ':', '(', // 0x08-0x0F
        '5',  '"', ')',  '2',  '#', '6',  '0', '1', // 0x10-0x17
        '9',  '?', '&',  '\0', '.', '/',  ';', '\0' // 0x18-0x1F
    };

    // Speciális karakterek ellenőrzése
    if (baudotCode == 0x1F) { // LETTERS
        figureMode_ = false;
        return '\0';                 // Nem nyomtatható karakter
    } else if (baudotCode == 0x1B) { // FIGURES
        figureMode_ = true;
        return '\0'; // Nem nyomtatható karakter
    }

    // Karakter táblából való lekérés
    if (baudotCode < 32) {
        return figureMode_ ? figuresTable[baudotCode] : lettersTable[baudotCode];
    }

    return '?'; // Érvénytelen kód
}

/**
 * Dekódolt szöveg visszaadása - csak az új karakterek
 */
String RttyDecoder::getDecodedText() {
    if (!newCharacterAdded_) {
        return "";
    }

    // Új karakterek jelzőjének törlése
    newCharacterAdded_ = false;

    // Utolsó karakter(ek) visszaadása
    String result = decodedText_;
    decodedText_ = ""; // Szöveg puffer ürítése a következő karakterekhez

    return result;
}

/**
 * Manuális szöveg törlés
 */
void RttyDecoder::clearDecodedText() {
    decodedText_ = "";
    newCharacterAdded_ = false;
    DEBUG("[RTTY-CLEAR] Dekódolt szöveg törölve\n");
}