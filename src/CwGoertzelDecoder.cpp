#include "CwGoertzelDecoder.h"
#include <chrono>
#include <cmath>

float CwGoertzelDecoder::getSampleRate() const { return goertzel_.getSampleRate(); }
int CwGoertzelDecoder::getBlockSize() const { return goertzel_.getBlockSize(); }

// Morse kód táblázat (egyszerűsített)
static const struct {
    const char *code;
    char letter;
} MORSE_TABLE[] = {{".-", 'A'},   {"-...", 'B'}, {"-.-.", 'C'},  {"-..", 'D'},   {".", 'E'},     {"..-.", 'F'},  {"--.", 'G'},   {"....", 'H'},  {"..", 'I'},    {".---", 'J'},  {"-.-", 'K'},   {".-..", 'L'},
                   {"--", 'M'},   {"-.", 'N'},   {"---", 'O'},   {".--.", 'P'},  {"--.-", 'Q'},  {".-.", 'R'},   {"...", 'S'},   {"-", 'T'},     {"..-", 'U'},   {"...-", 'V'},  {".--", 'W'},   {"-..-", 'X'},
                   {"-.--", 'Y'}, {"--..", 'Z'}, {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"....-", '4'}, {".....", '5'}, {"-....", '6'}, {"--...", '7'}, {"---..", '8'}, {"----.", '9'}};

CwGoertzelDecoder::CwGoertzelDecoder(float sampleRate, int blockSize, float cwFreqHz, float threshold) : goertzel_(sampleRate, blockSize), cwFreq_(cwFreqHz), threshold_(threshold) {}

void CwGoertzelDecoder::processBlock(const double *samples, int numSamples) {
    // Egyszerűsített: minden processBlock egy fix időt fed le (pl. 10ms)
    static unsigned long blockDurationMs = 10; // pl. 10ms blokk
    static unsigned long currentTimeMs = 0;

    // Auto-threshold: zajszint követése
    double mag = goertzel_.run(samples, numSamples, cwFreq_);
    if (autoThreshold_) {
        if (noiseEstimate_ == 0.0)
            noiseEstimate_ = mag;
        noiseEstimate_ = (1.0 - alpha_) * noiseEstimate_ + alpha_ * mag;
        threshold_ = noiseEstimate_ * thresholdFactor_;
    }
    bool tone = mag > threshold_;

    if (state_ == State::IDLE) {
        if (tone) {
            state_ = State::TONE;
            lastEdgeMs_ = currentTimeMs;
        }
    } else if (state_ == State::TONE) {
        if (!tone) {
            unsigned long duration = currentTimeMs - lastEdgeMs_;
            elementDurations_.push_back(duration);
            elementIsTone_.push_back(true);
            lastEdgeMs_ = currentTimeMs;
            state_ = State::GAP;
        }
    } else if (state_ == State::GAP) {
        if (tone) {
            unsigned long duration = currentTimeMs - lastEdgeMs_;
            elementDurations_.push_back(duration);
            elementIsTone_.push_back(false);
            lastEdgeMs_ = currentTimeMs;
            state_ = State::TONE;
        } else if (currentTimeMs - lastEdgeMs_ > 300) { // szóköz (pl. 300ms)
            // Dekódolás
            processMorseBuffer();
            elementDurations_.clear();
            elementIsTone_.clear();
            morseBuffer_.clear();
            state_ = State::IDLE;
        }
    }
    currentTimeMs += blockDurationMs;
}

void CwGoertzelDecoder::processMorseBuffer() {
    // Egyszerűsített: pont/vonás threshold 100ms
    for (size_t i = 0; i < elementDurations_.size(); ++i) {
        if (elementIsTone_[i]) {
            if (elementDurations_[i] < 100)
                morseBuffer_ += '.';
            else
                morseBuffer_ += '-';
        }
    }
    // Kikeressük a karaktert
    for (const auto &entry : MORSE_TABLE) {
        if (morseBuffer_ == entry.code) {
            decodedText_ += entry.letter;
            return;
        }
    }
    // Ha nem talált, szóköz
    decodedText_ += ' ';
}

std::string CwGoertzelDecoder::getDecodedText() const { return decodedText_; }

void CwGoertzelDecoder::reset() {
    decodedText_.clear();
    elementDurations_.clear();
    elementIsTone_.clear();
    morseBuffer_.clear();
    state_ = State::IDLE;
    lastEdgeMs_ = 0;
}
