#include "GoertzelDecoder.h"
#include <cmath>

GoertzelDecoder::GoertzelDecoder(float sampleRate, int blockSize) : sampleRate_(sampleRate), blockSize_(blockSize) {}

float GoertzelDecoder::run(const double *samples, int numSamples, float targetFreqHz) {
    float s_prev = 0.0f, s_prev2 = 0.0f;
    float normalizedFreq = 2.0f * M_PI * targetFreqHz / sampleRate_;
    float coeff = 2.0f * cos(normalizedFreq);
    for (int i = 0; i < numSamples; ++i) {
        float s = samples[i] + coeff * s_prev - s_prev2;
        s_prev2 = s_prev;
        s_prev = s;
    }
    return s_prev2 * s_prev2 + s_prev * s_prev - coeff * s_prev * s_prev2;
}

bool GoertzelDecoder::detectCw(const double *samples, int numSamples, float cwFreqHz, float threshold) {
    float power = run(samples, numSamples, cwFreqHz);
    return power > threshold;
}

void GoertzelDecoder::detectRtty(const double *samples, int numSamples, float markFreqHz, float spaceFreqHz, float threshold, bool &markDetected, bool &spaceDetected) {
    float markPower = run(samples, numSamples, markFreqHz);
    float spacePower = run(samples, numSamples, spaceFreqHz);
    markDetected = markPower > threshold;
    spaceDetected = spacePower > threshold;
}
