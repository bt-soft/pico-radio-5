// Fejlett spektrum-alapú CW dekóder (időzítés, karakter, szóköz, puffer)
#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <string>

#include "CwRttyDecoder.h"

// --- Dekódolt szöveg folyamatos gyűjtése ---
static std::string decodedText_;

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

    // 1. Adaptív küszöb: környező bin-ek átlaga + szorzó
    int cwBin = static_cast<int>(cwFreqHz_ / binWidthHz_ + 0.5f);
    if (cwBin < 1 || cwBin >= fftSize_ - 1)
        return;
    // Vegyük a fő bin és két szomszéd átlagát (3 bin)
    double mag = (spectrum[cwBin - 1] + spectrum[cwBin] + spectrum[cwBin + 1]) / 3.0;
    // Zajszint: 10 bin-nel arrébb mindkét irányban átlagolva
    double noiseSum = 0.0;
    int noiseBins = 0;
    for (int i = -10; i <= 10; ++i) {
        int idx = cwBin + i;
        if (i >= -1 && i <= 1)
            continue; // fő bin-eket kihagyjuk
        if (idx >= 0 && idx < fftSize_) {
            noiseSum += spectrum[idx];
            ++noiseBins;
        }
    }
    double noiseAvg = (noiseBins > 0) ? (noiseSum / noiseBins) : 0.0;
    double threshold = noiseAvg * 2.5 + 80.0; // adaptívabb, de fix offsettel
    bool tone = mag > threshold;

    // // DEBUG kiírás a változók deklarációja után
    // Serial.print("[CWDBG] mag: ");
    // Serial.print(mag, 1);
    // Serial.print(" thr: ");
    // Serial.print(threshold, 1);
    // Serial.print(" tone: ");
    // Serial.print(tone ? "ON" : "off");
    // Serial.print(" tMin: ");
    // Serial.print(toneMinDurationMs_);
    // Serial.print(" tMax: ");
    // Serial.print(toneMaxDurationMs_);
    // Serial.print(" ref: ");
    // Serial.print(currentReferenceMs_);
    // Serial.print(" idx: ");
    // Serial.print(toneIndex_);
    // Serial.print(" tones: [");
    // for (int i = 0; i < toneIndex_; ++i) {
    //     Serial.print(rawToneDurations_[i]);
    //     if (i < toneIndex_ - 1)
    //         Serial.print(",");
    // }
    // Serial.print("]");

    // 2. Él detektálás, időzítés
    if (tone != lastTone_) {
        if (tone) {
            lastToneOnMicros_ = nowMicros;
            Serial.print(" | RISING edge");
        } else {
            lastToneOffMicros_ = nowMicros;
            unsigned long duration = (lastToneOffMicros_ - lastToneOnMicros_) / 1000UL; // ms
            Serial.print(" | FALL edge dur: ");
            Serial.print(duration);
            if (duration > 5 && duration < 1000 && toneIndex_ < MAX_TONES) {
                rawToneDurations_[toneIndex_++] = duration;
                updateReferenceTimings(duration);
            }
            // Részletes debug: aktuális toneIndex_ és elemek
            Serial.print(" | toneIndex: ");
            Serial.print(toneIndex_);
            Serial.print(" [");
            for (int i = 0; i < toneIndex_; ++i) {
                Serial.print(rawToneDurations_[i]);
                if (i < toneIndex_ - 1)
                    Serial.print(",");
            }
            Serial.print("]");
            // Karakterhatár detektálás: csak akkor töröljük a toneIndex_-et, ha tényleg karakterhatár van (lásd lejjebb)
            // Itt NEM dekódolunk karaktert, csak gyűjtjük az elemeket!
        }
        lastEdgeMicros_ = nowMicros;
    }
    lastTone_ = tone;

    // 3. Karakter- és szóköz detektálás szünet alapján
    static uint64_t lastGapCheckMicros = 0;
    if (!tone && toneIndex_ > 0 && lastToneOffMicros_ > 0) {
        unsigned long gapMs = (nowMicros - lastToneOffMicros_) / 1000UL;
        unsigned long charGapMs = std::max(180UL, (unsigned long)(toneMinDurationMs_ * 6.0));  // Morse: 6x pont (még engedékenyebb)
        unsigned long wordGapMs = std::max(600UL, (unsigned long)(toneMinDurationMs_ * 12.0)); // szóköz: 12x pont (még engedékenyebb)
        Serial.print(" | gap: ");
        Serial.print(gapMs);
        // Csak akkor vizsgáljuk a karakterhatárt, ha a gap alatt nem volt újabb RISING edge (azaz lastTone_ továbbra is false)
        if (gapMs > wordGapMs && !wordSpaceProcessed_ && lastGapCheckMicros != lastToneOffMicros_) {
            Serial.print(" | SZÓKÖZ (WORD GAP)");
            addToBuffer(' ');
            wordSpaceProcessed_ = true;
            char decodedChar = processCollectedElements();
            if (decodedChar != '\0') {
                Serial.print(" | SZÓKÖZ dekódolva: ");
                Serial.print(decodedChar);
                addToBuffer(decodedChar);
            }
            toneIndex_ = 0;
            std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            lastGapCheckMicros = lastToneOffMicros_;
        } else if (gapMs > charGapMs && lastGapCheckMicros != lastToneOffMicros_) {
            // Csak akkor dekódolunk karaktert, ha legalább 2 elem van, vagy ha 1 elem, akkor az nagyon hosszú vagy rövid (T/E)
            bool decode = false;
            if (toneIndex_ >= 2) {
                decode = true;
            } else if (toneIndex_ == 1) {
                unsigned long dur = rawToneDurations_[0];
                if (dur < (toneMinDurationMs_ * 1.5) || dur > (toneMaxDurationMs_ * 0.8)) {
                    decode = true;
                }
            }
            if (decode) {
                Serial.print(" | KARAKTERHATÁR dekódolva: ");
                // Nyomtatjuk a gyűjtött elemeket is
                Serial.print(" [");
                for (int i = 0; i < toneIndex_; ++i) {
                    Serial.print(rawToneDurations_[i]);
                    if (i < toneIndex_ - 1)
                        Serial.print(",");
                }
                Serial.print("] ");
                char decodedChar = processCollectedElements();
                Serial.print(decodedChar);
                addToBuffer(decodedChar);
                toneIndex_ = 0;
                std::memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
                wordSpaceProcessed_ = false;
                lastGapCheckMicros = lastToneOffMicros_;
            } else {
                Serial.print(" | túl kevés elem, nem dekódol");
            }
        } else {
            // Elemközi szünet debug
            Serial.print(" | elemközi szünet");
        }
    }
    Serial.println();

    // 4. Soros port kimenet: minden új karaktert kiírunk
    char c;
    bool newWordOrLine = false;
    while ((c = getCharacterFromBuffer()) != '\0') {
        Serial.print(c);
        decodedText_ += c;
        if (c == ' ' || c == '\n' || c == '\r')
            newWordOrLine = true;
    }
    // Ha szóköz vagy sortörés volt, írjuk ki a teljes eddigi dekódolt szöveget
    if (newWordOrLine && !decodedText_.empty()) {
        Serial.print("\n[CWDBG] Eddig dekódolt szöveg: ");
        Serial.println(decodedText_.c_str());
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
    const unsigned long ADAPTIVE_WEIGHT_OLD = 3; // Gyorsabb adaptáció
    const unsigned long ADAPTIVE_WEIGHT_NEW = 2;
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
    startReferenceMs_ = 70;    // kb. 70ms pont, 210ms vonás (10-15 WPM, gyorsabb indulás)
    currentReferenceMs_ = 210; // elsőre vonásnak vegye, de gyorsan adaptál
    toneMinDurationMs_ = 70L;
    toneMaxDurationMs_ = 210L;
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
