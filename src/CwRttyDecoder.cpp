// Fejlett spektrum-alapú CW dekóder (időzítés, karakter, szóköz, puffer)
#include "CwRttyDecoder.h"
#include <Arduino.h>
#include <algorithm>
#include <cstring>

// Morse bináris fa (átemelve)
const char CwRttyDecoder::MORSE_TREE_SYMBOLS[128] = {' ', '5', ' ', 'H', ' ', '4', ' ', 'S', ' ', ' ', ' ', 'V', ' ', '3', ' ', 'I', ' ', ' ', ' ', 'F', ' ', ' ', ' ', 'U', '?', ' ', '_', ' ', ' ',  '2', ' ', 'E',
                                                     ' ', '&', ' ', 'L', '"', ' ', ' ', 'R', ' ', '+', '.', ' ', ' ', ' ', ' ', 'A', ' ', ' ', ' ', 'P', '@', ' ', ' ', 'W', ' ', ' ', ' ', 'J', '\'', '1', ' ', ' ',
                                                     ' ', '6', '-', 'B', ' ', '=', ' ', 'D', ' ', '/', ' ', 'X', ' ', ' ', ' ', 'N', ' ', ' ', ' ', 'C', ';', ' ', '!', 'K', ' ', '(', ')', 'Y', ' ',  ' ', ' ', 'T',
                                                     ' ', '7', ' ', 'Z', ' ', ' ', ',', 'G', ' ', ' ', ' ', 'Q', ' ', ' ', ' ', 'M', ':', '8', ' ', ' ', ' ', ' ', ' ', 'O', ' ', '9', ' ', ' ', ' ',  '0', ' ', ' '};

CwRttyDecoder::CwRttyDecoder(float sampleRate, int fftSize, float binWidthHz) : sampleRate_(sampleRate), fftSize_(fftSize), binWidthHz_(binWidthHz) { resetDecoderState(); }

/**
 * @brief CW dekóder frekvencia beállítása (Hz)
 * @param freqHz CW dekódolási frekvencia (Hz)
 */
void CwRttyDecoder::setCwFreq(float freqHz) { cwFreqHz_ = freqHz; }

/**
 * @brief RTTY dekóder frekvencia beállítása (Hz)
 * @param markHz Mark frekvencia (Hz)
 * @param spaceHz Space frekvencia (Hz)
 *
 */
void CwRttyDecoder::setRttyFreq(float markHz, float spaceHz) {
    rttyMarkHz_ = markHz;
    rttySpaceHz_ = spaceHz;
}

/**
 * @brief CW/RTTY dekódolás futtatása egy FFT frame-re.
 * @param spectrum Magnitúdó tömb (AudioProcessor::RvReal)
 */
void CwRttyDecoder::process(const double *spectrum) {
    uint64_t nowMicros = micros();

    // 1. CW detektálás: a cwFreqHz_ körüli bin maximumát figyeljük
    int cwBin = static_cast<int>(cwFreqHz_ / binWidthHz_ + 0.5f);
    if (cwBin < 0 || cwBin >= fftSize_)
        return;
    double mag = spectrum[cwBin];
    bool tone = mag > 1000.0; // Ezt hangolni kell

    // 2. Él detektálás, időzítés
    if (tone != lastTone_) {
        if (tone) {
            lastToneOnMicros_ = nowMicros;
        } else {
            lastToneOffMicros_ = nowMicros;
            unsigned long duration = (lastToneOffMicros_ - lastToneOnMicros_) / 1000UL; // ms
            if (duration > 5 && duration < 1000 && toneIndex_ < MAX_TONES) {
                rawToneDurations_[toneIndex_++] = duration;
                updateReferenceTimings(duration);
            }
            // Karakterhatár detektálás
            if (toneIndex_ >= MAX_TONES) {
                char decodedChar = processCollectedElements();
                addToBuffer(decodedChar);
                toneIndex_ = 0;
                std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            }
        }
        lastEdgeMicros_ = nowMicros;
    }
    lastTone_ = tone;

    // 3. Karakter- és szóköz detektálás szünet alapján
    if (!tone && toneIndex_ > 0 && lastToneOffMicros_ > 0) {
        unsigned long gapMs = (nowMicros - lastToneOffMicros_) / 1000UL;
        unsigned long charGapMs = std::max(60UL, (unsigned long)(toneMinDurationMs_ * 2.5));
        unsigned long wordGapMs = std::max(200UL, (unsigned long)(toneMinDurationMs_ * 7));
        if (gapMs > wordGapMs && !wordSpaceProcessed_) {
            addToBuffer(' ');
            wordSpaceProcessed_ = true;
            toneIndex_ = 0;
            std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
        } else if (gapMs > charGapMs) {
            char decodedChar = processCollectedElements();
            addToBuffer(decodedChar);
            toneIndex_ = 0;
            std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            wordSpaceProcessed_ = false;
        }
    }

    // 4. Soros port kimenet: minden új karaktert kiírunk
    char c;
    while ((c = getCharacterFromBuffer()) != '\0') {
        Serial.print(c);
    }
}

