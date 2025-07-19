#include "CwDecoder.h"
#include "Config.h"
#include "defines.h"
#include "utils.h"
#include <cmath>

/**
 * Statikus morze tábla inicializálása a class-on kívül
 * Ez a tábla egy egyszerűsített példa; egy teljes morze tábla ennél jóval nagyobb lenne.
 */
const std::map<String, char> CwDecoder::morseTable_ = {{".-", 'A'},    {"-...", 'B'},  {"-.-.", 'C'},  {"-..", 'D'},   {".", 'E'},     {"..-.", 'F'},  {"--.", 'G'},   {"....", 'H'},  {"..", 'I'},
                                                       {".---", 'J'},  {"-.-", 'K'},   {".-..", 'L'},  {"--", 'M'},    {"-.", 'N'},    {"---", 'O'},   {".--.", 'P'},  {"--.-", 'Q'},  {".-.", 'R'},
                                                       {"...", 'S'},   {"-", 'T'},     {"..-", 'U'},   {"...-", 'V'},  {".--", 'W'},   {"-..-", 'X'},  {"-.--", 'Y'},  {"--..", 'Z'},  {"-----", '0'},
                                                       {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"....-", '4'}, {".....", '5'}, {"-....", '6'}, {"--...", '7'}, {"---..", '8'}, {"----.", '9'}};
/**
 * Konstruktor: minden állapotot alaphelyzetbe állít
 */

CwDecoder::CwDecoder() { clear(); }

/**
 * Minden állapot és változó alaphelyzetbe állítása
 */

void CwDecoder::clear() {
    currentState_ = DecoderState::IDLE;
    lastStateChangeMs_ = millis(); // Inicializálás az aktuális idővel
    decodedText_ = "";
    currentMorseChar_ = "";

    peakFrequencyHz_ = 0.0f;
    peakMagnitude_ = 0.0f;
    noiseLevel_ = 0.0f;
    signalThreshold_ = 1000.0f; // Kezdeti küszöbszint

    bitTimeMs_ = 120.0f; // Alapértelmezett pont idő (kb. 15 WPM)
    wpm_ = 0.0f;         // Később számoljuk

    bitTimeInitialized_ = false;
    bitTimeInitCount_ = 0;
    bitTimeInitSum_ = 0.0f;
    bitTimeInitStartMs_ = 0;
}

/**
 * dekódolt szöveg visszaadása
 */
String CwDecoder::getDecodedText() { return decodedText_; }

/**
 * Hibakeresési információk összegyűjtése, állapot, jelszintek, morze stb.
 */
String CwDecoder::getDebugInfo() const {
    String debug = "State: ";
    switch (currentState_) {
        case DecoderState::IDLE:
            debug += "IDLE";
            break;
        case DecoderState::TONE:
            debug += "TONE";
            break;
        case DecoderState::SILENCE_INTRA_CHAR:
            debug += "SILENCE_INTRA_CHAR";
            break;
        case DecoderState::SILENCE_CHAR_SPACE:
            debug += "SILENCE_CHAR_SPACE";
            break;
        case DecoderState::SILENCE_WORD_SPACE:
            debug += "SILENCE_WORD_SPACE";
            break;
    }
    debug += ", Last State Change: " + String(lastStateChangeMs_) + "ms";
    debug += ", Peak Freq: " + Utils::floatToString(peakFrequencyHz_) + "Hz";
    debug += ", Peak Mag: " + Utils::floatToString(peakMagnitude_);
    debug += ", Noise Level: " + Utils::floatToString(noiseLevel_);
    debug += ", Signal Threshold: " + Utils::floatToString(signalThreshold_);
    debug += ", Bit Time: " + Utils::floatToString(bitTimeMs_) + "ms";
    debug += ", WPM: " + Utils::floatToString(wpm_);
    debug += ", Current Morse: " + currentMorseChar_;
    debug += ", Decoded: " + decodedText_;
    return debug;
}

