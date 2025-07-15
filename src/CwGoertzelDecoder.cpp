
#include "CwGoertzelDecoder.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

// --- Morse bináris fa (CwDecoder-ből átemelve) ---
namespace {
constexpr int MORSE_TREE_ROOT_INDEX = 63;
constexpr int MORSE_TREE_INITIAL_OFFSET = 32;
constexpr int MORSE_TREE_MAX_DEPTH = 6;
constexpr char MORSE_TREE_SYMBOLS[] = {' ', '5', ' ', 'H', ' ', '4', ' ', 'S', ' ', ' ', ' ', 'V', ' ', '3', ' ', 'I', ' ', ' ', ' ', 'F', ' ', ' ', ' ', 'U', '?', ' ', '_', ' ', ' ',  '2', ' ', 'E',
                                       ' ', '&', ' ', 'L', '"', ' ', ' ', 'R', ' ', '+', '.', ' ', ' ', ' ', ' ', 'A', ' ', ' ', ' ', 'P', '@', ' ', ' ', 'W', ' ', ' ', ' ', 'J', '\'', '1', ' ', ' ',
                                       ' ', '6', '-', 'B', ' ', '=', ' ', 'D', ' ', '/', ' ', 'X', ' ', ' ', ' ', 'N', ' ', ' ', ' ', 'C', ';', ' ', '!', 'K', ' ', '(', ')', 'Y', ' ',  ' ', ' ', 'T',
                                       ' ', '7', ' ', 'Z', ' ', ' ', ',', 'G', ' ', ' ', ' ', 'Q', ' ', ' ', ' ', 'M', ':', '8', ' ', ' ', ' ', ' ', ' ', 'O', ' ', '9', ' ', ' ', ' ',  '0', ' ', ' '};
} // namespace

// --- Dekódolási paraméterek (CwDecoder-ből) ---
constexpr unsigned long DOT_MIN_MS = 25;
constexpr unsigned long DOT_MAX_MS = 300;
constexpr unsigned long DASH_MAX_MS = 900;
constexpr unsigned long MIN_CHAR_GAP_MS_FALLBACK = 120;
constexpr unsigned long MIN_WORD_GAP_MS_FALLBACK = 300;
constexpr float CHAR_GAP_DOT_MULTIPLIER = 2.5f;
constexpr float WORD_GAP_DOT_MULTIPLIER = 6.5f;
constexpr unsigned long MIN_ADAPTIVE_DOT_MS = 15;
constexpr unsigned long NOISE_THRESHOLD_FACTOR = 5;
constexpr unsigned long SPACE_DEBUG_INTERVAL_MS = 1000;
constexpr int DECODED_CHAR_BUFFER_SIZE = 32;

#include "defines.h"
#include <Arduino.h>
#include <cstdio>
CwGoertzelDecoder::CwGoertzelDecoder(float sampleRate, int blockSize, float cwFreqHz, float threshold) : goertzel_(sampleRate, blockSize), cwFreq_(cwFreqHz), threshold_(threshold) {
    thresholdFactor_ = 1.2; // érzékenyebb alapértelmezett érték
    reset();
}

float CwGoertzelDecoder::getSampleRate() const { return goertzel_.getSampleRate(); }
int CwGoertzelDecoder::getBlockSize() const { return goertzel_.getBlockSize(); }

void CwGoertzelDecoder::reset() {
    std::memset(decodedCharBuffer_, 0, sizeof(decodedCharBuffer_));
    charBufferReadPos_ = 0;
    charBufferWritePos_ = 0;
    charBufferCount_ = 0;
    treeIndex_ = MORSE_TREE_ROOT_INDEX;
    treeOffset_ = MORSE_TREE_INITIAL_OFFSET;
    treeCount_ = MORSE_TREE_MAX_DEPTH;
    toneIndex_ = 0;
    toneMaxDurationMs_ = 0L;
    toneMinDurationMs_ = 9999L;
    currentReferenceMs_ = 120;
    startReferenceMs_ = 120;
    decoderStarted_ = false;
    measuringTone_ = false;
    toneDetectedState_ = false;
    lastActivityMs_ = 0;
    lastDecodedChar_ = '\0';
    wordSpaceProcessed_ = false;
    lastSpaceDebugMs_ = 0;
    inInactiveState = false;
    leadingEdgeTimeMs_ = 0;
    trailingEdgeTimeMs_ = 0;
    std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
}

