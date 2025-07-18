#include "CwRttyDecoder.h"
#include "defines.h" // for DEBUG

CwRttyDecoder::CwRttyDecoder() { clear(); }

void CwRttyDecoder::clear() {
    decodedText = "";
    currentState = DecoderState::IDLE;
    lastStateChangeTime = millis();
    signalPresent = false;
    peakFrequencyHz = 0.0f;
    peakMagnitude = 0.0f;
    currentMorseChar = "";
    lastMarkTime = 0;
}

String CwRttyDecoder::getDecodedText() {
    // Visszaadjuk a dekódolt szöveget, vagy a debug állapotot
    return decodedText + " " + currentMorseChar;
}

/**
 * @brief Feldolgozza az FFT adatokat és dekódolja a CW jelet.
 * Ez a metódus lesz a dekóder "szíve".
 */
void CwRttyDecoder::processFftData(const float *fftData, uint16_t fftSize, float binWidth) {
    uint32_t now = millis();
    if (!fftData || fftSize == 0 || binWidth <= 0.0f) {
        return;
    }

    // 1. Peak detektálás a CW sávban
    int startBin = static_cast<int>(MIN_CW_FREQ_HZ / binWidth);
    int endBin = static_cast<int>(MAX_CW_FREQ_HZ / binWidth);

    if (endBin >= fftSize / 2) {
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

    // 2. Állapot frissítése a jel megléte alapján
    bool signalIsOn = (peakBin != -1 && peakMagnitude > SIGNAL_THRESHOLD);
    updateState(signalIsOn, now);
}

void CwRttyDecoder::updateState(bool signalIsOn, uint32_t now) {
    DecoderState previousState = currentState;

    currentState = signalIsOn ? DecoderState::MARK : DecoderState::SPACE;

    if (currentState != previousState) {
        // Állapotváltozás történt, rögzítjük az időt
        uint32_t duration = now - lastStateChangeTime;
        lastStateChangeTime = now;

        if (previousState == DecoderState::MARK) {
            // Egy hangjel (MARK) ért véget. Megnézzük, dit vagy dah volt-e.
            if (duration > DIT_DAH_THRESHOLD_MS) {
                currentMorseChar += "-"; // Dah (vonás)
            } else {
                currentMorseChar += "."; // Dit (pont)
            }
        }
        // A SPACE állapotok kezelése a következő lépés lesz.
        // Most csak a MARK-okat gyűjtjük.
    }

    // Karakter lezárása, ha túl hosszú a szünet
    if (currentState == DecoderState::SPACE) {
        uint32_t spaceDuration = now - lastStateChangeTime;
        // Ha a szünet hosszabb, mint egy dah, akkor valószínűleg vége a karakternek.
        if (spaceDuration > DAH_LENGTH_MS && !currentMorseChar.isEmpty()) {
            decodedText += currentMorseChar; // Itt kellene majd a morse->karakter konverzió
            decodedText += " ";
            currentMorseChar = "";
        }
    }
}
