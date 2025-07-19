#include "CwDecoder.h"
#include "Config.h"
#include "defines.h"
#include "utils.h"
#include <cmath>

CwDecoder::CwDecoder() { clear(); }

/**
 * Minden állapot és változó alaphelyzetbe állítása
 */

void CwDecoder::clear() {
    peakFrequencyHz_ = 0.0f;
    peakMagnitude_ = 0.0f;
    noiseLevel_ = 0.0f;
    signalThreshold_ = 0.0f;
    prevIsToneDetected = false;
    isToneDetected = false;
    decodedText = "";
    currentSymbol = "";
    lastEdgeMs = 0;
    toneSamples = 0;
    silenceSamples = 0;
    dotLenMs = 120.0f;
    sampleHead = 0;
    sampleCount = 0;
    freqInRange_ = false;
    memset(sampleBuf, 0, sizeof(sampleBuf));
}

/**
 * dekódolt szöveg visszaadása
 */
String CwDecoder::getDecodedText() { return decodedText; }

void CwDecoder::detectTone(const float *fftData, uint16_t fftSize, float binWidth) {

    // Lekérjük CW a középfrekvenciát a konfigurációból
    uint16_t centerFreqHz = config.data.cwReceiverOffsetHz;

    constexpr uint16_t SEARCH_WINDOW_HZ = 200; // +-200 Hz keresési ablak a bin-ek körül
    uint16_t startFreqHz = (centerFreqHz > SEARCH_WINDOW_HZ) ? (centerFreqHz - SEARCH_WINDOW_HZ) : 0;
    uint16_t endFreqHz = centerFreqHz + SEARCH_WINDOW_HZ;

    // Keresés a megadott frekvencia ablakban
    int startBin = static_cast<int>(startFreqHz / binWidth);
    int endBin = static_cast<int>(endFreqHz / binWidth);
    if (endBin >= (int)fftSize / 2) {
        endBin = (fftSize / 2) - 1; // Ne lépjünk túl a bin határokon
    }

    // Keresés a legnagyobb amplitúdójú frekvenciára a megadott ablakban
    float maxMagnitude = 0.0f;
    int peakBin = -1;
    for (int i = startBin; i <= endBin; ++i) {
        if (fftData[i] > maxMagnitude) {
            maxMagnitude = fftData[i];
            peakBin = i;
        }
    }
    peakMagnitude_ = maxMagnitude;                                    // Legnagyobb amplitúdó érték
    peakFrequencyHz_ = (peakBin != -1) ? (peakBin * binWidth) : 0.0f; // Detektált csúcsfrekvencia

    // --- Noise level számítása: ablak összes binjének átlaga, csúcs bin kihagyásával ---
    float noiseSum = 0.0f;
    int noiseCount = 0;
    for (int i = startBin; i <= endBin; ++i) {
        if (i != peakBin) {
            noiseSum += fftData[i];
            ++noiseCount;
        }
    }
    float measuredNoise = (noiseCount > 0) ? (noiseSum / noiseCount) : 0.0f;

    // --- Adaptive Noise Level and Signal Threshold Calculation (javított) ---
    constexpr float NOISE_ALPHA = 0.035f;           // Lassabb zaj adaptáció (alap)
    constexpr float SIGNAL_ALPHA = 0.025f;          // Lassabb threshold adaptáció
    constexpr float NOISE_FLOOR_FACTOR_ON = 1.15f;  // Jel detektálásához (hiszterézis, stabilabb)
    constexpr float NOISE_FLOOR_FACTOR_OFF = 0.85f; // Jel eltűnéséhez (hiszterézis, stabilabb)
    constexpr float MINIMUM_THRESHOLD = 10.0f;      // Még alacsonyabb minimum

    // Zaj adaptáció: ha a csúcs bin értéke extrém nagy, gyors reset (4x!)
    if (noiseLevel_ == 0.0f) {
        // Inicializálás: első érték beállítása
        noiseLevel_ = measuredNoise;
    } else if (peakMagnitude_ > measuredNoise * 4.0f) {
        // Ha a jel extrém kiugró, gyors reset a zajszintre
        noiseLevel_ = measuredNoise;
    } else {
        // Gyorsabb lefelé adaptáció (NOISE_ALPHA * 2)
        float alpha = NOISE_ALPHA * 2.0f;
        if (alpha > 1.0f)
            alpha = 1.0f;
        noiseLevel_ = (1.0f - alpha) * noiseLevel_ + alpha * measuredNoise;
    }

    // Threshold adaptáció
    float targetThreshold = noiseLevel_ * NOISE_FLOOR_FACTOR_ON;
    if (targetThreshold < MINIMUM_THRESHOLD)
        targetThreshold = MINIMUM_THRESHOLD;

    if (signalThreshold_ == 0.0f)
        signalThreshold_ = targetThreshold;
    else
        signalThreshold_ = (1.0f - SIGNAL_ALPHA) * signalThreshold_ + SIGNAL_ALPHA * targetThreshold;

    // Feltételek külön változókban, olvashatóbb logika
    constexpr float FREQ_TOLERANCE_HZ = 120.0f;        // tolerancia a frekvencia eltérésre (közepes)
    constexpr float NOISE_THRESHOLD_MULTIPLIER = 2.0f; // legalább 2x a zaj szintjéhez képest a jel (stabilabb)
    // Hiszterézis visszaállítása
    freqInRange_ = std::abs(peakFrequencyHz_ - centerFreqHz) <= FREQ_TOLERANCE_HZ;
    bool peakIsStrong = peakMagnitude_ > measuredNoise * NOISE_THRESHOLD_MULTIPLIER;
    bool aboveOnThreshold = peakMagnitude_ > (noiseLevel_ * NOISE_FLOOR_FACTOR_ON);
    bool aboveOffThreshold = peakMagnitude_ > (noiseLevel_ * NOISE_FLOOR_FACTOR_OFF);

    // DEBUG minden kritikus értékre
    // DEBUG("[CW] peakFreq: %s Hz, peakMag: %s, noise: %s, th_on: %s, th_off: %s, freqInRange: %d, peakIsStrong: %d\n", Utils::floatToString(peakFrequencyHz_).c_str(), Utils::floatToString(peakMagnitude_).c_str(),
    //       Utils::floatToString(noiseLevel_).c_str(), Utils::floatToString(noiseLevel_ * NOISE_FLOOR_FACTOR_ON).c_str(), Utils::floatToString(noiseLevel_ * NOISE_FLOOR_FACTOR_OFF).c_str(), freqInRange, peakIsStrong);

    if (!prevIsToneDetected) {
        // Jel bekapcsolásához: threshold felett, kiugró csúcs, frekvencia ablakban
        isToneDetected = aboveOnThreshold && peakIsStrong && freqInRange_;
    } else {
        // Jel kikapcsolásához: threshold alatt, kiugró csúcs, frekvencia ablakban
        isToneDetected = aboveOffThreshold && peakIsStrong && freqInRange_;
    }
}

