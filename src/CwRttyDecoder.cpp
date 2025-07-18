#include "CwRttyDecoder.h"
#include "Config.h"
#include "utils.h"
#include "defines.h"
#include <cmath>

const char CwRttyDecoder::MORSE_TREE_SYMBOLS[] = {
    ' ', '5', ' ', 'H', ' ',  '4', ' ', 'S', // 0
    ' ', ' ', ' ', 'V', ' ',  '3', ' ', 'I', // 8
    ' ', ' ', ' ', 'F', ' ',  ' ', ' ', 'U', // 16
    '?', ' ', '_', ' ', ' ',  '2', ' ', 'E', // 24
    ' ', '&', ' ', 'L', '"',  ' ', ' ', 'R', // 32
    ' ', '+', '.', ' ', ' ',  ' ', ' ', 'A', // 40
    ' ', ' ', ' ', 'P', '@',  ' ', ' ', 'W', // 48
    ' ', ' ', ' ', 'J', '\'', '1', ' ', ' ', // 56
    ' ', '6', '-', 'B', ' ',  '=', ' ', 'D', // 64
    ' ', '/', ' ', 'X', ' ',  ' ', ' ', 'N', // 72
    ' ', ' ', ' ', 'C', ';',  ' ', '!', 'K', // 80
    ' ', '(', ')', 'Y', ' ',  ' ', ' ', 'T', // 88
    ' ', '7', ' ', 'Z', ' ',  ' ', ',', 'G', // 96
    ' ', ' ', ' ', 'Q', ' ',  ' ', ' ', 'M', // 104
    ':', '8', ' ', ' ', ' ',  ' ', ' ', 'O', // 112
    ' ', '9', ' ', ' ', ' ',  '0', ' ', ' '  // 120
};

CwRttyDecoder::CwRttyDecoder() { clear(); }

void CwRttyDecoder::clear() {
    decodedText = "";
    peakFrequencyHz = 0.0f;
    peakMagnitude = 0.0f;
    noiseLevel_ = 0.0f;
    signalThreshold_ = 1000.0f;

    // Adaptív változók inicializálása
    startReferenceMs_ = 120;
    currentReferenceMs_ = startReferenceMs_;
    leadingEdgeTimeMs_ = 0;
    trailingEdgeTimeMs_ = 0;
    toneIndex_ = 0;
    toneMaxDurationMs_ = 0L;
    toneMinDurationMs_ = 9999L;
    decoderStarted_ = false;
    measuringTone_ = false;
    lastActivityMs_ = 0;
    lastDecodedChar_ = '\0';
    wordSpaceProcessed_ = false;
    lastSpaceDebugMs_ = 0;
    inInactiveState = false;
    resetMorseTree();
    memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
}

String CwRttyDecoder::getDecodedText() {
    String inProgressMorse = "";
    if (toneIndex_ > 0) {
        for (uint8_t i = 0; i < toneIndex_; i++) {
            if (rawToneDurations_[i] < currentReferenceMs_) {
                inProgressMorse += ".";
            } else {
                inProgressMorse += "-";
            }
        }
    }
    return decodedText + inProgressMorse;
}

