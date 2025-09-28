#include "CwDecoder.h"
#include "Config.h"
#include "defines.h"
#include "utils.h"
#include <cmath>

CwDecoder::CwDecoder() {
    pinMode(LED_BUILTIN, OUTPUT); // Initialize LED_BUILTIN
    clear();
}

void CwDecoder::calibrateTimingFromWpm(uint8_t wpm) {
    float baseDotMs = 1200.0f / wpm;
    dotLengthMs_ = (uint16_t)baseDotMs;
    dashLengthMs_ = dotLengthMs_ * 3;
    elementGapMs_ = dotLengthMs_;
    letterGapMs_ = dotLengthMs_ * 3;
    wordGapMs_ = (uint16_t)(dotLengthMs_ * 7.0f);
    dotMinMs_ = (uint16_t)(dotLengthMs_ * 0.7f);
    dotMaxMs_ = (uint16_t)(dotLengthMs_ * 1.3f);
    dashMinMs_ = (uint16_t)(dashLengthMs_ * 0.7f);
    dashMaxMs_ = (uint16_t)(dashLengthMs_ * 1.3f);
    adaptiveWordGap_ = wordGapMs_;
    DEBUG("[CW-TIMING] Kalibrálva %u WPM-re: dot=%u ms, dash=%u ms, elem_gap=%u ms, betű_gap=%u ms, szó_gap=%u ms (adaptív=%u)\n", wpm, dotLengthMs_, dashLengthMs_, elementGapMs_, letterGapMs_, wordGapMs_, adaptiveWordGap_);
}

void CwDecoder::clear() {
    calibrateTimingFromWpm(15);
    currentState_ = CW_IDLE;
    toneStartTime_ = 0;
    lastToneEndTime_ = 0;
    adaptiveSnrThreshold_ = 8.0f;
    recentToneCount_ = 0;
    recentNoiseCount_ = 0;
    currentMorseBuffer_ = "";
    wordSpaceAdded_ = false;
    detectedDotsCount_ = 0;
    detectedDashesCount_ = 0;
    lastPauseLength_ = 0;
    averagePauseLength_ = 0;
    pauseCount_ = 0;
    DEBUG("[CW-CLEAR] CW dekóder inicializálva 15 WPM-re\n");
}

void CwDecoder::updateAdaptiveThreshold(bool toneDetected, float currentSnr) {
    if (toneDetected) {
        recentToneCount_++;
        if (recentToneCount_ > 20) {
            adaptiveSnrThreshold_ = max(6.0f, adaptiveSnrThreshold_ - 0.5f);
            recentToneCount_ = 0;
            recentNoiseCount_ = 0;
            DEBUG("[CW-ADAPT] SNR küszöb csökkentve: %s dB (sok jel detektálva)\n", Utils::floatToString(adaptiveSnrThreshold_).c_str());
        }
    } else {
        recentNoiseCount_++;
        if (recentNoiseCount_ > 300) {
            adaptiveSnrThreshold_ = min(18.0f, adaptiveSnrThreshold_ + 0.5f);
            recentToneCount_ = 0;
            recentNoiseCount_ = 0;
            DEBUG("[CW-ADAPT] SNR küszöb emelve: %s dB (sok zaj detektálva)\n", Utils::floatToString(adaptiveSnrThreshold_).c_str());
        }
    }
    static unsigned long lastThresholdDecay = 0;
    unsigned long now = millis();
    if (now - lastThresholdDecay > 10000 && adaptiveSnrThreshold_ > 4.0f && recentToneCount_ == 0) {
        adaptiveSnrThreshold_ = max(4.0f, adaptiveSnrThreshold_ - 1.0f);
        lastThresholdDecay = now;
        DEBUG("[CW-ADAPT] SNR küszöb időalapú csökkentés: %s dB (hosszú inaktivitás)\n", Utils::floatToString(adaptiveSnrThreshold_).c_str());
    }
}