/**
 * Fő jelfeldolgozó függvény: FFT adatokból morze jelek detektálása és állapotgép futtatása
 * @param fftData FFT amplitúdó tömb
 * @param fftSize FFT méret
 * @param binWidth Frekvencia bin szélesség (Hz)
 */

// --- Stabil edge-counting alapú dekóder ---
void CwDecoder::processFftData(const float *fftData, uint16_t fftSize, float binWidth) {

     unsigned long now = millis();
     
    // static unsigned long lastNow = now;
    // if (now != lastNow) {
    //     DEBUG("[CW]  dt: %lu ms\n", now - lastNow);
    //     lastNow = now;
    // }

    detectTone(fftData, fftSize, binWidth);

    // Ha a frekvencia NINCS ablakban, mindig csendként kezeljük, és ha előzőleg hang volt, akkor szimbólumot zárunk
    static bool prevFreqInRange = true;
    if (!freqInRange_) {
        if (prevFreqInRange) {
            // Ha most lépett ki az ablakból, szimbólum lezárása
            if (!currentSymbol.isEmpty()) {
                DEBUG("[CW] Frekvencia kilépett az ablakból, szimbólum lezárva: %s\n", currentSymbol.c_str());
                char decoded = decodeMorse(currentSymbol);
                DEBUG("Decoded char: %c\n", decoded);
                pushChar(decoded);
                resetSymbol();
            }
        }
        prevFreqInRange = false;
        // Mintavételezés: csak csendet írunk be
        uint8_t sample = 0;
        sampleBuf[sampleHead] = sample;
        sampleHead = (sampleHead + 1) % SAMPLE_BUF_SIZE;
        if (sampleCount < SAMPLE_BUF_SIZE)
            sampleCount++;
        // Állapotfrissítés
        static bool lastSample = 0;
        if (sample != lastSample) {
            unsigned long edgeMs = now;
            unsigned long duration = (lastEdgeMs == 0) ? 0 : (edgeMs - lastEdgeMs);
            lastEdgeMs = edgeMs;
            silenceSamples = duration;
            if (silenceSamples > 0) {
                if (silenceSamples > 7 * dotLenMs) {
                    DEBUG("Word gap (freq out): %lu ms\n", silenceSamples);
                    pushChar(' ');
                } else if (silenceSamples > 3 * dotLenMs) {
                    DEBUG("Inter-char gap (freq out): %lu ms\n", silenceSamples);
                } else {
                    DEBUG("Intra-char gap (freq out): %lu ms\n", silenceSamples);
                }
            }
            toneSamples = 0;
            lastSample = sample;
        }
        prevIsToneDetected = false;
        return;
    } else {
        prevFreqInRange = true;
    }

    // Mintavételezés: minden híváskor 1 (tone) vagy 0 (silence) sample-t teszünk a FIFO-ba
    uint8_t sample = isToneDetected ? 1 : 0;
    sampleBuf[sampleHead] = sample;
    sampleHead = (sampleHead + 1) % SAMPLE_BUF_SIZE;
    if (sampleCount < SAMPLE_BUF_SIZE)
        sampleCount++;

    // Edge detektálás: csak akkor dolgozunk, ha változott az állapot
    static bool lastSample = 0;
    if (sample != lastSample) {
        unsigned long edgeMs = now;
        unsigned long duration = (lastEdgeMs == 0) ? 0 : (edgeMs - lastEdgeMs);
        lastEdgeMs = edgeMs;

        if (sample == 1) {
            // Silence -> Tone: szünet vége
            silenceSamples = duration;
            if (silenceSamples > 0) {
                // Gap típus eldöntése
                if (silenceSamples > 7 * dotLenMs) {
                    DEBUG("Word gap: %lu ms\n", silenceSamples);
                    pushChar(' ');
                } else if (silenceSamples > 3 * dotLenMs) {
                    DEBUG("Inter-char gap: %lu ms | morze: %s\n", silenceSamples, currentSymbol.c_str());
                    char decoded = decodeMorse(currentSymbol);
                    DEBUG("Decoded char: %c\n", decoded);
                    pushChar(decoded);
                    resetSymbol();
                } else {
                    DEBUG("Intra-char gap: %lu ms\n", silenceSamples);
                }
            }
            toneSamples = 0;
        } else {
            // Tone -> Silence: hang vége
            toneSamples = duration;
            if (toneSamples > 0) {
                if (toneSamples > 2.8f * dotLenMs) {
                    DEBUG("Dash - : %lu ms (dotLen: %s)\n", toneSamples, Utils::floatToString(dotLenMs).c_str());
                    pushSymbol('-');
                } else {
                    DEBUG("Dot . : %lu ms (dotLen: %s)\n", toneSamples, Utils::floatToString(dotLenMs).c_str());
                    pushSymbol('.');
                    // dotLen adaptáció: csak rövid hangokra
                    if (toneSamples < 2.0f * dotLenMs) {
                        dotLenMs = 0.93f * dotLenMs + 0.07f * toneSamples;
                    }
                }
            }
            silenceSamples = 0;
        }
        lastSample = sample;
    }
    prevIsToneDetected = isToneDetected;
}