void CwRttyDecoder::processFftData(const float *fftData, uint16_t fftSize, float binWidth) {

    // 1. Peak detektálás a CW sávban
    // A keresési tartományt dinamikusan határozzuk meg a beállított CW offset köré.
    uint16_t centerFreqHz = config.data.cwReceiverOffsetHz;
    constexpr uint16_t SEARCH_WINDOW_HZ = 200; // +/- 200 Hz a keresési ablak (toleránsabb)

    // Biztonsági ellenőrzés, hogy a kezdő frekvencia ne legyen negatív
    uint16_t startFreqHz = (centerFreqHz > SEARCH_WINDOW_HZ) ? (centerFreqHz - SEARCH_WINDOW_HZ) : 0;
    uint16_t endFreqHz = centerFreqHz + SEARCH_WINDOW_HZ;

    int startBin = static_cast<int>(startFreqHz / binWidth);
    int endBin = static_cast<int>(endFreqHz / binWidth);

    // Bin indexek korlátozása az érvényes tartományon belül
    if (endBin >= (int)fftSize / 2) {
        endBin = (fftSize / 2) - 1;
    }

    float maxMagnitude = 0.0f;
    int peakBin = -1;
    for (int i = startBin; i <= endBin; ++i) {
        if (fftData[i] > maxMagnitude) {
            maxMagnitude = fftData[i];
            peakBin = i;
        }
    }
    peakMagnitude = maxMagnitude;
    peakFrequencyHz = (peakBin != -1) ? (peakBin * binWidth) : 0.0f;

    // 2. Zajszint becslése
    float noiseSum = 0.0f;
    int noiseBinCount = 0;
    int noiseWindow = 50;
    int noiseGap = 10;
    for (int i = startBin - noiseWindow; i < startBin - noiseGap; ++i) {
        if (i >= 0) {
            noiseSum += fftData[i];
            noiseBinCount++;
        }
    }
    for (int i = endBin + noiseGap; i < endBin + noiseWindow; ++i) {
        if (i < (int)fftSize / 2) {
            noiseSum += fftData[i];
            noiseBinCount++;
        }
    }
    float averageNoise = (noiseBinCount > 0) ? (noiseSum / noiseBinCount) : 1.0f;
    if (noiseLevel_ == 0.0f) {
        noiseLevel_ = averageNoise;
    } else {
        noiseLevel_ += NOISE_SMOOTHING_FACTOR * (averageNoise - noiseLevel_);
    }

    // 3. Dinamikus küszöb beállítása
    signalThreshold_ = noiseLevel_ * NOISE_FLOOR_FACTOR;
    if (signalThreshold_ < MINIMUM_THRESHOLD) {
        signalThreshold_ = MINIMUM_THRESHOLD;
    }

    // =================================================================
    // Itt kezdődik a CwDecoder.cpp-ből átvett állapotgép logika
    // =================================================================
    bool currentToneState = (peakBin != -1 && peakMagnitude > signalThreshold_);
    unsigned long currentTimeMs = millis();
    char decodedChar = '\0';

    // Dinamikus szünetek és zajküszöb számítása
    unsigned long elementGapMinMs = DOT_MIN_MS / 2;
    if (toneMinDurationMs_ != 9999L && toneMinDurationMs_ > 0) {
        elementGapMinMs = max(DOT_MIN_MS / 2, toneMinDurationMs_ / 4);
    }

    unsigned long estimatedDotLength = (toneMinDurationMs_ == 9999L || toneMinDurationMs_ == 0) ? (currentReferenceMs_ / 2) : toneMinDurationMs_;
    if (estimatedDotLength < DOT_MIN_MS || currentReferenceMs_ == 0)
        estimatedDotLength = DOT_MIN_MS;

    unsigned long charGapMs = max(MIN_CHAR_GAP_MS_FALLBACK, (unsigned long)(estimatedDotLength * CHAR_GAP_DOT_MULTIPLIER));
    unsigned long wordGapMs = max(MIN_WORD_GAP_MS_FALLBACK, (unsigned long)(estimatedDotLength * WORD_GAP_DOT_MULTIPLIER));
    if (wordGapMs <= charGapMs)
        wordGapMs = charGapMs + max(1UL, MIN_CHAR_GAP_MS_FALLBACK / 2);

    if (currentToneState) {
        lastActivityMs_ = currentTimeMs;
        if (!measuringTone_) {
            wordSpaceProcessed_ = false;
        }
    }

    if (currentTimeMs - lastActivityMs_ > MAX_SILENCE_MS && decoderStarted_) {
        if (!inInactiveState) {
            clear();
            DEBUG("CW: Reset tétlenség (%lu ms) miatt\n", MAX_SILENCE_MS);
            inInactiveState = true;
        }
        return;
    }

    if (!decoderStarted_ && !measuringTone_ && currentToneState) {
        leadingEdgeTimeMs_ = currentTimeMs;
        decoderStarted_ = true;
        inInactiveState = false;
        measuringTone_ = true;
        wordSpaceProcessed_ = false;
    } else if (decoderStarted_ && measuringTone_ && !currentToneState) {
        trailingEdgeTimeMs_ = currentTimeMs;
        unsigned long duration = trailingEdgeTimeMs_ - leadingEdgeTimeMs_;

        if (toneIndex_ >= MAX_CW_ELEMENTS) {
            DEBUG("CW: Tömb tele (%d elem), kényszer dekódolás hang végén\n", toneIndex_);
            decodedChar = processCollectedElements();
        }

        // Dinamikus zajszűrés
        unsigned long dynamicMinDuration = DOT_MIN_MS;
        if (toneMinDurationMs_ != 9999L && toneMinDurationMs_ > 0) {
            dynamicMinDuration = max(MIN_ADAPTIVE_DOT_MS, toneMinDurationMs_ / NOISE_THRESHOLD_FACTOR);
        }

        if (duration >= dynamicMinDuration && duration <= DASH_MAX_MS && toneIndex_ < MAX_CW_ELEMENTS) {
            rawToneDurations_[toneIndex_++] = duration;
            updateReferenceTimings(duration);
        } else {
            if (duration > DASH_MAX_MS) {
                DEBUG("CW: TÚL HOSSZÚ elem: %lu ms (max: %lu, index: %d)\n", duration, (unsigned long)DASH_MAX_MS, toneIndex_);
            } else if (duration < dynamicMinDuration) {
                DEBUG("CW: Dinamikus zajszűrés: %lu ms < %lu ms (adaptív minimum)\n", duration, dynamicMinDuration);
            }
        }
        measuringTone_ = false;
    } else if (decoderStarted_ && !measuringTone_ && currentToneState) {
        unsigned long gapDuration = currentTimeMs - trailingEdgeTimeMs_;
        wordSpaceProcessed_ = false;

        if (toneIndex_ >= MAX_CW_ELEMENTS) {
            DEBUG("CW: Tömb tele (%d elem), kényszer dekódolás\n", toneIndex_);
            decodedChar = processCollectedElements();
        }

        if (gapDuration >= charGapMs && toneIndex_ > 0) {
            decodedChar = processCollectedElements();
        }

        leadingEdgeTimeMs_ = currentTimeMs;
        measuringTone_ = true;
    } else if (decoderStarted_ && !measuringTone_ && !currentToneState) {
        unsigned long spaceDuration = currentTimeMs - trailingEdgeTimeMs_;
        if ((spaceDuration > charGapMs && toneIndex_ > 0) || toneIndex_ >= MAX_CW_ELEMENTS) {
            if (toneIndex_ >= MAX_CW_ELEMENTS) {
                DEBUG("CW: Tömb tele csendben (%d elem), kényszer dekódolás\n", toneIndex_);
            }
            decodedChar = processCollectedElements();
            decoderStarted_ = false;
        }
    }

    if (decodedChar == '\0' && !measuringTone_ && !currentToneState && lastDecodedChar_ != '\0') {
        unsigned long spaceDuration = currentTimeMs - trailingEdgeTimeMs_;

        if (currentTimeMs - lastSpaceDebugMs_ >= SPACE_DEBUG_INTERVAL_MS) {
            DEBUG("CW: Szóköz ellenőrzés - space: %lu ms, küszöb: %lu ms, lastChar: '%c'\n", spaceDuration, wordGapMs, lastDecodedChar_);
            lastSpaceDebugMs_ = currentTimeMs;
        }

        if (spaceDuration > wordGapMs && !wordSpaceProcessed_) {
            decodedChar = ' ';
            wordSpaceProcessed_ = true;
        }
    }

    if (decodedChar != '\0') {
        if (isprint(decodedChar)) { // Csak nyomtatható karaktereket adunk hozzá
            decodedText += decodedChar;
            DEBUG("CW Decoded: %s\n", decodedText.c_str());
        }
        if (decodedChar != ' ') {
            lastDecodedChar_ = decodedChar;
        }
    }
}

