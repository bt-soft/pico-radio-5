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
}

/**
 * dekódolt szöveg visszaadása
 */
String CwDecoder::getDecodedText() { return ""; }

/**
 * Fő jelfeldolgozó függvény: FFT adatokból morze jelek detektálása és állapotgép futtatása
 * @param fftData FFT amplitúdó tömb
 * @param fftSize FFT méret
 * @param binWidth Frekvencia bin szélesség (Hz)
 */
void CwDecoder::processFftData(const float *fftData, uint16_t fftSize, float binWidth) {

    static uint32_t lastCallMs = 0;
    uint32_t nowMs = millis();
    if (lastCallMs != 0) {
        // DEBUG("CwDecoder::processFftData() delta: %lu ms\n", nowMs - lastCallMs);
    }
    lastCallMs = nowMs;

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
    constexpr float NOISE_FLOOR_FACTOR_ON = 1.07f;  // Jel detektálásához (nagyon érzékeny)
    constexpr float NOISE_FLOOR_FACTOR_OFF = 0.93f; // Jel eltűnéséhez (hiszterézis, érzékeny)
    constexpr float MINIMUM_THRESHOLD = 10.0f;      // Még alacsonyabb minimum

    // Zaj adaptáció: ha a csúcs bin értéke jelentősen nagyobb, gyors reset
    if (noiseLevel_ == 0.0f) {
        // Inicializálás: első érték beállítása
        noiseLevel_ = measuredNoise;
    } else if (peakMagnitude_ > measuredNoise * 2.0f) {
        // Ha a jel kiugróan nagy, gyors reset a zajszintre
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

    // Hiszterézis: külön threshold a be- és kikapcsolásra
    static bool prevIsToneDetected = false;
    bool isToneDetected;

    // Feltételek külön változókban, olvashatóbb logika
    constexpr float FREQ_TOLERANCE_HZ = 50.0f;         // tolerancia a frekvencia eltérésre
    constexpr float NOISE_THRESHOLD_MULTIPLIER = 3.0f; // legalább 3x a zaj szintjéhez képest a jel

    bool freqInRange = std::abs(peakFrequencyHz_ - centerFreqHz) <= FREQ_TOLERANCE_HZ;
    bool peakIsStrong = peakMagnitude_ > measuredNoise * NOISE_THRESHOLD_MULTIPLIER;
    bool aboveOnThreshold = peakMagnitude_ > (noiseLevel_ * NOISE_FLOOR_FACTOR_ON);
    bool aboveOffThreshold = peakMagnitude_ > (noiseLevel_ * NOISE_FLOOR_FACTOR_OFF);

    if (!prevIsToneDetected) {
        // Jel bekapcsolásához: threshold felett, kiugró csúcs, frekvencia ablakban
        isToneDetected = aboveOnThreshold && peakIsStrong && freqInRange;
    } else {
        // Jel kikapcsolásához: threshold alatt, kiugró csúcs, frekvencia ablakban
        isToneDetected = aboveOffThreshold && peakIsStrong && freqInRange;
    }

    prevIsToneDetected = isToneDetected;
    DEBUG("Tone: %s, peak Frequency: %s Hz, binWidth: %s Hz, Magnitude: %s, Noise Level: %s, Threshold: %s\n", isToneDetected ? "true" : "false", Utils::floatToString(peakFrequencyHz_).c_str(),
          Utils::floatToString(binWidth).c_str(), Utils::floatToString(peakMagnitude_).c_str(), Utils::floatToString(noiseLevel_).c_str(), Utils::floatToString(signalThreshold_).c_str());
}