/**
 * Egy adott időtartam osztályozása (pont, vonal, szünet stb.)
 * @param duration Időtartam (ms)
 * @param isToneDuration Hang (true) vagy szünet (false)
 * @return Osztályozott morze időzítés
 *
 * Ezek az arányok a szabványos morze időzítést követik:
 * Pont = 1 egység, Vonal = 3 egység
 * Elemközi szünet = 1 egység
 * Karakterközi szünet = 3 egység
 * Szóköz = 7 egység
 */
CwDecoder::MorseTiming CwDecoder::classifyDuration(unsigned long duration, bool isToneDuration) {
    // Ezek az arányok a szabványos morze időzítést követik.
    // Pont = 1 egység, Vonal = 3 egység
    // Elemközi szünet = 1 egység
    // Karakterközi szünet = 3 egység
    // Szóköz = 7 egység

    if (duration < (bitTimeMs_ * 0.3f)) { // Itt a nagyon rövideket is kiszűrjük (valószínűleg zaj)
        return MorseTiming::TOO_SHORT;
    }

    if (isToneDuration) {
        // PONT: 0.5–1.5, VONAL: 1.5–4.0
        float ratio = (float)duration / bitTimeMs_;
        if (ratio >= 0.5f && ratio < 1.5f) {
            return MorseTiming::DOT;
        } else if (ratio >= 1.5f && ratio < 4.0f) {
            return MorseTiming::DASH;
        } else {
            return MorseTiming::DASH; // Ha túl hosszú, akkor is vonalnak vesszük
        }
    } else {
        // ELEM KÖZTI SZÜNET: 0.5–2.0, KARAKTERKÖZI: 2.0–5.0, SZÓKÖZ: 5.0+
        float ratio = (float)duration / bitTimeMs_;
        if (ratio >= 0.5f && ratio < 2.0f) {
            return MorseTiming::INTRA_CHAR_SPACE;
        } else if (ratio >= 2.0f && ratio < 5.0f) {
            return MorseTiming::CHAR_SPACE;
        } else if (ratio >= 5.0f) {
            return MorseTiming::WORD_SPACE;
        }
        return MorseTiming::TOO_SHORT;
    }
}

/**
 * Időzítés szövegesen (debug célra)
 * @param timing Osztályozott morze időzítés
 */
String CwDecoder::getMorseTimingString(MorseTiming timing) const {
    switch (timing) {
        case MorseTiming::TOO_SHORT:
            return "TOO_SHORT";
        case MorseTiming::DOT:
            return "DOT";
        case MorseTiming::DASH:
            return "DASH";
        case MorseTiming::INTRA_CHAR_SPACE:
            return "INTRA_CHAR_SPACE";
        case MorseTiming::CHAR_SPACE:
            return "CHAR_SPACE";
        case MorseTiming::WORD_SPACE:
            return "WORD_SPACE";
        default:
            return "UNKNOWN";
    }
}

/**
 * Pont idő (bitTime) adaptáció, automatikus WPM követés
 * @param duration Aktuális hang időtartama (ms)
 * @return Frissített bitTime érték (ms)
 */