void CwRttyDecoder::updateReferenceTimings(unsigned long duration) {
    const unsigned long ADAPTIVE_WEIGHT_OLD = 2;
    const unsigned long ADAPTIVE_WEIGHT_NEW = 1;
    const unsigned long ADAPTIVE_DIVISOR = ADAPTIVE_WEIGHT_OLD + ADAPTIVE_WEIGHT_NEW;

    if (toneMinDurationMs_ == 9999L) {
        if (duration < (startReferenceMs_ * 1.5)) {
            toneMinDurationMs_ = duration;
            currentReferenceMs_ = duration * 2.2;
        } else {
            toneMinDurationMs_ = duration / 3.2;
            toneMaxDurationMs_ = duration;
            currentReferenceMs_ = (toneMinDurationMs_ + toneMaxDurationMs_) / 2;
        }
    } else {
        if (duration < currentReferenceMs_) {
            toneMinDurationMs_ = (toneMinDurationMs_ * ADAPTIVE_WEIGHT_OLD + duration * ADAPTIVE_WEIGHT_NEW) / ADAPTIVE_DIVISOR;
        } else {
            if (toneMaxDurationMs_ == 0L) {
                toneMaxDurationMs_ = duration;
            } else {
                toneMaxDurationMs_ = (toneMaxDurationMs_ * ADAPTIVE_WEIGHT_OLD + duration * ADAPTIVE_WEIGHT_NEW) / ADAPTIVE_DIVISOR;
            }
        }

        if (toneMaxDurationMs_ > 0L && toneMinDurationMs_ < 9999L) {
            unsigned long calculatedRef = toneMinDurationMs_ + ((toneMaxDurationMs_ - toneMinDurationMs_) / 3);
            currentReferenceMs_ = (currentReferenceMs_ * ADAPTIVE_WEIGHT_OLD + calculatedRef * ADAPTIVE_WEIGHT_NEW) / ADAPTIVE_DIVISOR;
        }
    }

    toneMinDurationMs_ = constrain(toneMinDurationMs_, DOT_MIN_MS, DOT_MAX_MS);
    if (toneMaxDurationMs_ > 0) {
        toneMaxDurationMs_ = constrain(toneMaxDurationMs_, DOT_MIN_MS, DASH_MAX_MS);
    }

    unsigned long lowerBound = max(DOT_MIN_MS + 5, toneMinDurationMs_ * 2);
    unsigned long upperBound = DOT_MAX_MS + 50;
    currentReferenceMs_ = constrain(currentReferenceMs_, lowerBound, upperBound);
}

