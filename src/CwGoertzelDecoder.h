
#pragma once
#include "GoertzelDecoder.h"
#include <cstring>
#include <string>

class CwGoertzelDecoder {
  public:
    CwGoertzelDecoder(float sampleRate, int blockSize, float cwFreqHz, float threshold = 1e6f);
    void processBlock(const double *samples, int numSamples, unsigned long currentTimeMs);
    void setAutoThreshold(bool enable) { autoThreshold_ = enable; }
    void setThresholdFactor(double factor) { thresholdFactor_ = factor; }
    std::string getDecodedText() const;
    void reset();

    float getSampleRate() const;
    int getBlockSize() const;

  private:
    // Goertzel
    GoertzelDecoder goertzel_;
    float cwFreq_;
    float threshold_;
    double noiseEstimate_ = 0.0;
    double alpha_ = 0.05;
    bool autoThreshold_ = true;
    double thresholdFactor_ = 3.0;

    // Morse bináris fa dekódolás
    int treeIndex_ = 0;
    int treeOffset_ = 0;
    int treeCount_ = 0;

    // Időzítési és dekódolási változók
    unsigned long toneMinDurationMs_ = 9999L;
    unsigned long toneMaxDurationMs_ = 0L;
    unsigned long currentReferenceMs_ = 120;
    unsigned long startReferenceMs_ = 120;
    short toneIndex_ = 0;
    unsigned long rawToneDurations_[6] = {0};

    // Állapotok
    bool decoderStarted_ = false;
    bool measuringTone_ = false;
    bool toneDetectedState_ = false;
    unsigned long lastActivityMs_ = 0;
    char lastDecodedChar_ = '\0';
    bool wordSpaceProcessed_ = false;
    unsigned long lastSpaceDebugMs_ = 0;
    bool inInactiveState = false;
    unsigned long leadingEdgeTimeMs_ = 0;
    unsigned long trailingEdgeTimeMs_ = 0;

    // Körkörös karakterpuffer
    static constexpr int DECODED_CHAR_BUFFER_SIZE = 32;
    char decodedCharBuffer_[DECODED_CHAR_BUFFER_SIZE] = {0};
    int charBufferReadPos_ = 0;
    int charBufferWritePos_ = 0;
    int charBufferCount_ = 0;

    // Segédfüggvények
    void resetMorseTree();
    char getCharFromTree();
    void processDot();
    void processDash();
    void updateReferenceTimings(unsigned long duration);
    char processCollectedElements();
    void addToBuffer(char c);
};