char CwDecoder::morseToChar(const String &morseCode) {
    if (morseCode.length() > 5) {
        DEBUG("[CW-MORSE] Túl hosszú morse kód: '%s' (%d karakter)\n", morseCode.c_str(), morseCode.length());
        return '?';
    }
    if (morseCode == ".") return 'E'; if (morseCode == "-") return 'T';
    if (morseCode == "..") return 'I'; if (morseCode == ".-") return 'A';
    if (morseCode == "-.") return 'N'; if (morseCode == "--") return 'M';
    if (morseCode == "...") return 'S'; if (morseCode == "..-") return 'U';
    if (morseCode == ".-.") return 'R'; if (morseCode == ".--") return 'W';
    if (morseCode == "-..") return 'D'; if (morseCode == "-.- ") return 'K';
    if (morseCode == "--.") return 'G'; if (morseCode == "---") return 'O';
    if (morseCode == "....") return 'H'; if (morseCode == ".-..") return 'L';
    if (morseCode == ".--.") return 'P'; if (morseCode == "...-") return 'V';
    if (morseCode == "-...") return 'B'; if (morseCode == "-.-.") return 'C';
    if (morseCode == "..-.") return 'F'; if (morseCode == "-..-") return 'X';
    if (morseCode == "-.--") return 'Y'; if (morseCode == "--..") return 'Z';
    if (morseCode == "--.-") return 'Q'; if (morseCode == "---.") return 'J';
    if (morseCode == ".....") return '5'; if (morseCode == ".----") return '1';
    if (morseCode == "..---") return '2'; if (morseCode == "...--") return '3';
    if (morseCode == "....-") return '4'; if (morseCode == "--.--") return '6';
    if (morseCode == "--...") return '7'; if (morseCode == "---..") return '8';
    if (morseCode == "----.") return '9'; if (morseCode == "-----") return '0';
    DEBUG("[CW-MORSE] Ismeretlen morse kód: '%s'\n", morseCode.c_str());
    return '?';
}

void CwDecoder::updateAdaptiveWordGap(unsigned long pauseLength) {
    if (pauseLength >= letterGapMs_) {
        pauseCount_++;
        if (pauseCount_ == 1) {
            averagePauseLength_ = pauseLength;
        } else {
            averagePauseLength_ = (averagePauseLength_ * 0.7f) + (pauseLength * 0.3f);
        }
        if (averagePauseLength_ > letterGapMs_ * 1.5f) {
            adaptiveWordGap_ = (uint16_t)(averagePauseLength_ * 1.3f);
        } else {
            adaptiveWordGap_ = (uint16_t)(letterGapMs_ * 2.2f);
        }
        adaptiveWordGap_ = max((uint16_t)(letterGapMs_ * 1.8f), adaptiveWordGap_);
        adaptiveWordGap_ = min((uint16_t)(letterGapMs_ * 4.0f), adaptiveWordGap_);
        DEBUG("[CW-ADAPTIVE] Átlag szünet: %u ms, adaptív szó gap: %u ms (alapértelmezett: %u ms), szünet_szám: %u\n", (uint16_t)averagePauseLength_, adaptiveWordGap_, wordGapMs_, pauseCount_);
    }
}

void CwDecoder::discardCurrentPattern(const char *reason) {
    if (currentMorseBuffer_.length() > 0) {
        DEBUG("[CW-DISCARD] Morse minta eldobva ('%s'): '%s' - %s\n", currentMorseBuffer_.c_str(), currentMorseBuffer_.c_str(), reason);
        currentMorseBuffer_ = "";
    }
    currentState_ = CW_IDLE;
    toneStartTime_ = 0;
    DEBUG("[CW-DISCARD] Állapotgép resetelve IDLE-ra\n");
}

