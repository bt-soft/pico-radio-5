#pragma once
#include "GoertzelDecoder.h"
#include <cstdint>
#include <string>

class RttyDecoder {
  public:
    RttyDecoder(float sampleRate, int blockSize, float markFreqHz, float spaceFreqHz, float threshold = 1e6f);
    void processBlock(const double *samples, int numSamples);
    void setAutoThreshold(bool enable) { autoThreshold_ = enable; }
    void setThresholdFactor(double factor) { thresholdFactor_ = factor; }
    std::string getDecodedText() const;
    void reset();

    float getSampleRate() const;
    int getBlockSize() const;

  private:
    // Auto-thresholding
    double noiseEstimate_ = 0.0;
    double alpha_ = 0.05;
    bool autoThreshold_ = true;
    double thresholdFactor_ = 3.0;
    GoertzelDecoder goertzel_;
    float markFreq_;
    float spaceFreq_;
    float threshold_;
    std::string decodedText_;
    // Állapotgépek, bitbuffer stb. ide jöhetnek
    // ...
};