// --- Bináris Morse-fa logika ---
void CwGoertzelDecoder::resetMorseTree() {
    treeIndex_ = MORSE_TREE_ROOT_INDEX;
    treeOffset_ = MORSE_TREE_INITIAL_OFFSET;
    treeCount_ = MORSE_TREE_MAX_DEPTH;
}
char CwGoertzelDecoder::getCharFromTree() {
    if (treeIndex_ >= 0 && treeIndex_ < (int)sizeof(MORSE_TREE_SYMBOLS)) {
        return MORSE_TREE_SYMBOLS[treeIndex_];
    }
    return '\0';
}
void CwGoertzelDecoder::processDot() {
    treeIndex_ -= treeOffset_;
    treeOffset_ /= 2;
    treeCount_--;
}
void CwGoertzelDecoder::processDash() {
    treeIndex_ += treeOffset_;
    treeOffset_ /= 2;
    treeCount_--;
}

void CwGoertzelDecoder::updateReferenceTimings(unsigned long duration) {
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
        unsigned long currentThreshold = currentReferenceMs_;
        if (duration < currentThreshold) {
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
    toneMinDurationMs_ = std::clamp(toneMinDurationMs_, DOT_MIN_MS, DOT_MAX_MS);
    toneMaxDurationMs_ = std::clamp(toneMaxDurationMs_, DOT_MIN_MS, DASH_MAX_MS);
    unsigned long lowerBound = std::max(DOT_MIN_MS + 5, toneMinDurationMs_ * 2);
    unsigned long upperBound = DOT_MAX_MS + 50;
    currentReferenceMs_ = std::clamp(currentReferenceMs_, lowerBound, upperBound);
}

char CwGoertzelDecoder::processCollectedElements() {
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
    if (result != '\0' && result != ' ' && std::isprint(result)) {
        return result;
    }
    return '\0';
}

void CwGoertzelDecoder::addToBuffer(char c) {
    if (c == '\0')
        return;
    decodedCharBuffer_[charBufferWritePos_] = c;
    charBufferWritePos_ = (charBufferWritePos_ + 1) % DECODED_CHAR_BUFFER_SIZE;
    if (charBufferCount_ < DECODED_CHAR_BUFFER_SIZE) {
        charBufferCount_++;
    } else {
        charBufferReadPos_ = (charBufferReadPos_ + 1) % DECODED_CHAR_BUFFER_SIZE;
    }
}

std::string CwGoertzelDecoder::getDecodedText() const {
    std::string result;
    int pos = charBufferReadPos_;
    for (int i = 0; i < charBufferCount_; ++i) {
        result += decodedCharBuffer_[pos];
        pos = (pos + 1) % DECODED_CHAR_BUFFER_SIZE;
    }
    return result;
}

// --- Fő dekódoló logika (mint CwDecoder::updateDecoder, de Goertzel-alapú hangdetektálással) ---
void CwGoertzelDecoder::processBlock(const double *samples, int numSamples, unsigned long currentTimeMs) {
    static const unsigned long MAX_SILENCE_MS = 4000;

    // Goertzel-alapú hangdetektálás
    double mag = goertzel_.run(samples, numSamples, cwFreq_);
    if (autoThreshold_) {
        if (noiseEstimate_ == 0.0)
            noiseEstimate_ = mag;
        noiseEstimate_ = (1.0 - alpha_) * noiseEstimate_ + alpha_ * mag;
        threshold_ = noiseEstimate_ * thresholdFactor_;
    }

    double magNorm = mag / (double)goertzel_.getBlockSize();
    double thrNorm = threshold_ / (double)goertzel_.getBlockSize();
    float magDb = (magNorm > 0.0) ? 20.0f * log10f((float)magNorm) : -99.0f;
    float thrDb = (thrNorm > 0.0) ? 20.0f * log10f((float)thrNorm) : -99.0f;
    float diffDb = magDb - thrDb;
    // Automatikus thresholdFactor hangolás: cél, hogy diffDb kb. 3-6 dB legyen
    static float autoThreshTargetDb = 4.0f;
    static float stuckDbLimit = -2.0f;
    static int stuckCount = 0;
    static const int stuckLimit = 10; // 10 egymás utáni blokk
    if (autoThreshold_) {
        float error = diffDb - autoThreshTargetDb;
        if (diffDb < stuckDbLimit) {
            stuckCount++;
            thresholdFactor_ += 0.10f; // gyors visszaemelés, ha túl alacsony a diff
        } else if (diffDb > autoThreshTargetDb + 2.0f) {
            stuckCount = 0;
            thresholdFactor_ -= 0.02f; // nagyon lassú csökkentés
        } else if (diffDb > autoThreshTargetDb + 1.0f) {
            stuckCount = 0;
            thresholdFactor_ -= 0.01f; // lassú csökkentés
        } else {
            stuckCount = 0;
            thresholdFactor_ += error * 0.07f; // finomhangolás
        }
        if (stuckCount > stuckLimit) {
            thresholdFactor_ = 1.30f; // recovery érték
            stuckCount = 0;
        }
        if (thresholdFactor_ < 1.20f)
            thresholdFactor_ = 1.20f;
        if (thresholdFactor_ > 2.5f)
            thresholdFactor_ = 2.5f;
    }
    bool currentToneState = mag > threshold_;
    char magStr[16], thrStr[16], magDbStr[16], thrDbStr[16], diffDbStr[16], tfStr[16];
    dtostrf(magNorm, 8, 2, magStr);
    dtostrf(thrNorm, 8, 2, thrStr);
    dtostrf(magDb, 7, 1, magDbStr);
    dtostrf(thrDb, 7, 1, thrDbStr);
    dtostrf(diffDb, 7, 1, diffDbStr);
    dtostrf(thresholdFactor_, 4, 2, tfStr);
    DEBUG("[CWDBG] mag=%s (dB=%s) threshold=%s (dB=%s) diff=%s tf=%s state=%s\n", magStr, magDbStr, thrStr, thrDbStr, diffDbStr, tfStr, currentToneState ? "true" : "false");

    unsigned long estimatedDotLength = (toneMinDurationMs_ == 9999L || toneMinDurationMs_ == 0) ? (currentReferenceMs_ / 2) : toneMinDurationMs_;
    if (estimatedDotLength < DOT_MIN_MS || currentReferenceMs_ == 0)
        estimatedDotLength = DOT_MIN_MS;
    unsigned long charGapMs = std::max(MIN_CHAR_GAP_MS_FALLBACK, (unsigned long)(estimatedDotLength * CHAR_GAP_DOT_MULTIPLIER));
    unsigned long wordGapMs = std::max(MIN_WORD_GAP_MS_FALLBACK, (unsigned long)(estimatedDotLength * WORD_GAP_DOT_MULTIPLIER));
    if (wordGapMs <= charGapMs)
        wordGapMs = charGapMs + std::max(1UL, MIN_CHAR_GAP_MS_FALLBACK / 2);

    unsigned long currentTime = currentTimeMs;
    char decodedChar = '\0';

    if (currentToneState) {
        lastActivityMs_ = currentTime;
        if (!measuringTone_) {
            wordSpaceProcessed_ = false;
        }
    }
    if (currentTime - lastActivityMs_ > MAX_SILENCE_MS) {
        if (!inInactiveState) {
            reset();
            inInactiveState = true;
        }
        return;
    }
    if (!decoderStarted_ && !measuringTone_ && currentToneState) {
        leadingEdgeTimeMs_ = currentTime;
        decoderStarted_ = true;
        inInactiveState = false;
        measuringTone_ = true;
        wordSpaceProcessed_ = false;
    } else if (decoderStarted_ && measuringTone_ && !currentToneState) {
        trailingEdgeTimeMs_ = currentTime;
        unsigned long duration = trailingEdgeTimeMs_ - leadingEdgeTimeMs_;
        if (toneIndex_ >= 6) {
            decodedChar = processCollectedElements();
            std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            resetMorseTree();
            toneIndex_ = 0;
        }
        if (duration >= DOT_MIN_MS && duration <= DASH_MAX_MS && toneIndex_ < 6) {
            unsigned long dynamicMinDuration = DOT_MIN_MS;
            if (toneMinDurationMs_ != 9999L && toneMinDurationMs_ > 0) {
                dynamicMinDuration = std::max(DOT_MIN_MS, std::max(MIN_ADAPTIVE_DOT_MS, toneMinDurationMs_ / NOISE_THRESHOLD_FACTOR));
                if (duration >= DOT_MIN_MS && duration >= (dynamicMinDuration * 0.6)) {
                    dynamicMinDuration = std::min(dynamicMinDuration, duration);
                }
            } else {
                dynamicMinDuration = std::max(DOT_MIN_MS, MIN_ADAPTIVE_DOT_MS);
            }
            if (duration >= dynamicMinDuration) {
                rawToneDurations_[toneIndex_] = duration;
                toneIndex_++;
                updateReferenceTimings(duration);
            }
        }
        measuringTone_ = false;
    } else if (decoderStarted_ && !measuringTone_ && currentToneState) {
        unsigned long gapDuration = currentTime - trailingEdgeTimeMs_;
        wordSpaceProcessed_ = false;
        if (toneIndex_ >= 6) {
            decodedChar = processCollectedElements();
            std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            resetMorseTree();
            toneIndex_ = 0;
        }
        if (gapDuration >= charGapMs && toneIndex_ > 0) {
            decodedChar = processCollectedElements();
            if (decodedChar != '\0') {
                lastDecodedChar_ = decodedChar;
                lastActivityMs_ = currentTime;
            }
            resetMorseTree();
            toneIndex_ = 0;
            std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            leadingEdgeTimeMs_ = currentTime;
            measuringTone_ = true;
        } else if (gapDuration >= DOT_MIN_MS / 2 || toneIndex_ == 0) {
            leadingEdgeTimeMs_ = currentTime;
            measuringTone_ = true;
        }
    } else if (decoderStarted_ && !measuringTone_ && !currentToneState) {
        unsigned long spaceDuration = currentTime - trailingEdgeTimeMs_;
        if ((spaceDuration > charGapMs && toneIndex_ > 0) || toneIndex_ >= 6) {
            decodedChar = processCollectedElements();
            resetMorseTree();
            std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            toneIndex_ = 0;
            decoderStarted_ = false;
        }
    }
    if (decodedChar == '\0' && !measuringTone_ && !currentToneState && lastDecodedChar_ != '\0') {
        unsigned long spaceDuration = currentTime - trailingEdgeTimeMs_;
        unsigned long dynamicWordGapMs;
        if (toneMinDurationMs_ != 9999L && toneMinDurationMs_ > 0) {
            dynamicWordGapMs = toneMinDurationMs_ * 7;
        } else {
            dynamicWordGapMs = std::max(200UL, (unsigned long)(estimatedDotLength * 4.0f));
        }
        if (spaceDuration > dynamicWordGapMs && !wordSpaceProcessed_ && lastDecodedChar_ != ' ') {
            decodedChar = ' ';
            wordSpaceProcessed_ = true;
        }
    }
    if (decodedChar != '\0') {
        if (decodedChar != ' ') {
            lastDecodedChar_ = decodedChar;
        }
    }
    addToBuffer(decodedChar);
}