void CwDecoder::processCwStateMachine(bool tonePresent, String &newChars) {
    unsigned long currentTime = millis();
    switch (currentState_) {
        case CW_IDLE:
            if (tonePresent) {
                toneStartTime_ = currentTime;
                currentState_ = CW_TONE;
                wordSpaceAdded_ = false;
                DEBUG("[CW-STATE] IDLE -> TONE (jel kezdete)\n");
            } else {
                if (lastToneEndTime_ > 0) {
                    unsigned long idleDuration = currentTime - lastToneEndTime_;
                    if (idleDuration >= 2000) {
                        DEBUG("[CW-IDLE] Mondat szünet detektálva: %lu ms - új sor\n", idleDuration);
                        newChars += "\n";
                        DEBUG("[CW-DECODE] Új sor hozzáadva (puffer: '%s')\n", newChars.c_str());
                        lastToneEndTime_ = 0;
                    } else if (idleDuration >= adaptiveWordGap_ && idleDuration < 2000) {
                        if (!wordSpaceAdded_) {
                            DEBUG("[CW-IDLE] Szó szünet detektálva IDLE-ban: %lu ms >= %u ms\n", idleDuration, adaptiveWordGap_);
                            newChars += " ";
                            wordSpaceAdded_ = true;
                            DEBUG("[CW-DECODE] Szóköz hozzáadva IDLE-ban (puffer: '%s')\n", newChars.c_str());
                        }
                    }
                }
            }
            break;
        case CW_TONE:
            if (!tonePresent) {
                unsigned long toneDuration = currentTime - toneStartTime_;
                lastToneEndTime_ = currentTime;
                if (toneDuration >= dotMinMs_ && toneDuration <= dotMaxMs_) {
                    if (currentMorseBuffer_.length() >= 5) {
                        if (currentMorseBuffer_.length() > 0) {
                            char decodedChar = morseToChar(currentMorseBuffer_);
                            if (decodedChar == '?') {
                                discardCurrentPattern("túl hosszú és ismeretlen");
                            } else {
                                newChars += decodedChar;
                                DEBUG("[CW-DECODE] Betű lezárva hossz miatt (5+): '%s' -> '%c'\n", currentMorseBuffer_.c_str(), decodedChar);
                            }
                            currentMorseBuffer_ = "";
                        }
                    }
                    currentMorseBuffer_ += ".";
                    detectedDotsCount_++;
                    DEBUG("[CW-STATE] PONT detektálva (%lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                } else if (toneDuration >= dashMinMs_ && toneDuration <= dashMaxMs_) {
                    if (currentMorseBuffer_.length() >= 5) {
                        if (currentMorseBuffer_.length() > 0) {
                            char decodedChar = morseToChar(currentMorseBuffer_);
                            if (decodedChar == '?') {
                                discardCurrentPattern("túl hosszú és ismeretlen");
                            } else {
                                newChars += decodedChar;
                                DEBUG("[CW-DECODE] Betű lezárva hossz miatt (5+): '%s' -> '%c'\n", currentMorseBuffer_.c_str(), decodedChar);
                            }
                            currentMorseBuffer_ = "";
                        }
                    }
                    currentMorseBuffer_ += "-";
                    detectedDashesCount_++;
                    DEBUG("[CW-STATE] VONAL detektálva (%lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                } else if (toneDuration > dotMaxMs_ && toneDuration < dashMinMs_) {
                    if (currentMorseBuffer_.length() >= 5) {
                        if (currentMorseBuffer_.length() > 0) {
                            char decodedChar = morseToChar(currentMorseBuffer_);
                            if (decodedChar == '?') {
                                discardCurrentPattern("túl hosszú és ismeretlen");
                            } else {
                                newChars += decodedChar;
                                DEBUG("[CW-DECODE] Betű lezárva hossz miatt (5+): '%s' -> '%c'\n", currentMorseBuffer_.c_str(), decodedChar);
                            }
                            currentMorseBuffer_ = "";
                        }
                    }
                    uint16_t distanceToMaxDot = toneDuration - dotMaxMs_;
                    uint16_t distanceToMinDash = dashMinMs_ - toneDuration;
                    if (distanceToMaxDot <= distanceToMinDash) {
                        currentMorseBuffer_ += ".";
                        detectedDotsCount_++;
                        DEBUG("[CW-STATE] PONT detektálva (átfedő, %lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                    } else {
                        currentMorseBuffer_ += "-";
                        detectedDashesCount_++;
                        DEBUG("[CW-STATE] VONAL detektálva (átfedő, %lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                    }
                } else {
                    DEBUG("[CW-STATE] Érvénytelen időzítés: %lu ms (pont: %u-%u, vonal: %u-%u)\n", toneDuration, dotMinMs_, dotMaxMs_, dashMinMs_, dashMaxMs_);
                    if (toneDuration > dashMaxMs_ * 2) {
                        discardCurrentPattern("túl hosszú jel");
                    }
                }
                currentState_ = CW_PAUSE;
                DEBUG("[CW-STATE] TONE -> PAUSE (jel vége)\n");
            }
            break;
        case CW_PAUSE:
            if (tonePresent) {
                toneStartTime_ = currentTime;
                currentState_ = CW_TONE;
                DEBUG("[CW-STATE] PAUSE -> TONE (újabb jel)\n");
            } else {
                unsigned long pauseDuration = currentTime - lastToneEndTime_;
                if (pauseDuration >= letterGapMs_) {
                    DEBUG("[CW-PAUSE] Betű gap detektálva: %lu ms >= %u ms\n", pauseDuration, letterGapMs_);
                    if (currentMorseBuffer_.length() > 0) {
                        char decodedChar = morseToChar(currentMorseBuffer_);
                        if (decodedChar == '?') {
                            discardCurrentPattern("ismeretlen morse kód betű gap után");
                        } else {
                            newChars += decodedChar;
                            DEBUG("[CW-DECODE] Betű dekódolva: '%s' -> '%c' (puffer: '%s')\n", currentMorseBuffer_.c_str(), decodedChar, newChars.c_str());
                        }
                        currentMorseBuffer_ = "";
                    }
                    updateAdaptiveWordGap(pauseDuration);
                    if (pauseDuration >= adaptiveWordGap_) {
                        DEBUG("[CW-PAUSE] Adaptív szó gap detektálva: %lu ms >= %u ms (fix: %u ms)\n", pauseDuration, adaptiveWordGap_, wordGapMs_);
                        newChars += " ";
                        DEBUG("[CW-DECODE] Szóköz hozzáadva (puffer: '%s')\n", newChars.c_str());
                    }
                    if (pauseDuration >= adaptiveWordGap_ * 3) {
                        DEBUG("[CW-PAUSE] Nagyon hosszú szünet detektálva: %lu ms >= %u ms - szó vége\n", pauseDuration, adaptiveWordGap_ * 3);
                        newChars += " ";
                        DEBUG("[CW-DECODE] Szóköz hozzáadva hosszú szünet után\n");
                    }
                    currentState_ = CW_IDLE;
                    DEBUG("[CW-STATE] PAUSE -> IDLE (betű/szó vége)\n");
                }
            }
            break;
    }
}

bool CwDecoder::detectTone(const float *fftData, uint16_t fftSize, float binWidth) {
    uint16_t centerFreqHz = config.data.cwToneFrequencyHz;
    constexpr uint16_t SEARCH_WINDOW_HZ = 100;
    uint16_t startFreqHz = (centerFreqHz > SEARCH_WINDOW_HZ) ? (centerFreqHz - SEARCH_WINDOW_HZ) : 0;
    uint16_t endFreqHz = centerFreqHz + SEARCH_WINDOW_HZ;
    int startBin = static_cast<int>(startFreqHz / binWidth);
    int endBin = static_cast<int>(endFreqHz / binWidth);
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
    float peakMagnitude_ = maxMagnitude;
    float peakFrequencyHz_ = (peakBin != -1) ? (peakBin * binWidth) : 0.0f;
    float noiseSum = 0.0f;
    int noiseCount = 0;
    if (peakBin != -1) {
        int totalBins = endBin - startBin + 1;
        int excludeRange = (totalBins > 10) ? 2 : 1;
        for (int i = startBin; i <= endBin; ++i) {
            if (i < (peakBin - excludeRange) || i > (peakBin + excludeRange)) {
                noiseSum += fftData[i];
                ++noiseCount;
            }
        }
        if (noiseCount < 3) {
            noiseSum = 0.0f;
            noiseCount = 0;
            for (int i = startBin; i <= endBin; ++i) {
                if (i != peakBin) {
                    noiseSum += fftData[i];
                    ++noiseCount;
                }
            }
        }
    } else {
        for (int i = startBin; i <= endBin; ++i) {
            noiseSum += fftData[i];
            ++noiseCount;
        }
    }
    float measuredNoise = 1e-6f;
    if (noiseCount > 0 && noiseSum > 0.0f) {
        measuredNoise = noiseSum / noiseCount;
        if (measuredNoise < 1e-6f) {
            measuredNoise = 1e-6f;
        }
    }
    float snrDb = 0.0f;
    if (measuredNoise > 1e-9f && peakMagnitude_ > 0.0f) {
        snrDb = 10.0f * log10(peakMagnitude_ / measuredNoise);
    } else {
        snrDb = -60.0f;
    }
    bool isToneDetected = (snrDb >= adaptiveSnrThreshold_);
    if (isToneDetected) {
        DEBUG("[CW Decoder] CW: %dHz, ablak: [%d Hz - %d Hz],\n   Peak: %s Hz,\n   SNR: %s dB (adaptiveSnrThreshold_: %s dB),\n   PeakAmpl: %s, Noise: %s)\n", centerFreqHz, startFreqHz, endFreqHz, Utils::floatToString(peakFrequencyHz_).c_str(), Utils::floatToString(snrDb).c_str(), Utils::floatToString(adaptiveSnrThreshold_, 0).c_str(), Utils::floatToString(peakMagnitude_).c_str(), Utils::floatToString(measuredNoise).c_str());
    }
    updateAdaptiveThreshold(isToneDetected, snrDb);
    return isToneDetected;
}

String CwDecoder::processCwFftData(const float *fftData, uint16_t fftSize, float binWidth) {
    static unsigned long cwProcessCount = 0;
    static unsigned long lastCwProcessReport = millis();
    cwProcessCount++;
    unsigned long nowProcess = millis();
    if (nowProcess - lastCwProcessReport > 5000) {
        DEBUG("[CW-PROCESS] CW dekóder feldolgozás: %lu / 5sec (%s FPS), adaptív SNR: %s dB\n", cwProcessCount, Utils::floatToString(cwProcessCount / 5.0f).c_str(), Utils::floatToString(adaptiveSnrThreshold_).c_str());
        cwProcessCount = 0;
        lastCwProcessReport = nowProcess;
    }
    
    String newChars = "";
    bool currentToneDetected = detectTone(fftData, fftSize, binWidth);
    processCwStateMachine(currentToneDetected, newChars);
    
    if (config.data.cwRttyLedDebugEnabled) {
        digitalWrite(LED_BUILTIN, currentToneDetected ? HIGH : LOW);
    }
    return newChars;
}