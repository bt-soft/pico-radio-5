#include "CwRttyDecoder.h"
#include <Arduino.h>
#include <algorithm>

CwRttyDecoder::CwRttyDecoder(float sampleRate, int fftSize, float binWidthHz) : sampleRate_(sampleRate), fftSize_(fftSize), binWidthHz_(binWidthHz) { cwToneHistory_.reserve(64); }

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

    // Egyszerű CW detektálás: a cwFreqHz_ körüli bin maximumát figyeljük
    int cwBin = static_cast<int>(cwFreqHz_ / binWidthHz_ + 0.5f);
    if (cwBin < 0 || cwBin >= fftSize_)
        return;

    double mag = spectrum[cwBin];

    // Küszöbérték (egyszerűsített, később adaptív)
    bool tone = mag > 1000.0; // Ezt hangolni kell
    cwToneHistory_.push_back(tone);
    if (cwToneHistory_.size() > 64)
        cwToneHistory_.erase(cwToneHistory_.begin());

    // Él detektálás (egyszerű)
    if (tone != lastTone_) {
        uint64_t dt = nowMicros - lastEdgeMicros_;
        lastEdgeMicros_ = nowMicros;
        if (tone) {
            Serial.print("CW TONE ON, dt: ");
            Serial.println(dt);
        } else {
            Serial.print("CW TONE OFF, dt: ");
            Serial.println(dt);
        }
    }

    lastTone_ = tone;
    // Itt lehetne morze dekódolás, RTTY detektálás stb.
}
