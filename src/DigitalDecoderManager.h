#pragma once
#include "CwGoertzelDecoder.h"
#include "RttyDecoder.h"
#include <string>

class DigitalDecoderManager {
  public:
    enum class Mode { NONE, CW, RTTY };

    DigitalDecoderManager(float sampleRate, int blockSize);
    void setMode(Mode mode);
    void processBlock(const double *samples, int numSamples);
    std::string getDecodedText() const;
    void reset();

    // CW paraméterek
    void setCwParams(float freq);
    // RTTY paraméterek
    void setRttyParams(float mark, float space);

  private:
    Mode mode_;
    CwGoertzelDecoder cwDecoder_;
    RttyDecoder rttyDecoder_;
};
