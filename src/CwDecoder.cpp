#include "CwDecoder.h"
#include "Config.h"
#include "defines.h"
#include "utils.h"
#include <cmath>

constexpr float MIN_ADAPTIVE_SNR_THRESHOLD = 8.0f;  // Egységes minimum SNR küszöbérték
constexpr float MAX_ADAPTIVE_SNR_THRESHOLD = 18.0f; // Egységes maximum SNR küszöbérték

/**
 * @brief Létrehoz egy új CwDecoder objektumot és inicializálja azt.
 */
CwDecoder::CwDecoder() {
    pinMode(LED_BUILTIN, OUTPUT); // Beépített LED inicializálása hibakeresési célokra.
    clear();                      // Alapértelmezett értékek beállítása.
}

/**
 * @brief Kalibrálja a dekóder időzítését a WPM-re vonatkozó Párizs-szabvány alapján.
 * @param wpm A célzott szavak percenkénti száma. Egy standard pont hossza 1200 / WPM.
 */
void CwDecoder::calibrateTimingFromWpm(uint8_t wpm) {
    // A "PARIS" szó a CW sebesség számításának standardja. 50 "dit"-et tartalmaz.
    // WPM = (ditek percenként) / 50. Tehát, ditek percenként = WPM * 50.
    // Dit hossz (ms) = (60 * 1000) / (WPM * 50) = 1200 / WPM.
    float baseDotMs = 1200.0f / wpm;

    dotLengthMs_ = (uint16_t)baseDotMs;
    dashLengthMs_ = dotLengthMs_ * 3;             // Egy vonal 3x hosszabb, mint egy pont.
    elementGapMs_ = dotLengthMs_;                 // A betűn belüli elemek közötti szünet 1x pont hosszúságú.
    letterGapMs_ = dotLengthMs_ * 3;              // A betűk közötti szünet 3x pont hosszúságú.
    wordGapMs_ = (uint16_t)(dotLengthMs_ * 7.0f); // A szavak közötti szünet 7x pont hosszúságú.

    // Időzítési toleranciák beállítása a túl szigorú érzékelés elkerülése érdekében.
    dotMinMs_ = (uint16_t)(dotLengthMs_ * 0.7f);
    dotMaxMs_ = (uint16_t)(dotLengthMs_ * 1.3f);
    dashMinMs_ = (uint16_t)(dashLengthMs_ * 0.7f);
    dashMaxMs_ = (uint16_t)(dashLengthMs_ * 1.3f);

    adaptiveWordGap_ = wordGapMs_; // Az adaptív szóköz inicializálása az alapértelmezettel.
    DEBUG("[CW-TIMING] Kalibrálva %u WPM-re: pont=%u ms, vonal=%u ms, elem_szünet=%u ms, betű_szünet=%u ms, szó_szünet=%u ms (adaptív=%u)\n", wpm, dotLengthMs_, dashLengthMs_, elementGapMs_, letterGapMs_, wordGapMs_,
          adaptiveWordGap_);
}

/**
 * @brief Visszaállítja a dekódert egy tiszta kezdeti állapotba.
 */
