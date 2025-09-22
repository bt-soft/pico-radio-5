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
void CwDecoder::clear() {}

/**
 * dekódolt szöveg visszaadása
 */
String CwDecoder::getDecodedText() { return ""; }

/**
 * Detektálja a CW hangot az FFT adatok alapján
 * @param fftData FFT amplitúdó tömb
 * @param fftSize FFT méret
 * @param binWidth Frekvencia bin szélesség (Hz)
 */
bool CwDecoder::detectTone(const float *fftData, uint16_t fftSize, float binWidth) {

    // Lekérjük CW a középfrekvenciát a konfigurációból
    uint16_t centerFreqHz = config.data.cwToneFrequencyHz;

    // Még nagyobb keresési ablak a CW jelekhez
    constexpr uint16_t SEARCH_WINDOW_HZ = 200; // +-200 Hz keresési ablak
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

    float peakMagnitude_ = maxMagnitude;                                    // Legnagyobb amplitúdó érték
    float peakFrequencyHz_ = (peakBin != -1) ? (peakBin * binWidth) : 0.0f; // Detektált csúcsfrekvencia

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

    // Kiszámítjuk a mért zajszintet
    float measuredNoise = (noiseCount > 0) ? (noiseSum / noiseCount) : 0.0f;

    // Egyszerű SNR számítás
    float snrDb = 10.0f * log10(peakMagnitude_ / (measuredNoise + 1e-6f)); // +1e-6 a nulla elkerüléséhez

    // DEBUG("[CW Decoder] CW: %dHz, ablak: [%d Hz - %d Hz], Peak: %s Hz,  SNR: %s dB, (Ampl: %s, Noise: %s)\n", //
    //       centerFreqHz, startFreqHz, endFreqHz,                                                               //
    //       Utils::floatToString(peakFrequencyHz_).c_str(),                                                     //
    //       Utils::floatToString(snrDb).c_str(),                                                                //
    //       Utils::floatToString(peakMagnitude_).c_str(),                                                       //
    //       Utils::floatToString(measuredNoise).c_str()                                                         //
    // );

    // SNR küszöb 20 dB és minimális amplitúdó 10000.00
    bool isToneDetected = (snrDb >= 20.0f) && (peakMagnitude_ >= 10000.00f);

    return isToneDetected;
}

/**
 * Fő jelfeldolgozó függvény: FFT adatokból morze jelek detektálása és állapotgép futtatása
 * @param fftData FFT amplitúdó tömb
 * @param fftSize FFT méret
 * @param binWidth Frekvencia bin szélesség (Hz)
 */
void CwDecoder::processCwFftData(const float *fftData, uint16_t fftSize, float binWidth) {

    // Debug: új FFT adatok érkezése
    static unsigned long debugDataCount = 0;
    static unsigned long lastDebugDataReport = millis();
    debugDataCount++;
    unsigned long now = millis();
    if (now - lastDebugDataReport > 5000) { // 5 másodpercenként
        DEBUG("[CW-DEBUG] Új FFT adatok: %lu / 5sec (%s FPS)\n", debugDataCount, Utils::floatToString(debugDataCount / 5.0f).c_str());
        debugDataCount = 0;
        lastDebugDataReport = now;
    }

    // Megkeressük a CW hang frekvenciájának megfelelő bin index
    bool toneDetected = detectTone(fftData, fftSize, binWidth);

    // LED frissítés minden FFT feldolgozáskor (nincs szükség állapot követésre CW-nél)
    if (config.data.cwRttyLedDebugEnabled) {
        digitalWrite(LED_BUILTIN, toneDetected ? HIGH : LOW);
    }
}