// --- Morse fa, puffer, referencia logika ---
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
        resetMorseTree();
        currentReferenceMs_ = startReferenceMs_;
        toneMinDurationMs_ = 9999L;
        toneMaxDurationMs_ = 0L;
        toneIndex_ = 0;
        decoderStarted_ = false;
        measuringTone_ = false;
    }
}
void CwRttyDecoder::processDash() {
    treeIndex_ += treeOffset_;
    treeOffset_ /= 2;
    treeCount_--;
    if (treeCount_ < 0) {
        resetMorseTree();
        currentReferenceMs_ = startReferenceMs_;
        toneMinDurationMs_ = 9999L;
        toneMaxDurationMs_ = 0L;
        toneIndex_ = 0;
        decoderStarted_ = false;
        measuringTone_ = false;
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
    // Biztonsági korlátok
    toneMinDurationMs_ = std::max(20UL, std::min(toneMinDurationMs_, 200UL));
    toneMaxDurationMs_ = std::max(40UL, std::min(toneMaxDurationMs_, 600UL));
    unsigned long lowerBound = std::max(25UL, toneMinDurationMs_ * 2);
    unsigned long upperBound = 350UL;
    currentReferenceMs_ = std::max(lowerBound, std::min(currentReferenceMs_, upperBound));
}
char CwRttyDecoder::processCollectedElements() {
    if (toneIndex_ == 0)
        return '\0';
    resetMorseTree();
    for (int i = 0; i < toneIndex_; i++) {
        unsigned long duration = rawToneDurations_[i];
        if (duration < currentReferenceMs_) {
            processDot();
        } else {
            processDash();
        }
    }
    char result = getCharFromTree();
    if (result != '\0' && result != ' ') {
        if (isprint(result))
            return result;
    }
    return '\0';
}
void CwRttyDecoder::addToBuffer(char c) {
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
char CwRttyDecoder::getCharacterFromBuffer() {
    if (charBufferCount_ == 0)
        return '\0';
    char c = decodedCharBuffer_[charBufferReadPos_];
    charBufferReadPos_ = (charBufferReadPos_ + 1) % DECODED_CHAR_BUFFER_SIZE;
    charBufferCount_--;
    return c;
}
void CwRttyDecoder::resetDecoderState() {
    startReferenceMs_ = 120;
    currentReferenceMs_ = startReferenceMs_;
    toneMinDurationMs_ = 9999L;
    toneMaxDurationMs_ = 0L;
    lastEdgeMicros_ = 0;
    lastToneOnMicros_ = 0;
    lastToneOffMicros_ = 0;
    lastTone_ = false;
    decoderStarted_ = false;
    measuringTone_ = false;
    wordSpaceProcessed_ = false;
    inInactiveState_ = false;
    lastDecodedChar_ = '\0';
    resetMorseTree();
    toneIndex_ = 0;
    std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
    std::memset(decodedCharBuffer_, 0, sizeof(decodedCharBuffer_));
    charBufferReadPos_ = 0;
    charBufferWritePos_ = 0;
    charBufferCount_ = 0;
}