void CwDecoder::clear() {
    calibrateTimingFromWpm(15); // Indításkor/törléskor alapértelmezetten 15 WPM-re állítjuk.
    currentState_ = CW_IDLE;
    toneStartTime_ = 0;
    lastToneEndTime_ = 0;
    adaptiveSnrThreshold_ = MIN_ADAPTIVE_SNR_THRESHOLD; // Kezdetben az egységes minimum értékkel indulunk.
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

/**
 * @brief Dinamikusan beállítja az SNR küszöbértéket a legutóbbi jelminőség alapján.
 * @param toneDetected Igaz, ha éppen hangot észleltünk.
 * @param currentSnr Az aktuális keret SNR értéke (jelenleg nincs használatban, de a jövőbeli logika számára elérhető).
 */
void CwDecoder::updateAdaptiveThreshold(bool toneDetected, float currentSnr) {
    if (toneDetected) {
        recentToneCount_++;
        // Ha sok hangot észlelünk egymás után, a jel valószínűleg jó. Csökkentsük a küszöböt, hogy érzékenyebbek legyünk.
        if (recentToneCount_ > 20) {
            adaptiveSnrThreshold_ = max(MIN_ADAPTIVE_SNR_THRESHOLD, adaptiveSnrThreshold_ - 0.5f); // Ne menjünk az egységes minimum alá.
            recentToneCount_ = 0;
            recentNoiseCount_ = 0;
            DEBUG("[CW-ADAPT] SNR küszöb csökkentve: %s dB (sok jel észlelve)\n", Utils::floatToString(adaptiveSnrThreshold_).c_str());
        }
    } else {
        recentNoiseCount_++;
        // Ha sok zajt látunk (nincs hang), a körülmények rosszak lehetnek. Emeljük a küszöböt a téves pozitívok elkerülése érdekében.
        if (recentNoiseCount_ > 300) {
            adaptiveSnrThreshold_ = min(MAX_ADAPTIVE_SNR_THRESHOLD, adaptiveSnrThreshold_ + 0.5f); // Ne menjünk egy maximum fölé.
            recentToneCount_ = 0;
            recentNoiseCount_ = 0;
            DEBUG("[CW-ADAPT] SNR küszöb emelve: %s dB (sok zaj észlelve)\n", Utils::floatToString(adaptiveSnrThreshold_).c_str());
        }
    }

    // Idővel lassan csökkentsük a küszöböt is, ha nincs aktivitás.
    // Ez megakadályozza, hogy a küszöb magasan maradjon egy zajos időszak után.
    static unsigned long lastThresholdDecay = 0;
    unsigned long now = millis();
    if (now - lastThresholdDecay > 10000 && adaptiveSnrThreshold_ > MIN_ADAPTIVE_SNR_THRESHOLD && recentToneCount_ == 0) {
        adaptiveSnrThreshold_ = max(MIN_ADAPTIVE_SNR_THRESHOLD, adaptiveSnrThreshold_ - 1.0f);
        lastThresholdDecay = now;
        DEBUG("[CW-ADAPT] SNR küszöb időalapú csökkentése: %s dB (hosszú inaktivitás)\n", Utils::floatToString(adaptiveSnrThreshold_).c_str());
    }
}

/**
 * @brief Átalakít egy morze kód stringet (pl. ".-") a megfelelő karakterré.
 * @param morseCode A morze kód string.
 * @return A karakter, vagy '?' egy ismeretlen kód esetén.
 */
char CwDecoder::morseToChar(const String &morseCode) {
    // Egyszerű keresőtábla. Egy map vagy egy strukturáltabb megközelítés is használható lenne.
    if (morseCode.length() > 5) {
        DEBUG("[CW-MORSE] Túl hosszú morze kód: '%s' (%d karakter)\n", morseCode.c_str(), morseCode.length());
        return '?';
    }
    if (morseCode == ".")
        return 'E';
    if (morseCode == "-")
        return 'T';
    if (morseCode == "..")
        return 'I';
    if (morseCode == ".-")
        return 'A';
    if (morseCode == "-.")
        return 'N';
    if (morseCode == "--")
        return 'M';
    if (morseCode == "...")
        return 'S';
    if (morseCode == "..-")
        return 'U';
    if (morseCode == ".-.")
        return 'R';
    if (morseCode == ".--")
        return 'W';
    if (morseCode == "-..")
        return 'D';
    if (morseCode == "-.- ")
        return 'K';
    if (morseCode == "--.")
        return 'G';
    if (morseCode == "---")
        return 'O';
    if (morseCode == "....")
        return 'H';
    if (morseCode == ".-..")
        return 'L';
    if (morseCode == ".--.")
        return 'P';
    if (morseCode == "...-")
        return 'V';
    if (morseCode == "-...")
        return 'B';
    if (morseCode == "-.-.")
        return 'C';
    if (morseCode == "..-.")
        return 'F';
    if (morseCode == "-..-")
        return 'X';
    if (morseCode == "-.--")
        return 'Y';
    if (morseCode == "--..")
        return 'Z';
    if (morseCode == "--.-")
        return 'Q';
    if (morseCode == "---.")
        return 'J';
    if (morseCode == ".....")
        return '5';
    if (morseCode == ".----")
        return '1';
    if (morseCode == "..---")
        return '2';
    if (morseCode == "...--")
        return '3';
    if (morseCode == "....-")
        return '4';
    if (morseCode == "--.--")
        return '6';
    if (morseCode == "--...")
        return '7';
    if (morseCode == "---..")
        return '8';
    if (morseCode == "----.")
        return '9';
    if (morseCode == "-----")
        return '0';
    DEBUG("[CW-MORSE] Ismeretlen morze kód: '%s'\n", morseCode.c_str());
    return '?';
}

/**
 * @brief Frissíti az adaptív szóköz hosszát a legutóbbi szünetek exponenciálisan súlyozott mozgóátlaga alapján.
 * @param pauseLength Az utoljára mért szünet időtartama.
 */
void CwDecoder::updateAdaptiveWordGap(unsigned long pauseLength) {
    // Csak azokat a szüneteket vegyük figyelembe, amelyek legalább egy betűszünet hosszúságúak.
    if (pauseLength >= letterGapMs_) {
        pauseCount_++;
        if (pauseCount_ == 1) {
            averagePauseLength_ = pauseLength;
        } else {
            // Exponenciális mozgóátlagot használunk a változások kisimítására.
            averagePauseLength_ = (averagePauseLength_ * 0.7f) + (pauseLength * 0.3f);
        }

        // Heurisztikusan határozzuk meg az új szóközt az átlagos szünetből.
        if (averagePauseLength_ > letterGapMs_ * 1.5f) {
            adaptiveWordGap_ = (uint16_t)(averagePauseLength_ * 1.3f);
        } else {
            adaptiveWordGap_ = (uint16_t)(letterGapMs_ * 2.2f);
        }

        // Az adaptív szóközt ésszerű határok közé szorítjuk a szélsőséges értékek elkerülése érdekében.
        adaptiveWordGap_ = max((uint16_t)(letterGapMs_ * 1.8f), adaptiveWordGap_);
        adaptiveWordGap_ = min((uint16_t)(letterGapMs_ * 4.0f), adaptiveWordGap_);
        // DEBUG("[CW-ADAPTIVE] Átlag szünet: %u ms, adaptív szóköz: %u ms (alapértelmezett: %u ms), szünet_szám: %u\n", (uint16_t)averagePauseLength_, adaptiveWordGap_, wordGapMs_, pauseCount_);
    }
}

/**
 * @brief Eldobja az aktuális morze mintát és visszaállítja az állapotgépet.
 * @param reason Egy hibakeresési string, amely elmagyarázza, miért lett eldobva a minta.
 */
void CwDecoder::discardCurrentPattern(const char *reason) {
    if (currentMorseBuffer_.length() > 0) {
        DEBUG("[CW-DISCARD] Morze minta eldobva ('%s'): %s\n", currentMorseBuffer_.c_str(), reason);
        currentMorseBuffer_ = "";
    }
    currentState_ = CW_IDLE;
    toneStartTime_ = 0;
    DEBUG("[CW-DISCARD] Állapotgép visszaállítva IDLE-ra\n");
}

/**
 * @brief A CW dekódolás központi állapotgép logikája.
 * @param tonePresent Igaz, ha jelenleg hangot érzékel.
 * @param newChars Referencia egy stringre, ahová a dekódolt karakterek kerülnek.
 */
void CwDecoder::processCwStateMachine(bool tonePresent, String &newChars) {
    unsigned long currentTime = millis();
    switch (currentState_) {
        case CW_IDLE:
            if (tonePresent) {
                // Új hang kezdődött.
                toneStartTime_ = currentTime;
                currentState_ = CW_TONE;
                wordSpaceAdded_ = false;
                DEBUG("[CW-STATE] IDLE -> TONE (hang kezdete)\n");
            } else {
                // Még mindig tétlen. Ellenőrizzük a nagyon hosszú szüneteket, amelyek új sort vagy szóközt jelezhetnek.
                if (lastToneEndTime_ > 0) {
                    unsigned long idleDuration = currentTime - lastToneEndTime_;
                    if (idleDuration >= 2000) { // Nagyon hosszú szünet.
                        DEBUG("[CW-IDLE] Mondatszünet észlelve: %lu ms - új sor\n", idleDuration);
                        newChars += "\n";
                        lastToneEndTime_ = 0; // Visszaállítás a többszörös új sorok elkerülése érdekében.
                    } else if (idleDuration >= adaptiveWordGap_ && !wordSpaceAdded_) {
                        DEBUG("[CW-IDLE] Szószünet észlelve IDLE-ban: %lu ms >= %u ms\n", idleDuration, adaptiveWordGap_);
                        newChars += " ";
                        wordSpaceAdded_ = true; // Többszörös szóközök megakadályozása.
                    }
                }
            }
            break;

        case CW_TONE:
            if (!tonePresent) {
                // A hang éppen véget ért. Mérjük meg az időtartamát.
                unsigned long toneDuration = currentTime - toneStartTime_;
                lastToneEndTime_ = currentTime;
                currentState_ = CW_PAUSE;
                DEBUG("[CW-STATE] TONE -> PAUSE (hang vége)\n");

                // Ellenőrizzük, hogy az aktuális morze puffer nem túl hosszú-e. Ha igen, próbáljuk meg először dekódolni.
                if (currentMorseBuffer_.length() >= 5) {
                    char decodedChar = morseToChar(currentMorseBuffer_);
                    if (decodedChar == '?') {
                        discardCurrentPattern("túl hosszú és ismeretlen");
                    } else {
                        newChars += decodedChar;
                        DEBUG("[CW-DECODE] Betű kényszerítve hossz miatt (5+): '%s' -> '%c'\n", currentMorseBuffer_.c_str(), decodedChar);
                    }
                    currentMorseBuffer_ = "";
                }

                // Osztályozzuk a hangot pontként vagy vonalként az időtartama alapján.
                if (toneDuration >= dotMinMs_ && toneDuration <= dotMaxMs_) {
                    currentMorseBuffer_ += ".";
                    detectedDotsCount_++;
                    DEBUG("[CW-STATE] PONT észlelve (%lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                } else if (toneDuration >= dashMinMs_ && toneDuration <= dashMaxMs_) {
                    currentMorseBuffer_ += "-";
                    detectedDashesCount_++;
                    DEBUG("[CW-STATE] VONAL észlelve (%lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                } else if (toneDuration > dotMaxMs_ && toneDuration < dashMinMs_) {
                    // Az időtartam kétértelmű (egy pont és egy vonal között van).
                    // Válasszuk azt, amelyikhez közelebb van.
                    uint16_t distanceToMaxDot = toneDuration - dotMaxMs_;
                    uint16_t distanceToMinDash = dashMinMs_ - toneDuration;
                    if (distanceToMaxDot <= distanceToMinDash) {
                        currentMorseBuffer_ += ".";
                        detectedDotsCount_++;
                        DEBUG("[CW-STATE] PONT észlelve (kétértelmű, %lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                    } else {
                        currentMorseBuffer_ += "-";
                        detectedDashesCount_++;
                        DEBUG("[CW-STATE] VONAL észlelve (kétértelmű, %lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                    }
                } else {
                    // A hang túl rövid vagy túl hosszú volt ahhoz, hogy érvényes elem legyen.
                    DEBUG("[CW-STATE] Érvénytelen időzítés: %lu ms (pont: %u-%u, vonal: %u-%u)\n", toneDuration, dotMinMs_, dotMaxMs_, dashMinMs_, dashMaxMs_);
                    if (toneDuration > dashMaxMs_ * 2) {
                        discardCurrentPattern("túl hosszú hang");
                    }
                }
            }
            break;

        case CW_PAUSE:
            if (tonePresent) {
                // Új hang kezdődött, véget vetve a szünetnek.
                toneStartTime_ = currentTime;
                currentState_ = CW_TONE;
                DEBUG("[CW-STATE] PAUSE -> TONE (új hang)\n");
            } else {
                // A szünet folytatódik. Ellenőrizzük az időtartamát, hogy lássuk, véget ért-e egy betű vagy szó.
                unsigned long pauseDuration = currentTime - lastToneEndTime_;

                if (pauseDuration >= letterGapMs_) {
                    // A szünet elég hosszú ahhoz, hogy betűszünet legyen.
                    DEBUG("[CW-PAUSE] Betűszünet észlelve: %lu ms >= %u ms\n", pauseDuration, letterGapMs_);
                    if (currentMorseBuffer_.length() > 0) {
                        char decodedChar = morseToChar(currentMorseBuffer_);
                        if (decodedChar == '?') {
                            discardCurrentPattern("ismeretlen morze kód betűszünet után");
                        } else {
                            newChars += decodedChar;
                            DEBUG("[CW-DECODE] Betű dekódolva: '%s' -> '%c' (puffer: '%s')\n", currentMorseBuffer_.c_str(), decodedChar, newChars.c_str());
                        }
                        currentMorseBuffer_ = ""; // Puffer törlése a következő betűhöz.
                    }

                    // Adaptív időzítés frissítése ezen szünet alapján.
                    updateAdaptiveWordGap(pauseDuration);

                    // Ellenőrizzük, hogy ez egyben szóköz is-e.
                    if (pauseDuration >= adaptiveWordGap_) {
                        DEBUG("[CW-PAUSE] Adaptív szóköz észlelve: %lu ms >= %u ms (fix: %u ms)\n", pauseDuration, adaptiveWordGap_, wordGapMs_);
                        newChars += " ";
                    }

                    // Visszatérés IDLE állapotba a következő hangra várva.
                    currentState_ = CW_IDLE;
                    DEBUG("[CW-STATE] PAUSE -> IDLE (betű/szó vége)\n");
                }
            }
            break;
    }
}

/**
 * @brief CW hangot észlel az FFT adatokban egy csúcs megkeresésével és az SNR kiszámításával.
 * @param fftData Mutató az FFT magnitúdó adatokra.
 * @param fftSize Az FFT tömb mérete.
 * @param binWidth Az FFT frekvenciafelbontása Hz-ben.
 * @return Igaz, ha hangot észlel, egyébként hamis.
 */
bool CwDecoder::detectTone(const float *fftData, uint16_t fftSize, float binWidth) {
    uint16_t centerFreqHz = config.data.cwToneFrequencyHz;
    constexpr uint16_t SEARCH_WINDOW_HZ = 50; // +/- 50 Hz-es keresés a középfrekvencia körül.

    uint16_t startFreqHz = (centerFreqHz > SEARCH_WINDOW_HZ) ? (centerFreqHz - SEARCH_WINDOW_HZ) : 0;
    uint16_t endFreqHz = centerFreqHz + SEARCH_WINDOW_HZ;

    int startBin = static_cast<int>(startFreqHz / binWidth);
    int endBin = static_cast<int>(endFreqHz / binWidth);
    if (endBin >= (int)fftSize / 2) {
        endBin = (fftSize / 2) - 1;
    }

    // A csúcs magnitúdó megkeresése a keresési ablakban.
    float maxMagnitude = 0.0f;
    int peakBin = -1;
    for (int i = startBin; i <= endBin; ++i) {
        if (fftData[i] > maxMagnitude) {
            maxMagnitude = fftData[i];
            peakBin = i;
        }
    }

    // Az átlagos zajszint kiszámítása ugyanabban az ablakban, a csúcsot magát kivéve.
    float noiseSum = 0.0f;
    int noiseCount = 0;
    if (peakBin != -1) {
        int totalBins = endBin - startBin + 1;
        int excludeRange = (totalBins > 10) ? 2 : 1; // Néhány rekeszt kizárunk a csúcs körül.
        for (int i = startBin; i <= endBin; ++i) {
            if (i < (peakBin - excludeRange) || i > (peakBin + excludeRange)) {
                noiseSum += fftData[i];
                ++noiseCount;
            }
        }
        // Tartalék megoldás, ha a kizárási tartomány túl nagy volt.
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
        // Nem található csúcs, tehát minden zaj.
        for (int i = startBin; i <= endBin; ++i) {
            noiseSum += fftData[i];
            ++noiseCount;
        }
    }

    float measuredNoise = 1e-6f; // Nullával való osztás elkerülése.
    if (noiseCount > 0 && noiseSum > 0.0f) {
        measuredNoise = noiseSum / noiseCount;
        if (measuredNoise < 1e-6f) {
            measuredNoise = 1e-6f;
        }
    }

    // SNR kiszámítása decibelben.
    float snrDb = 0.0f;
    if (measuredNoise > 1e-9f && maxMagnitude > 0.0f) {
        snrDb = 10.0f * log10(maxMagnitude / measuredNoise);
    } else {
        snrDb = -60.0f; // Nagyon alacsony SNR, ha nincs jel vagy zaj.
    }

    bool isToneDetected = (snrDb >= adaptiveSnrThreshold_);

    if (isToneDetected) {
        float peakFrequencyHz = (peakBin != -1) ? (peakBin * binWidth) : 0.0f;
        DEBUG("[CW Decoder] CW: %dHz, ablak: [%d Hz - %d Hz], Csúcs: %s Hz, SNR: %s dB (adaptiveSnrThreshold_: %s dB), CsúcsAmpl: %s, Zaj: %s)\n", centerFreqHz, startFreqHz, endFreqHz,
              Utils::floatToString(peakFrequencyHz).c_str(), Utils::floatToString(snrDb).c_str(), Utils::floatToString(adaptiveSnrThreshold_, 0).c_str(), Utils::floatToString(maxMagnitude).c_str(),
              Utils::floatToString(measuredNoise).c_str());
    }

    // Az adaptív küszöb frissítése ezen eredmény alapján.
    updateAdaptiveThreshold(isToneDetected, snrDb);

    return isToneDetected;
}

/**
 * @brief A CW dekóder fő feldolgozó függvénye.
 * @param fftData Mutató az FFT magnitúdó adatokra.
 * @param fftSize Az FFT tömb mérete.
 * @param binWidth Az FFT frekvenciafelbontása Hz-ben.
 * @return Egy string, amely az újonnan dekódolt karaktereket tartalmazza.
 */
String CwDecoder::processCwFftData(const float *fftData, uint16_t fftSize, float binWidth) {
    // --- Teljesítménykövetés ---
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
    // 1. Észleljük, hogy van-e hang az aktuális FFT keretben.
    bool currentToneDetected = detectTone(fftData, fftSize, binWidth);

    // 2. Futtassuk az állapotgépet az észlelési eredménnyel.
    processCwStateMachine(currentToneDetected, newChars);

    // 3. (Opcionális) Kapcsoljunk egy LED-et a hangérzékelés vizuális visszajelzéséhez.
    if (config.data.cwRttyLedDebugEnabled) {
        digitalWrite(LED_BUILTIN, currentToneDetected ? HIGH : LOW);
    }

    return newChars;
}