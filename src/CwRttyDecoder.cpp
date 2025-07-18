#include "CwRttyDecoder.h"
#include "defines.h" // for DEBUG
#include <map>

// Morse kód táblázat
static const std::map<String, char> morseMap = {{".-", 'A'},     {"-...", 'B'},   {"-.-.", 'C'},   {"-..", 'D'},    {".", 'E'},       {"..-.", 'F'},    {"--.", 'G'},   {"....", 'H'},   {"..", 'I'},
                                                {".---", 'J'},   {"-.-", 'K'},    {".-..", 'L'},   {"--", 'M'},     {"-.", 'N'},      {"---", 'O'},     {".--.", 'P'},  {"--.-", 'Q'},   {".-.", 'R'},
                                                {"...", 'S'},    {"-", 'T'},      {"..-", 'U'},    {"...-", 'V'},   {".--", 'W'},     {"-..-", 'X'},    {"-.--", 'Y'},  {"--..", 'Z'},   {"-----", '0'},
                                                {".----", '1'},  {"..---", '2'},  {"...--", '3'},  {"....-", '4'},  {".....", '5'},   {"-....", '6'},   {"--...", '7'}, {"---..", '8'},  {"----.", '9'},
                                                {".-.-.-", '.'}, {"--..--", ','}, {"..--..", '?'}, {"-..-.", '/'},  {"-.--.", '('},   {"-.--.-", ')'},  {".-...", '&'}, {"---...", ':'}, {"-.-.-.", ';'},
                                                {"-...-", '='},  {".-.-.", '+'},  {"-....-", '-'}, {"..--.-", '_'}, {".----.", '\''}, {"...-..-", '$'}, {".--.-.", '@'}};

CwRttyDecoder::CwRttyDecoder() { clear(); }

void CwRttyDecoder::clear() {
    decodedText = "";
    currentState = DecoderState::IDLE;
    lastStateChangeTime = millis();
    signalPresent = false;
    peakFrequencyHz = 0.0f;
    peakMagnitude = 0.0f;
    noiseLevel_ = 0.0f;
    signalThreshold_ = 1000.0f; // Magas kezdeti érték
    currentMorseChar = "";
    lastMarkTime = 0;
}

String CwRttyDecoder::getDecodedText() {
    // Visszaadjuk a dekódolt szöveget, vagy a debug állapotot
    return decodedText + currentMorseChar;
}

/**
 * @brief Feldolgozza az FFT adatokat és dekódolja a CW jelet.
 * Ez a metódus lesz a dekóder "szíve".
 */
void CwRttyDecoder::processFftData(const float *fftData, uint16_t fftSize, float binWidth) {
    uint32_t now = millis();
    if (!fftData || fftSize == 0 || binWidth <= 0.0f) {
        return;
    }

    // 1. Peak detektálás a CW sávban
    int startBin = static_cast<int>(MIN_CW_FREQ_HZ / binWidth);
    int endBin = static_cast<int>(MAX_CW_FREQ_HZ / binWidth);

    if (endBin >= (int)fftSize / 2) {
        endBin = (fftSize / 2) - 1;
    }

    float maxMagnitude = 0.0f;
    int peakBin = -1;

    for (int i = startBin; i <= endBin; ++i) {
        if (fftData[i] > maxMagnitude) {
            maxMagnitude = fftData[i];
            peakBin = i;
        }
    }

    peakMagnitude = maxMagnitude;
    peakFrequencyHz = (peakBin != -1) ? (peakBin * binWidth) : 0.0f;

    // 2. Zajszint becslése (a jel sávján kívül)
    float noiseSum = 0.0f;
    int noiseBinCount = 0;
    int noiseWindow = 50; // Hány bin-t nézzünk a sáv mellett
    int noiseGap = 10;    // Hány bin-t hagyjunk ki a sáv mellett

    // Bins before the signal
    for (int i = startBin - noiseWindow; i < startBin - noiseGap; ++i) {
        if (i >= 0) {
            noiseSum += fftData[i];
            noiseBinCount++;
        }
    }
    // Bins after the signal
    for (int i = endBin + noiseGap; i < endBin + noiseWindow; ++i) {
        if (i < (int)fftSize / 2) {
            noiseSum += fftData[i];
            noiseBinCount++;
        }
    }

    float averageNoise = (noiseBinCount > 0) ? (noiseSum / noiseBinCount) : 1.0f;

    // Simítjuk a zajszintet, hogy ne ugráljon annyira
    if (noiseLevel_ == 0.0f) {
        noiseLevel_ = averageNoise; // Első mérésnél azonnal beállítjuk
    } else {
        noiseLevel_ += NOISE_SMOOTHING_FACTOR * (averageNoise - noiseLevel_);
    }

    // 3. Dinamikus küszöb beállítása
    signalThreshold_ = noiseLevel_ * NOISE_FLOOR_FACTOR;
    // Biztonsági minimum küszöb
    if (signalThreshold_ < MINIMUM_THRESHOLD) {
        signalThreshold_ = MINIMUM_THRESHOLD;
    }

    // 4. Állapot frissítése a jel megléte alapján
    bool signalIsOn = (peakBin != -1 && peakMagnitude > signalThreshold_);
    updateState(signalIsOn, now);
}

void CwRttyDecoder::updateState(bool signalIsOn, uint32_t now) {
    DecoderState previousState = currentState;
    currentState = signalIsOn ? DecoderState::MARK : DecoderState::SPACE;

    if (currentState != previousState) {
        uint32_t duration = now - lastStateChangeTime;
        lastStateChangeTime = now;

        if (previousState == DecoderState::MARK) {
            // Egy hangjel (MARK) ért véget.
            if (duration > DIT_DAH_THRESHOLD_MS) {
                currentMorseChar += "-";
            } else {
                currentMorseChar += ".";
            }
        }
    }

    // Karakter vagy szó lezárása, ha túl hosszú a szünet
    if (currentState == DecoderState::SPACE) {
        uint32_t spaceDuration = now - lastStateChangeTime;
        if (spaceDuration > CHAR_SPACE_MS && !currentMorseChar.isEmpty()) {
            char decodedChar = morseToChar(currentMorseChar);
            if (decodedChar != '\0') {
                decodedText += decodedChar;
                // Logoljuk a teljes dekódolt szöveget, amikor egy új karaktert adunk hozzá
                DEBUG("CW Decoded: %s\n", decodedText.c_str());
            }
            currentMorseChar = "";
            if (spaceDuration > WORD_SPACE_MS) {
                decodedText += " ";
            }
        }
    }
}

char CwRttyDecoder::morseToChar(const String &morseCode) {
    auto it = morseMap.find(morseCode);
    if (it != morseMap.end()) {
        return it->second;
    }
    return '\0'; // Ismeretlen kód
}