float CwDecoder::updateAndGetBitTime(unsigned long duration) {
    static const float ALPHA = 0.10f; // Smoothing factor for adaptation

    if (!bitTimeInitialized_) {
        // Kezdeti bitTime: engedj nagyobb szórást, szélesebb tartományt
        if (duration >= 50 && duration <= 400) {
            unsigned long nowMs = millis();
            if (bitTimeInitCount_ == 0) {
                bitTimeInitStartMs_ = nowMs;
            }

            // Reset, ha túl sok idő telt el
            if (bitTimeInitCount_ > 0 && (nowMs - bitTimeInitStartMs_ > 4000)) {
                DEBUG("CW: bitTime init timeout, resetting.\n");
                bitTimeInitCount_ = 0;
                bitTimeInitSum_ = 0.0f;
                bitTimeInitStartMs_ = nowMs;
            }

            // Engedj nagyobb szórást az első mintákra
            float currentAvg = (bitTimeInitCount_ == 0) ? duration : (bitTimeInitSum_ / (float)bitTimeInitCount_);
            if (bitTimeInitCount_ == 0 || fabsf(duration - currentAvg) < currentAvg * 0.55f) {
                bitTimeInitSum_ += duration;
                bitTimeInitCount_++;
                DEBUG("CW: bitTime init sample %u: %lu ms, sum: %s\n", bitTimeInitCount_, duration, Utils::floatToString(bitTimeInitSum_).c_str());
            } else {
                DEBUG("CW: bitTime init sample ignored (too far from avg): %lu ms, avg: %s\n", duration, Utils::floatToString(currentAvg).c_str());
            }

            if (bitTimeInitCount_ >= 4) {
                bitTimeMs_ = bitTimeInitSum_ / (float)bitTimeInitCount_;
                if (bitTimeMs_ < 40.0f)
                    bitTimeMs_ = 40.0f;
                if (bitTimeMs_ > 400.0f)
                    bitTimeMs_ = 400.0f;
                bitTimeInitialized_ = true;
                DEBUG("CW: bitTime initialized (avg of %u samples): %s ms\n", bitTimeInitCount_, Utils::floatToString(bitTimeMs_).c_str());
                wpm_ = 1200.0f / bitTimeMs_;
            }
        } else {
            DEBUG("CW: Bit time init: Duration %lu ms out of initial range.\n", duration);
        }
    } else {
        // Adaptáció: szélesebb tartományban engedjük
        float ratio = (float)duration / bitTimeMs_;
        if (duration >= 40 && ratio > 0.4f && ratio < 5.0f) {
            bitTimeMs_ = (1.0f - ALPHA) * bitTimeMs_ + ALPHA * duration;
            if (bitTimeMs_ < 40.0f)
                bitTimeMs_ = 40.0f;
            if (bitTimeMs_ > 400.0f)
                bitTimeMs_ = 400.0f;
            wpm_ = 1200.0f / bitTimeMs_;
            DEBUG("CW: bitTime adapted: %s ms, WPM: %s\n", Utils::floatToString(bitTimeMs_).c_str(), Utils::floatToString(wpm_).c_str());
        } else {
            DEBUG("CW: Bit time adaptation ignored: duration %lu ms, ratio %s\n", duration, Utils::floatToString(ratio).c_str());
        }
    }
    return bitTimeMs_;
}

/**
 * Az aktuális morze karakter dekódolása és hozzáfűzése a szöveghez
 */
void CwDecoder::decodeMorseChar() {
    if (currentMorseChar_.length() == 0)
        return;

    auto it = morseTable_.find(currentMorseChar_);
    if (it != morseTable_.end()) {
        decodedText_ += it->second;
        DEBUG("CW: Decoded char: '%c' from '%s'\n", it->second, currentMorseChar_.c_str());
    } else {
        decodedText_ += '?'; // Unknown character
        DEBUG("CW: Unknown Morse sequence: '%s'\n", currentMorseChar_.c_str());
    }
    currentMorseChar_ = ""; // Reset for next character
}

/**
 * Állapotgép: IDLE állapot kezelése
 * @param isToneDetected Detektáltunk-e jelet
 * @param now Aktuális idő (ms)
 */
void CwDecoder::handleIdleState(bool isToneDetected, unsigned long now) {
    if (isToneDetected) {
        DEBUG("CW: IDLE -> TONE\n");
        currentState_ = DecoderState::TONE;
        lastStateChangeMs_ = now;
    }
    // If not detected, stay in IDLE
}

/**
 * Állapotgép: TONE állapot kezelése
 * @param isToneDetected Detektáltunk-e jelet
 * @param now Aktuális idő (ms)
 */
