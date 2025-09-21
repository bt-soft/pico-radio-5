#include "CwDecoder.h"
#include "Config.h"
#include "defines.h"
#include "utils.h"
#include <cmath>

CwDecoder::CwDecoder() {
    pinMode(LED_BUILTIN, OUTPUT); // Initialize LED_BUILTIN
    clear();
}

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
    dotLenMs = 1200.0f; // Extrém lassú CW-hez (~1 WPM) megfelelő dot hossz (1200ms)
    DEBUG("[CW] Init dotLen: %s ms\n", Utils::floatToString(dotLenMs).c_str());
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

    // Még nagyobb keresési ablak a CW jelekhez
    constexpr uint16_t SEARCH_WINDOW_HZ = 600; // +-600 Hz keresési ablak (növelve 400-ról)
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

    // --- Javított Noise level számítása: robusztusabb módszer ---
    float noiseSum = 0.0f;
    int noiseCount = 0;
    // Több bin kihagyása a csúcs körül a pontosabb noise számításhoz
    int excludeRange = 2; // +-2 bin kihagyása a csúcs körül
    for (int i = startBin; i <= endBin; ++i) {
        if (peakBin == -1 || abs(i - peakBin) > excludeRange) {
            noiseSum += fftData[i];
            ++noiseCount;
        }
    }
    float measuredNoise = (noiseCount > 0) ? (noiseSum / noiseCount) : 0.0f;

    // --- Sokkal alacsonyabb küszöbértékek a jobb detektáláshoz ---
    constexpr float NOISE_ALPHA = 0.02f;           // Lassabb zaj adaptáció
    constexpr float SIGNAL_ALPHA = 0.015f;         // Lassabb threshold adaptáció
    constexpr float NOISE_FLOOR_FACTOR_ON = 1.3f;  // Alacsonyabb bekapcsolási küszöb (csökkentve)
    constexpr float NOISE_FLOOR_FACTOR_OFF = 0.9f; // Még alacsonyabb kikapcsolási küszöb (csökkentve)
    constexpr float MINIMUM_THRESHOLD = 3.0f;      // Még alacsonyabb minimum threshold

    // Zaj adaptáció: konzervatívabb, stabilabb
    if (noiseLevel_ == 0.0f) {
        // Inicializálás: első érték beállítása
        noiseLevel_ = measuredNoise;
    } else {
        // Normál adaptáció - kisebb alfa értékekkel
        noiseLevel_ = (1.0f - NOISE_ALPHA) * noiseLevel_ + NOISE_ALPHA * measuredNoise;
    }

    // Threshold adaptáció - konzervatívabb
    float targetThreshold = noiseLevel_ * NOISE_FLOOR_FACTOR_ON;
    if (targetThreshold < MINIMUM_THRESHOLD)
        targetThreshold = MINIMUM_THRESHOLD;

    if (signalThreshold_ == 0.0f)
        signalThreshold_ = targetThreshold;
    else
        signalThreshold_ = (1.0f - SIGNAL_ALPHA) * signalThreshold_ + SIGNAL_ALPHA * targetThreshold;

    // --- Javított jel detektálás logika ---
    constexpr float FREQ_TOLERANCE_HZ = 400.0f;        // Visszaállítjuk 400Hz-re (600->400)
    constexpr float NOISE_THRESHOLD_MULTIPLIER = 2.2f; // Finoman emeljük (2.0->2.2)

    freqInRange_ = std::abs(peakFrequencyHz_ - centerFreqHz) <= FREQ_TOLERANCE_HZ;
    bool peakIsStrong = peakMagnitude_ > measuredNoise * NOISE_THRESHOLD_MULTIPLIER;
    bool aboveOnThreshold = peakMagnitude_ > (noiseLevel_ * NOISE_FLOOR_FACTOR_ON);
    bool aboveOffThreshold = peakMagnitude_ > (noiseLevel_ * NOISE_FLOOR_FACTOR_OFF);

    // Debug üzenet csak állapotváltáskor (nem minden híváskor)
    static bool lastDebugState = false;
    if (freqInRange_ != lastDebugState) {
        DEBUG("[CW] Freq range change: %s Hz, mag: %s, noise: %s, inRange: %d, strong: %d\n", Utils::floatToString(peakFrequencyHz_).c_str(), Utils::floatToString(peakMagnitude_).c_str(),
              Utils::floatToString(noiseLevel_).c_str(), freqInRange_, peakIsStrong);
        lastDebugState = freqInRange_;
    }

    // Javított hiszterézis logika
    if (!prevIsToneDetected) {
        // Jel bekapcsolásához: minden feltételnek teljesülnie kell
        isToneDetected = aboveOnThreshold && peakIsStrong && freqInRange_;
    } else {
        // Jel kikapcsolásához: bármelyik feltétel hiánya elegendő
        isToneDetected = aboveOffThreshold && peakIsStrong && freqInRange_;
    }
}

