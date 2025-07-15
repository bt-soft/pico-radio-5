#pragma once
#include <cstdint>
#include <vector>

class GoertzelDecoder {
  public:
    GoertzelDecoder(float sampleRate, int blockSize);
    virtual ~GoertzelDecoder() = default;

    // Általános Goertzel futtatás egy adott frekvenciára
    float run(const double *samples, int numSamples, float targetFreqHz);

    // CW detektálás (true: hang van, false: nincs)
    bool detectCw(const double *samples, int numSamples, float cwFreqHz, float threshold);

    // RTTY detektálás (mindkét frekvenciára)
    void detectRtty(const double *samples, int numSamples, float markFreqHz, float spaceFreqHz, float threshold, bool &markDetected, bool &spaceDetected);

    float getSampleRate() const { return sampleRate_; }
    int getBlockSize() const { return blockSize_; }

  private:
    float sampleRate_;
    int blockSize_;
};
