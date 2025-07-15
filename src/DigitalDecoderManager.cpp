#include "DigitalDecoderManager.h"

DigitalDecoderManager::DigitalDecoderManager(float sampleRate, int blockSize) : mode_(Mode::NONE), cwDecoder_(sampleRate, blockSize, 800.0f), rttyDecoder_(sampleRate, blockSize, 2125.0f, 2295.0f) {}

void DigitalDecoderManager::setMode(Mode mode) {
    mode_ = mode;
    reset();
}

void DigitalDecoderManager::setCwParams(float freq) { cwDecoder_ = CwGoertzelDecoder(cwDecoder_.getSampleRate(), cwDecoder_.getBlockSize(), freq); }

void DigitalDecoderManager::setRttyParams(float mark, float space) { rttyDecoder_ = RttyDecoder(rttyDecoder_.getSampleRate(), rttyDecoder_.getBlockSize(), mark, space); }

void DigitalDecoderManager::processBlock(const double *samples, int numSamples) {
    if (mode_ == Mode::CW) {
        cwDecoder_.processBlock(samples, numSamples);
    } else if (mode_ == Mode::RTTY) {
        rttyDecoder_.processBlock(samples, numSamples);
    }
}

std::string DigitalDecoderManager::getDecodedText() const {
    if (mode_ == Mode::CW) {
        return cwDecoder_.getDecodedText();
    } else if (mode_ == Mode::RTTY) {
        return rttyDecoder_.getDecodedText();
    }
    return "";
}

void DigitalDecoderManager::reset() {
    cwDecoder_.reset();
    rttyDecoder_.reset();
}
