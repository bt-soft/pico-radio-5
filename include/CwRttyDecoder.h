#pragma once
#include <cstdint>
#include <vector>

/**
 * @brief Egyszerű CW/RTTY dekóder spektrum-alapú feldolgozáshoz.
 *
 * Az AudioProcessor által előállított spektrum (magnitude) tömböt használja.
 * A dekódolt karaktereket a soros portra írja.
 */
class CwRttyDecoder {
  public:
    /**
     * @param sampleRate Mintavételezési frekvencia (Hz)
     * @param fftSize FFT méret
     * @param binWidthHz FFT bin szélesség (Hz)
     */
    CwRttyDecoder(float sampleRate, int fftSize, float binWidthHz);

    /**
     * @brief CW/RTTY dekódolás futtatása egy FFT frame-re.
     * @param spectrum Magnitúdó tömb (AudioProcessor::RvReal)
     */
    void process(const double *spectrum);

    /**
     * @brief CW dekóder fő frekvencia beállítása (Hz)
     */
    void setCwFreq(float freqHz);

    /**
     * @brief RTTY dekóder fő frekvencia beállítása (Hz)
     */
    void setRttyFreq(float markHz, float spaceHz);

  private:
    float sampleRate_;
    int fftSize_;
    float binWidthHz_;
    float cwFreqHz_ = 800.0f; // Alapértelmezett CW dekód frekvencia
    float rttyMarkHz_ = 2125.0f, rttySpaceHz_ = 2295.0f;

    // Belső állapotok, puffer stb. (egyszerűsített)
    std::vector<bool> cwToneHistory_;
    uint64_t lastEdgeMicros_ = 0;
    bool lastTone_ = false;
};
