#include "CwRttyDecoder.h"
#include "Config.h"
#include "defines.h"
#include "utils.h"
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
    movingToneMs_ = 0;
    movingGapMs_ = 0;
    // Reálisabb kezdeti értékek 15-20wpm-hez
    dotMs_ = 80;   // kb. 15wpm pont
    dashMs_ = 240; // kb. 15wpm vonás
    gapMs_ = 240;  // karakterköz

    resetAfterLongElement_ = false;
}

String CwRttyDecoder::getDecodedText() {
    String inProgressMorse = "";
    if (toneIndex_ > 0) {
        float dotRef = (dotMs_ > 0) ? dotMs_ : 60.0f;
        float dashRef = (dashMs_ > 0) ? dashMs_ : 3.0f * dotRef;
        for (uint8_t i = 0; i < toneIndex_; i++) {
            unsigned long duration = rawToneDurations_[i];
            if (fabsf(duration - dotRef) < fabsf(duration - dashRef)) {
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

    // --- Adaptív zajszint és threshold becslés ---
    static float noiseLevel = 0.0f;
    static float signalThreshold = 0.0f;
    static const float NOISE_ALPHA = 0.01f; // lassú zajkövetés
    static const float SIGNAL_ALPHA = 0.2f; // gyorsabb threshold adaptáció
    static const float NOISE_FLOOR_FACTOR = 3.0f;
    static const float MINIMUM_THRESHOLD = 1000.0f;

    // Ha nincs jel, a peakMagnitude a zajszintet követi
    if (peakMagnitude < 5000.0f) {
        if (noiseLevel == 0.0f)
            noiseLevel = peakMagnitude;
        else
            noiseLevel = (1.0f - NOISE_ALPHA) * noiseLevel + NOISE_ALPHA * peakMagnitude;
    }
    // Threshold adaptáció
    float targetThreshold = noiseLevel * NOISE_FLOOR_FACTOR;
    if (targetThreshold < MINIMUM_THRESHOLD)
        targetThreshold = MINIMUM_THRESHOLD;
    if (signalThreshold == 0.0f)
        signalThreshold = targetThreshold;
    else
        signalThreshold = (1.0f - SIGNAL_ALPHA) * signalThreshold + SIGNAL_ALPHA * targetThreshold;

    // --- Hang detektálás ---
    static bool prevToneState = false;
    bool currentToneState = (peakMagnitude > signalThreshold);

    // --- Adaptív morze időzítések tanulása ---
    // Statikus változók a tanuláshoz
    static unsigned long lastGapDuration = 0;
    static const float ALPHA = 0.15f; // tanulási ráta

    // Logoljuk a threshold-ot is
    // DEBUG("CW: peakBin: %d (%-8s), peakMagnitude: %s, threshold: %s\n", peakBin, Utils::floatToString(peakFrequencyHz).c_str(), Utils::floatToString(peakMagnitude).c_str(),
    // Utils::floatToString(signalThreshold).c_str());

    // Hang kezdetének és végének logolása
    static unsigned long lastToneChangeMs = 0;
    static unsigned long lastToneEndMs = 0;
    static unsigned long lastResetMs = 0;
    unsigned long nowMs = millis();
    if (!prevToneState && currentToneState) {
        // Hang kezdete
        if (lastToneEndMs > 0) {
            unsigned long gap = nowMs - lastToneEndMs;
            DEBUG("CW: SZUNET (gap) %lu ms\n", gap);
        }
        // DEBUG("CW: HANG KEZDETE @ %lu ms\n", nowMs);
        lastToneChangeMs = nowMs;
    } else if (prevToneState && !currentToneState) {
        // Hang vége
        unsigned long elemDuration = nowMs - lastToneChangeMs;
        DEBUG("CW: HANG VEGE   @ %lu ms, tartam: %lu ms\n", nowMs, elemDuration);
        lastToneEndMs = nowMs;
        lastToneChangeMs = nowMs;

        // --- Morze elem felismerés és logolás ---
        if (elemDuration > DASH_MAX_MS) {
            DEBUG("CW: TÚL HOSSZÚ elem: %lu ms (max: %lu, index: %d) -- TELJES RESET!\n", elemDuration, (unsigned long)DASH_MAX_MS, toneIndex_);
            updateReferenceTimings(elemDuration);
            // Teljes reset, hogy ne ragadjon bent
            measuringTone_ = false;
            decoderStarted_ = false;
            toneIndex_ = 0;
            memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            resetMorseTree();
            lastResetMs = nowMs;
            resetAfterLongElement_ = true;
            // A következő hangot csak új hangnál kezdjük mérni, de csak ha elég hosszú volt a szünet
            return;
        } else if (elemDuration < DOT_MIN_MS) {
            DEBUG("CW: TÚL RÖVID elem: %lu ms (min: %lu, index: %d) -- IGNORÁLVA!\n", elemDuration, (unsigned long)DOT_MIN_MS, toneIndex_);
            updateReferenceTimings(elemDuration);
        } else {
            if (dotMs_ == 0)
                dotMs_ = elemDuration;
            if (dashMs_ == 0)
                dashMs_ = elemDuration;
            char morseType = '?';
            float avgDotDash = (dotMs_ + dashMs_) / 2;
            if (elemDuration < avgDotDash * 1.2f) {
                morseType = '.';
                dotMs_ = (1.0f - ALPHA) * dotMs_ + ALPHA * elemDuration;
            } else if (elemDuration < avgDotDash * 2.5f) {
                morseType = '-';
                dashMs_ = (1.0f - ALPHA) * dashMs_ + ALPHA * elemDuration;
            }
            DEBUG("CW: ELEM: %c (%lu ms)\n", morseType, elemDuration);
        }
    }
    prevToneState = currentToneState;

    // --- Adaptív morze időzítések tanulása ---
    // (definíciók fentebb)

    // Ha most kezdődött egy hang (tehát most volt szünet vége)
    static float charGapMs_ = 0, wordGapMs_ = 0;
    if (!prevToneState && currentToneState && lastToneEndMs > 0) {
        lastGapDuration = nowMs - lastToneEndMs;
        // Gap tanulása
        if (gapMs_ == 0)
            gapMs_ = lastGapDuration;
        if (charGapMs_ == 0)
            charGapMs_ = lastGapDuration;
        if (wordGapMs_ == 0)
            wordGapMs_ = lastGapDuration;

        float avgDotDash = (dotMs_ + dashMs_) / 2;
        if (lastGapDuration < 1.5f * avgDotDash) {
            gapMs_ = (1.0f - ALPHA) * gapMs_ + ALPHA * lastGapDuration;
        } else if (lastGapDuration < 4.0f * avgDotDash) {
            charGapMs_ = (1.0f - ALPHA) * charGapMs_ + ALPHA * lastGapDuration;
        } else {
            wordGapMs_ = (1.0f - ALPHA) * wordGapMs_ + ALPHA * lastGapDuration;
        }
    }

    // --- Karakterköz és szóköz detektálás, egyértelmű loggal ---
    if (!prevToneState && currentToneState && lastToneEndMs > 0) {
        float avgDotDash = (dotMs_ + dashMs_) / 2;
        if (lastGapDuration >= 1.5f * avgDotDash && lastGapDuration < 4.0f * avgDotDash) {
            DEBUG("CW: CHAR GAP (%lu ms) -- karakter vége\n", lastGapDuration);
        } else if (lastGapDuration >= 4.0f * avgDotDash) {
            DEBUG("CW: WORD GAP (%lu ms) -- szó vége\n", lastGapDuration);
        }
    }
}

void CwRttyDecoder::updateReferenceTimings(unsigned long duration) {
    // Ha túl hosszú elem jön, reseteljük a tanulást
    if (duration > DASH_MAX_MS) {
        toneMinDurationMs_ = 9999L;
        toneMaxDurationMs_ = 0L;
        currentReferenceMs_ = startReferenceMs_;
        return;
    }
    // Lassabb adaptáció: kisebb súly az új értéknek
    const unsigned long ADAPTIVE_WEIGHT_OLD = 3;
    const unsigned long ADAPTIVE_WEIGHT_NEW = 1;
    const unsigned long ADAPTIVE_DIVISOR = ADAPTIVE_WEIGHT_OLD + ADAPTIVE_WEIGHT_NEW;

    // Kezdeti tanulás: első rövid elem alapján induljon
    if (toneMinDurationMs_ == 9999L) {
        if (duration < (startReferenceMs_ * 1.5)) {
            toneMinDurationMs_ = duration;
            currentReferenceMs_ = duration * 2.0;
        } else {
            toneMinDurationMs_ = duration / 3.0;
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

    // Reális alsó/felső korlátok (gyorsabb tempóhoz is)
    toneMinDurationMs_ = constrain(toneMinDurationMs_, DOT_MIN_MS, DOT_MAX_MS);
    if (toneMaxDurationMs_ > 0) {
        toneMaxDurationMs_ = constrain(toneMaxDurationMs_, DOT_MIN_MS, DASH_MAX_MS);
    }

    // A currentReferenceMs_ alsó korlátja ne legyen túl magas
    unsigned long lowerBound = DOT_MIN_MS + 2; // pl. 32 ms
    unsigned long upperBound = DOT_MAX_MS * 2; // pl. 500 ms
    currentReferenceMs_ = constrain(currentReferenceMs_, lowerBound, upperBound);
}

char CwRttyDecoder::processCollectedElements() {
    if (toneIndex_ == 0)
        return '\0';

    // Forced decode vagy karakterhatár: csak akkor dekódolunk, ha minden elem érvényes
    for (short i = 0; i < toneIndex_; i++) {
        unsigned long duration = rawToneDurations_[i];
        if (duration < DOT_MIN_MS || duration > DASH_MAX_MS) {
            // Hibás elem, csak reset, nincs dekódolás
            memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            toneIndex_ = 0;
            return '\0';
        }
    }

    resetMorseTree();
    // Teljesen adaptív dot/dash döntés: amelyik átlaghoz közelebb van
    float dotRef = (dotMs_ > 0) ? dotMs_ : 60.0f;
    float dashRef = (dashMs_ > 0) ? dashMs_ : 3.0f * dotRef;
    for (short i = 0; i < toneIndex_; i++) {
        unsigned long duration = rawToneDurations_[i];
        if (fabsf(duration - dotRef) < fabsf(duration - dashRef)) {
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
