#include "RttyDecoder.h"
#include <vector>

float RttyDecoder::getSampleRate() const { return goertzel_.getSampleRate(); }
int RttyDecoder::getBlockSize() const { return goertzel_.getBlockSize(); }

RttyDecoder::RttyDecoder(float sampleRate, int blockSize, float markFreqHz, float spaceFreqHz, float threshold) : goertzel_(sampleRate, blockSize), markFreq_(markFreqHz), spaceFreq_(spaceFreqHz), threshold_(threshold) {}

// RTTY paraméterek (tipikus értékek):
constexpr float RTTY_BAUD = 45.45f; // 45.45 baud (22 ms/bit)
constexpr int RTTY_BITS = 5;        // 5 bit Baudot code
constexpr int RTTY_START_BITS = 1;
constexpr int RTTY_STOP_BITS = 1;
constexpr int RTTY_BITS_TOTAL = RTTY_START_BITS + RTTY_BITS + RTTY_STOP_BITS;

// Baudot code to ASCII (letters/figures)
static const char BAUDOT_LETTERS[32] = {' ', 'E', '\n', 'A', ' ', 'S', 'I', 'U', ' ', 'D', 'R', 'J', 'N', 'F', 'C', 'K', 'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q', 'O', 'B', 'G', ' ', 'M', 'X', 'V', ' '};
static const char BAUDOT_FIGURES[32] = {' ', '3', '\n', '-', ' ', '', '8', '7', ' ', '$', '4', '', ',', '!', ':', '(', '5', '"', ')', '2', '#', '6', '0', '1', '9', '?', '&', ' ', '.', '/', ';', ' '};

enum class RttyState { IDLE, START, DATA, STOP };

void RttyDecoder::processBlock(const double *samples, int numSamples) {
    // Egyszerűsített: minden processBlock egy bitnyi időt fed le (valódi implementációban időzítés szükséges!)
    double magMark = goertzel_.run(samples, numSamples, markFreq_);
    double magSpace = goertzel_.run(samples, numSamples, spaceFreq_);
    if (autoThreshold_) {
        double mag = std::min(magMark, magSpace);
        if (noiseEstimate_ == 0.0)
            noiseEstimate_ = mag;
        noiseEstimate_ = (1.0 - alpha_) * noiseEstimate_ + alpha_ * mag;
        threshold_ = noiseEstimate_ * thresholdFactor_;
    }
    bool mark = magMark > threshold_;
    bool space = magSpace > threshold_;
    bool bit = mark && !space ? 1 : 0; // MARK=1, SPACE=0

    // --- Állapotgép ---
    static RttyState state = RttyState::IDLE;
    static int bitCount = 0;
    static uint8_t data = 0;
    static bool shift = false; // Letters/Figures

    switch (state) {
        case RttyState::IDLE:
            if (!bit) { // SPACE = start bit
                state = RttyState::START;
                bitCount = 0;
                data = 0;
            }
            break;
        case RttyState::START:
            // Várjuk a start bit végét
            if (!bit) {
                state = RttyState::DATA;
                bitCount = 0;
                data = 0;
            } else {
                state = RttyState::IDLE; // Hibás start
            }
            break;
        case RttyState::DATA:
            data |= (bit << bitCount);
            bitCount++;
            if (bitCount >= RTTY_BITS) {
                state = RttyState::STOP;
            }
            break;
        case RttyState::STOP:
            if (bit) { // STOP bit = MARK
                // Dekódolás
                char decoded = shift ? BAUDOT_FIGURES[data & 0x1F] : BAUDOT_LETTERS[data & 0x1F];
                // Shift karakterek kezelése (pl. 0x1F = FIGS, 0x1B = LTRS)
                if ((data & 0x1F) == 0x1F)
                    shift = true;
                else if ((data & 0x1F) == 0x1B)
                    shift = false;
                else
                    decodedText_ += decoded;
            }
            state = RttyState::IDLE;
            break;
    }
}

std::string RttyDecoder::getDecodedText() const { return decodedText_; }

void RttyDecoder::reset() {
    decodedText_.clear();
    // Egyéb állapotok törlése
}