void CwDecoder::handleToneState(bool isToneDetected, unsigned long now) {
    if (!isToneDetected) {
        unsigned long duration = now - lastStateChangeMs_;
        MorseTiming timing = classifyDuration(duration, true);
        DEBUG("CW: TONE -> SILENCE. Tone duration: %lu ms (%s)\n", duration, getMorseTimingString(timing).c_str());

        updateAndGetBitTime(duration); // Always update bitTime based on tone duration

        if (timing == MorseTiming::DOT) {
            currentMorseChar_ += ".";
        } else if (timing == MorseTiming::DASH) {
            currentMorseChar_ += "-";
        } else {
            // If it's too short or unclassified tone, potentially noise, ignore.
            // Or decide to append '?' if strict, for now just ignore.
            DEBUG("CW: Ignored short/unclassified tone. Clearing current Morse.\n");
            currentMorseChar_ = ""; // Reset if invalid tone
        }

        currentState_ = DecoderState::SILENCE_INTRA_CHAR; // Default to smallest silence
        lastStateChangeMs_ = now;
    }
    // If tone detected, stay in TONE, extending the current tone duration
}

/**
 * Állapotgép: SILENCE állapotok kezelése
 * @param isToneDetected Detektáltunk-e jelet
 * @param now Aktuális idő (ms)
 */
void CwDecoder::handleSilenceState(bool isToneDetected, unsigned long now) {
    if (isToneDetected) {
        // Silence period ended, a new tone started
        unsigned long duration = now - lastStateChangeMs_;
        MorseTiming timing = classifyDuration(duration, false);
        DEBUG("CW: SILENCE -> TONE. Silence duration: %lu ms (%s)\n", duration, getMorseTimingString(timing).c_str());

        if (timing == MorseTiming::CHAR_SPACE || timing == MorseTiming::WORD_SPACE) {
            // Character or word space detected, decode the accumulated Morse character
            decodeMorseChar();
            if (timing == MorseTiming::WORD_SPACE) {
                decodedText_ += " "; // Add space for word separation
                DEBUG("CW: Added word space.\n");
            }
        }
        // If it's an intra-character space, no decode yet, just continue building morseChar.

        currentState_ = DecoderState::TONE;
        lastStateChangeMs_ = now;
    } else {
        // Still in silence, potentially transition to longer silence state
        unsigned long duration = now - lastStateChangeMs_;
        MorseTiming currentSilenceTiming = classifyDuration(duration, false);

        if (currentState_ == DecoderState::SILENCE_INTRA_CHAR && currentSilenceTiming == MorseTiming::CHAR_SPACE) {
            DEBUG("CW: SILENCE_INTRA_CHAR -> SILENCE_CHAR_SPACE\n");
            currentState_ = DecoderState::SILENCE_CHAR_SPACE;
            // Do not update lastStateChangeMs_ here, it's cumulative silence
        } else if ((currentState_ == DecoderState::SILENCE_INTRA_CHAR || currentState_ == DecoderState::SILENCE_CHAR_SPACE) && currentSilenceTiming == MorseTiming::WORD_SPACE) {
            DEBUG("CW: SILENCE -> SILENCE_WORD_SPACE\n");
            currentState_ = DecoderState::SILENCE_WORD_SPACE;
            // Do not update lastStateChangeMs_ here
        }
        // Stay in current silence state if condition not met for transition
    }
}

/**
 * Fő jelfeldolgozó függvény: FFT adatokból morze jelek detektálása és állapotgép futtatása
 * @param fftData FFT amplitúdó tömb
 * @param fftSize FFT méret
 * @param binWidth Frekvencia bin szélesség (Hz)
 */