/**
 * Fő jelfeldolgozó függvény: FFT adatokból morze jelek detektálása és állapotgép futtatása
 * @param fftData FFT amplitúdó tömb
 * @param fftSize FFT méret
 * @param binWidth Frekvencia bin szélesség (Hz)
 */

// --- Stabil edge-counting alapú dekóder (javított) ---
void CwDecoder::processFftData(const float *fftData, uint16_t fftSize, float binWidth) {

    unsigned long now = millis();

    // Debugging: időzítés követése (ritkábban)
    static unsigned long callCount = 0;
    callCount++;
    if (callCount % 500 == 0) { // Minden 500. híváskor (ritkábban)
        DEBUG("[CW] processFftData hívási frekvencia: %lu hívás/500ms\n", callCount);
        callCount = 0;
    }

    detectTone(fftData, fftSize, binWidth);

    // Ha a frekvencia NINCS ablakban, mindig csendként kezeljük, és ha előzőleg hang volt, akkor szimbólumot zárunk
    static bool prevFreqInRange = true;
    if (!freqInRange_) {
        if (prevFreqInRange) {
            // Ha most lépett ki az ablakból, szimbólum lezárása
            if (!currentSymbol.isEmpty()) {
                DEBUG("[CW] Freq out, symbol: %s\n", currentSymbol.c_str());
                char decoded = decodeMorse(currentSymbol);
                DEBUG("Char: %c\n", decoded);
                pushChar(decoded);
                resetSymbol();
            }
        }
        prevFreqInRange = false;

        // Mintavételezés: csak csendet írunk be
        uint8_t sample = 0;
        sampleBuf[sampleHead] = sample;
        sampleHead = (sampleHead + 1) % SAMPLE_BUF_SIZE;
        if (sampleCount < SAMPLE_BUF_SIZE) {
            sampleCount++;
        }

        // Állapotfrissítés - instance változó használata
        if (sample != isToneDetected) {
            unsigned long edgeMs = now;
            unsigned long duration = (lastEdgeMs == 0) ? 0 : (edgeMs - lastEdgeMs);
            lastEdgeMs = edgeMs;
            silenceSamples = duration;
            if (silenceSamples > 0) {
                if (silenceSamples > 7 * dotLenMs) {
                    // DEBUG("Word gap (freq out): %lu ms\n", silenceSamples); // Ritkítva
                    pushChar(' ');
                } else if (silenceSamples > 3 * dotLenMs) {
                    // DEBUG("Inter-char gap (freq out): %lu ms\n", silenceSamples); // Ritkítva
                } else {
                    // DEBUG("Intra-char gap (freq out): %lu ms\n", silenceSamples); // Ritkítva
                }
            }
            toneSamples = 0;
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
    if (sampleCount < SAMPLE_BUF_SIZE) {
        sampleCount++;
    }

    // Edge detektálás: csak akkor dolgozunk, ha változott az állapot (javított instance változó használat)
    if (sample != prevIsToneDetected) {
        unsigned long edgeMs = now;
        unsigned long duration = (lastEdgeMs == 0) ? 0 : (edgeMs - lastEdgeMs);
        lastEdgeMs = edgeMs;

        if (sample == 1) {
            // Silence -> Tone: szünet vége
            silenceSamples = duration;
            if (silenceSamples > 0) {
                // Gap típus eldöntése - extrém lassú CW-hez igazítva
                if (silenceSamples > 4 * dotLenMs) { // Word gap
                    DEBUG("Word gap: %lu ms\n", silenceSamples);
                    pushChar(' ');
                } else if (silenceSamples > 2.0f * dotLenMs) { // Csökkentjük inter-char gap-et (2.5->2.0)
                    DEBUG("Inter-char gap: %lu ms | %s\n", silenceSamples, currentSymbol.c_str());
                    char decoded = decodeMorse(currentSymbol);
                    DEBUG("Decoded: %c\n", decoded);
                    pushChar(decoded);
                    resetSymbol();
                } else {
                    // DEBUG("Intra-char gap: %lu ms\n", silenceSamples); // Ritkítva
                }
            }
            toneSamples = 0;
        } else {
            // Tone -> Silence: hang vége
            toneSamples = duration;
            if (toneSamples > 0) {
                // Adaptív dot/dash küszöb - extrém lassú CW-hez
                float dashThreshold = 2.0f * dotLenMs; // Csökkentjük 2.5 -> 2.0
                if (toneSamples > dashThreshold) {
                    DEBUG("Dash - : %lu ms (th: %s)\n", toneSamples, Utils::floatToString(dashThreshold).c_str());
                    pushSymbol('-');
                    // Dash adaptáció is - ha túl hosszú a dash, növeljük a dotLen-t
                    if (toneSamples > 4.0f * dotLenMs && toneSamples < 12.0f * dotLenMs) { // 4800ms-14400ms tartomány
                        float oldDotLen = dotLenMs;
                        dotLenMs = 0.8f * dotLenMs + 0.2f * (toneSamples / 3.0f); // Dash alapú adaptáció (dash/3 ≈ dot)
                        DEBUG("DotLen (dash adapt): %s -> %s ms\n", Utils::floatToString(oldDotLen).c_str(), Utils::floatToString(dotLenMs).c_str());
                    }
                } else {
                    DEBUG("Dot . : %lu ms (th: %s)\n", toneSamples, Utils::floatToString(dashThreshold).c_str());
                    pushSymbol('.');
                    // Még szélesebb adaptáció extrém lassú CW-hez
                    if (toneSamples > 200 && toneSamples < 10.0f * dotLenMs) { // 200ms-12000ms tartomány
                        float oldDotLen = dotLenMs;
                        dotLenMs = 0.6f * dotLenMs + 0.4f * toneSamples; // Gyorsabb dot adaptáció
                        DEBUG("DotLen: %s -> %s ms\n", Utils::floatToString(oldDotLen).c_str(), Utils::floatToString(dotLenMs).c_str());
                    }
                }
            }
            silenceSamples = 0;
        }
    }
    prevIsToneDetected = isToneDetected;

    // LED villogtatás, ha engedélyezve van a debug és CW jel van
    if (config.data.cwRttyLedDebugEnabled) {
        if (isToneDetected) {
            digitalWrite(LED_BUILTIN, HIGH);
        } else {
            digitalWrite(LED_BUILTIN, LOW);
        }
    }
}

// --- Teljes Morse-fa dekódolás (bővített karakterkészlet) ---
char CwDecoder::decodeMorse(const String &morse) {
    struct MorseNode {
        char c;
        const MorseNode *dot;
        const MorseNode *dash;
    };

    // Teljes morse fa - 63 node-dal, teljes karakterkészlet támogatással
    static const MorseNode morseTree[] = {
        // Index 0: root
        {' ', morseTree + 1, morseTree + 2}, // 0: root

        // Level 1
        {'E', morseTree + 3, morseTree + 4}, // 1: .
        {'T', morseTree + 5, morseTree + 6}, // 2: -

        // Level 2
        {'I', morseTree + 7, morseTree + 8},   // 3: ..
        {'A', morseTree + 9, morseTree + 10},  // 4: .-
        {'N', morseTree + 11, morseTree + 12}, // 5: -.
        {'M', morseTree + 13, morseTree + 14}, // 6: --

        // Level 3
        {'S', morseTree + 15, morseTree + 16}, // 7: ...
        {'U', morseTree + 17, morseTree + 18}, // 8: ..-
        {'R', morseTree + 19, morseTree + 20}, // 9: .-.
        {'W', morseTree + 21, morseTree + 22}, // 10: .--
        {'D', morseTree + 23, morseTree + 24}, // 11: -..
        {'K', morseTree + 25, morseTree + 26}, // 12: -.-
        {'G', morseTree + 27, morseTree + 28}, // 13: --.
        {'O', morseTree + 29, morseTree + 30}, // 14: ---

        // Level 4
        {'H', morseTree + 31, morseTree + 32}, // 15: ....
        {'V', morseTree + 33, morseTree + 34}, // 16: ...-
        {'F', morseTree + 35, morseTree + 36}, // 17: ..-.
        {'?', morseTree + 37, morseTree + 38}, // 18: ..-- (üres)
        {'L', morseTree + 39, morseTree + 40}, // 19: .-..
        {'?', morseTree + 41, morseTree + 42}, // 20: .-.- (üres)
        {'P', morseTree + 43, morseTree + 44}, // 21: .--.
        {'J', morseTree + 45, morseTree + 46}, // 22: .---
        {'B', morseTree + 47, morseTree + 48}, // 23: -...
        {'X', morseTree + 49, morseTree + 50}, // 24: -..-
        {'C', morseTree + 51, morseTree + 52}, // 25: -.-.
        {'Y', morseTree + 53, morseTree + 54}, // 26: -.--
        {'Z', morseTree + 55, morseTree + 56}, // 27: --..
        {'Q', morseTree + 57, morseTree + 58}, // 28: --.-
        {'?', morseTree + 59, morseTree + 60}, // 29: ---. (üres)
        {'?', morseTree + 61, morseTree + 62}, // 30: ---- (üres)

        // Level 5 - számok és speciális karakterek
        {'5', nullptr, nullptr}, // 31: .....
        {'4', nullptr, nullptr}, // 32: ....-
        {'?', nullptr, nullptr}, // 33: ...-. (üres)
        {'3', nullptr, nullptr}, // 34: ...--
        {'?', nullptr, nullptr}, // 35: ..-.. (üres)
        {'?', nullptr, nullptr}, // 36: ..-.- (üres)
        {'?', nullptr, nullptr}, // 37: ..--. (üres)
        {'2', nullptr, nullptr}, // 38: ..---
        {'?', nullptr, nullptr}, // 39: .-..- (üres)
        {'?', nullptr, nullptr}, // 40: .-..+ (üres)
        {'?', nullptr, nullptr}, // 41: .-.-. (üres)
        {'?', nullptr, nullptr}, // 42: .-.-- (üres)
        {'?', nullptr, nullptr}, // 43: .--.. (üres)
        {'?', nullptr, nullptr}, // 44: .--.- (üres)
        {'?', nullptr, nullptr}, // 45: .---. (üres)
        {'1', nullptr, nullptr}, // 46: .----
        {'6', nullptr, nullptr}, // 47: -....
        {'?', nullptr, nullptr}, // 48: -...- (üres)
        {'?', nullptr, nullptr}, // 49: -..-. (üres)
        {'?', nullptr, nullptr}, // 50: -..-- (üres)
        {'?', nullptr, nullptr}, // 51: -.-.. (üres)
        {'?', nullptr, nullptr}, // 52: -.-.+ (üres)
        {'?', nullptr, nullptr}, // 53: -.--. (üres)
        {'?', nullptr, nullptr}, // 54: -.-+- (üres)
        {'7', nullptr, nullptr}, // 55: --...
        {'?', nullptr, nullptr}, // 56: --..- (üres)
        {'?', nullptr, nullptr}, // 57: --.-. (üres)
        {'?', nullptr, nullptr}, // 58: --.-- (üres)
        {'8', nullptr, nullptr}, // 59: ---..
        {'?', nullptr, nullptr}, // 60: ---.- (üres)
        {'9', nullptr, nullptr}, // 61: ----.
        {'0', nullptr, nullptr}, // 62: -----
    };

    const MorseNode *node = &morseTree[0];
    for (size_t i = 0; i < morse.length(); ++i) {
        if (morse[i] == '.') {
            if (!node->dot) {
                DEBUG("[CW] Hiba: '%s' - nincs dot\n", morse.c_str());
                return '?';
            }
            node = node->dot;
        } else if (morse[i] == '-') {
            if (!node->dash) {
                DEBUG("[CW] Hiba: '%s' - nincs dash\n", morse.c_str());
                return '?';
            }
            node = node->dash;
        } else {
            DEBUG("[CW] Hiba: '%s' - rossz kar: %c\n", morse.c_str(), morse[i]);
            return '?';
        }
    }

    char result = node->c;
    if (result == '?') {
        DEBUG("[CW] Morse sikertelen: '%s'\n", morse.c_str());
    } else {
        DEBUG("[CW] Morse: '%s' -> '%c'\n", morse.c_str(), result);
    }
    return result;
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