// --- Morse-fa dekódolás ---
char CwDecoder::decodeMorse(const String &morse) {
    struct MorseNode {
        char c;
        const MorseNode *dot;
        const MorseNode *dash;
    };
    static const MorseNode morseTree[] = {
        // c, dot, dash
        {' ', morseTree + 1, morseTree + 2},   // 0: root
        {'E', morseTree + 3, morseTree + 4},   // 1: .
        {'T', morseTree + 5, morseTree + 6},   // 2: -
        {'I', morseTree + 7, morseTree + 8},   // 3: ..
        {'A', morseTree + 9, morseTree + 10},  // 4: .-
        {'N', morseTree + 11, morseTree + 12}, // 5: -.
        {'M', morseTree + 13, morseTree + 14}, // 6: --
        {'S', nullptr, nullptr},               // 7: ...
        {'U', nullptr, nullptr},               // 8: ..-
        {'R', nullptr, nullptr},               // 9: .-.
        {'W', nullptr, nullptr},               // 10: .--
        {'D', nullptr, nullptr},               // 11: -..
        {'K', nullptr, nullptr},               // 12: -.-
        {'G', nullptr, nullptr},               // 13: --.
        {'O', nullptr, nullptr},               // 14: ---
    };
    const MorseNode *node = &morseTree[0];
    for (size_t i = 0; i < morse.length(); ++i) {
        if (morse[i] == '.') {
            if (!node->dot)
                return '?';
            node = node->dot;
        } else if (morse[i] == '-') {
            if (!node->dash)
                return '?';
            node = node->dash;
        } else {
            return '?';
        }
    }
    return node->c;
}

void CwDecoder::pushSymbol(char symbol) { currentSymbol += symbol; }

void CwDecoder::pushChar(char c) {
    if (c == ' ') {
        decodedText += ' ';
    } else if (c != '?' && c != '\0') {
        decodedText += c;
    }
}

void CwDecoder::resetSymbol() { currentSymbol = ""; }