void CwDecoder::processFftData(const float *fftData, uint16_t fftSize, float binWidth) {
    unsigned long nowMs = millis();
    uint16_t centerFreqHz = config.data.cwReceiverOffsetHz;
    constexpr uint16_t SEARCH_WINDOW_HZ = 200; // Search 200Hz above/below center
    uint16_t startFreqHz = (centerFreqHz > SEARCH_WINDOW_HZ) ? (centerFreqHz - SEARCH_WINDOW_HZ) : 0;
    uint16_t endFreqHz = centerFreqHz + SEARCH_WINDOW_HZ;

    int startBin = static_cast<int>(startFreqHz / binWidth);
    int endBin = static_cast<int>(endFreqHz / binWidth);
    if (endBin >= (int)fftSize / 2) {
        endBin = (fftSize / 2) - 1; // Ensure within bounds
    }

    float maxMagnitude = 0.0f;
    int peakBin = -1;
    for (int i = startBin; i <= endBin; ++i) {
        if (fftData[i] > maxMagnitude) {
            maxMagnitude = fftData[i];
            peakBin = i;
        }
    }

    peakMagnitude_ = maxMagnitude;
    peakFrequencyHz_ = (peakBin != -1) ? (peakBin * binWidth) : 0.0f;

    // --- Adaptive Noise Level and Signal Threshold Calculation (javított) ---
    static const float NOISE_ALPHA = 0.15f;            // Még gyorsabb zaj adaptáció
    static const float SIGNAL_ALPHA = 0.07f;           // Gyorsabb threshold adaptáció
    static const float NOISE_FLOOR_FACTOR_ON = 1.25f;  // Jel detektálásához (érzékenyebb)
    static const float NOISE_FLOOR_FACTOR_OFF = 1.05f; // Jel eltűnéséhez (hiszterézis, érzékenyebb)
    static const float MINIMUM_THRESHOLD = 80.0f;      // Még alacsonyabb minimum

    // Zaj adaptáció
    if (peakMagnitude_ < 2000.0f) { // Csak akkor adaptáljuk, ha nincs erős jel
        if (noiseLevel_ == 0.0f)
            noiseLevel_ = peakMagnitude_;
        else
            noiseLevel_ = (1.0f - NOISE_ALPHA) * noiseLevel_ + NOISE_ALPHA * peakMagnitude_;
    } else {
        // Erős jel esetén a zaj ne nőjön meg hirtelen
        noiseLevel_ = (1.0f - NOISE_ALPHA) * noiseLevel_ + NOISE_ALPHA * (peakMagnitude_ * 0.05f);
    }

    // Threshold adaptáció
    float targetThreshold = noiseLevel_ * NOISE_FLOOR_FACTOR_ON;
    if (targetThreshold < MINIMUM_THRESHOLD)
        targetThreshold = MINIMUM_THRESHOLD;

    if (signalThreshold_ == 0.0f)
        signalThreshold_ = targetThreshold;
    else
        signalThreshold_ = (1.0f - SIGNAL_ALPHA) * signalThreshold_ + SIGNAL_ALPHA * targetThreshold;

    // Hiszterézis: külön threshold a be- és kikapcsolásra
    static bool prevToneDetected = false;
    bool isToneDetected;
    if (!prevToneDetected) {
        // Jel bekapcsolásához magasabb threshold
        isToneDetected = (peakMagnitude_ > (noiseLevel_ * NOISE_FLOOR_FACTOR_ON));
    } else {
        // Jel kikapcsolásához alacsonyabb threshold
        isToneDetected = (peakMagnitude_ > (noiseLevel_ * NOISE_FLOOR_FACTOR_OFF));
    }
    prevToneDetected = isToneDetected;

    // DEBUG("CW: (Tick) State: %s, Tone: %s, Mag: %s, Thresh: %s, Noise: %s\n", getDebugInfo().c_str(), isToneDetected ? "TRUE" : "FALSE", Utils::floatToString(peakMagnitude_).c_str(),
    //       Utils::floatToString(signalThreshold_).c_str(), Utils::floatToString(noiseLevel_).c_str());

    // --- State Machine Logic ---
    switch (currentState_) {
        case DecoderState::IDLE:
            handleIdleState(isToneDetected, nowMs);
            break;
        case DecoderState::TONE:
            handleToneState(isToneDetected, nowMs);
            break;
        case DecoderState::SILENCE_INTRA_CHAR:
        case DecoderState::SILENCE_CHAR_SPACE:
        case DecoderState::SILENCE_WORD_SPACE:
            handleSilenceState(isToneDetected, nowMs);
            break;
    }
}