char CwRttyDecoder::processCollectedElements() {
    if (toneIndex_ == 0)
        return '\0';

    resetMorseTree();

    for (short i = 0; i < toneIndex_; i++) {
        unsigned long duration = rawToneDurations_[i];
        if (duration < currentReferenceMs_) {
            processDot();
        } else {
            processDash();
        }
    }

    char result = getCharFromTree();

    memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
    toneIndex_ = 0;

    if (result != '\0' && result != ' ' && isprint(result)) {
        return result;
    }

    return '\0';
}

void CwRttyDecoder::resetMorseTree() {
    treeIndex_ = MORSE_TREE_ROOT_INDEX;
    treeOffset_ = MORSE_TREE_INITIAL_OFFSET;
    treeCount_ = MORSE_TREE_MAX_DEPTH;
}

char CwRttyDecoder::getCharFromTree() {
    if (treeIndex_ >= 0 && treeIndex_ < (int)sizeof(MORSE_TREE_SYMBOLS)) {
        return MORSE_TREE_SYMBOLS[treeIndex_];
    }
    return '\0';
}

void CwRttyDecoder::processDot() {
    treeIndex_ -= treeOffset_;
    treeOffset_ /= 2;
    treeCount_--;
    if (treeCount_ < 0) {
        DEBUG("CW Decoder: Tree error (dot)\n");
        clear(); // Full reset on error
    }
}

void CwRttyDecoder::processDash() {
    treeIndex_ += treeOffset_;
    treeOffset_ /= 2;
    treeCount_--;
    if (treeCount_ < 0) {
        DEBUG("CW Decoder: Tree error (dash)\n");
        clear(); // Full reset on error
    }
}
