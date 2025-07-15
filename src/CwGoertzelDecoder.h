#pragma once
#include "GoertzelDecoder.h"
#include <string>
#include <vector>

class CwGoertzelDecoder {
  public:
    CwGoertzelDecoder(float sampleRate, int blockSize, float cwFreqHz, float threshold = 1e6f);
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
    float cwFreq_;
    float threshold_;
    std::string decodedText_;
    // Morse timing, state, buffer, pont/vonás detektálás
    enum class State { IDLE, TONE, GAP };
    State state_ = State::IDLE;
    unsigned long lastEdgeMs_ = 0;
    std::vector<unsigned long> elementDurations_;
    std::vector<bool> elementIsTone_;
    std::string morseBuffer_;
    void processMorseBuffer();
};
