#include "CwDecoder.h"
#include "Config.h"
#include "defines.h"
#include "utils.h"
#include <cmath>

CwDecoder::CwDecoder() {
    pinMode(LED_BUILTIN, OUTPUT); // Initialize LED_BUILTIN
    clear();
}

/**
 * Adaptív időzítési konstansok kalibrálása WPM alapján
 */
void CwDecoder::calibrateTimingFromWpm(uint8_t wpm) {
    // Alapfomula: dot_length = 1200ms / WPM
    float baseDotMs = 1200.0f / wpm;

    // Időzítési konstansok kiszámítása (ms-ben)
    dotLengthMs_ = (uint16_t)baseDotMs;
    dashLengthMs_ = dotLengthMs_ * 3;             // Vonal = 3 * pont
    elementGapMs_ = dotLengthMs_;                 // Elemek közti szünet = 1 * pont
    letterGapMs_ = dotLengthMs_ * 3;              // Betűk közti szünet = 3 * pont
    wordGapMs_ = (uint16_t)(dotLengthMs_ * 7.0f); // Szavak közti szünet = 7 * pont (adaptív alapérték)

    // Tolerancia: ±40% az időzítésekben (relaxáltabb a generátor jel dekódolásához)
    dotMinMs_ = (uint16_t)(dotLengthMs_ * 0.6f);
    dotMaxMs_ = (uint16_t)(dotLengthMs_ * 1.4f);
    dashMinMs_ = (uint16_t)(dashLengthMs_ * 0.6f);
    dashMaxMs_ = (uint16_t)(dashLengthMs_ * 1.4f);

    // Adaptív szóköz inicializálása
    adaptiveWordGap_ = wordGapMs_;

    DEBUG("[CW-TIMING] Kalibrálva %u WPM-re: dot=%u ms, dash=%u ms, elem_gap=%u ms, betű_gap=%u ms, szó_gap=%u ms (adaptív=%u)\n", wpm, dotLengthMs_, dashLengthMs_, elementGapMs_, letterGapMs_, wordGapMs_,
          adaptiveWordGap_);
}

/**
 * Minden állapot és változó alaphelyzetbe állítása
 */
void CwDecoder::clear() {
    // Alapértelmezett 15 WPM kalibrálás
    calibrateTimingFromWpm(15);

    // Állapotgép reset
    currentState_ = CW_IDLE;
    toneStartTime_ = 0;
    lastToneEndTime_ = 0;

    // Jeladaptáció
    adaptiveSnrThreshold_ = 10.0f; // Kezdő SNR küszöb
    recentToneCount_ = 0;
    recentNoiseCount_ = 0;

    // Dekódolt adatok törlése
    currentMorseBuffer_ = "";
    decodedText_ = "";
    newCharacterAdded_ = false;

    // Statisztikák
    detectedDotsCount_ = 0;
    detectedDashesCount_ = 0;

    // Adaptív szó gap inicializálás
    lastPauseLength_ = 0;
    averagePauseLength_ = 0;
    pauseCount_ = 0;

    DEBUG("[CW-CLEAR] CW dekóder inicializálva 15 WPM-re\n");
}

/**
 * Adaptív SNR küszöb frissítése
 */
void CwDecoder::updateAdaptiveThreshold(bool toneDetected, float currentSnr) {
    if (toneDetected) {
        recentToneCount_++;
        // Ha túl sok jel érkezett, csökkentjük a küszöböt
        if (recentToneCount_ > 50) {
            adaptiveSnrThreshold_ = max(6.0f, adaptiveSnrThreshold_ - 0.5f);
            recentToneCount_ = 0;
            recentNoiseCount_ = 0;
        }
    } else {
        recentNoiseCount_++;
        // Ha túl sok zaj, emeljük a küszöböt
        if (recentNoiseCount_ > 100) {
            adaptiveSnrThreshold_ = min(15.0f, adaptiveSnrThreshold_ + 1.0f);
            recentToneCount_ = 0;
            recentNoiseCount_ = 0;
        }
    }
}

/**
 * Morze kód konvertálása karakterré - kibővített tábla (max 5 elem)
 */
char CwDecoder::morseToChar(const String &morseCode) {
    // Ha túl hosszú a kód, valószínűleg hiba - korai visszatérés
    if (morseCode.length() > 5) {
        DEBUG("[CW-MORSE] Túl hosszú morse kód: '%s' (%d karakter)\n", morseCode.c_str(), morseCode.length());
        return '?';
    }

    // 1 elemesek
    if (morseCode == ".")
        return 'E';
    if (morseCode == "-")
        return 'T';

    // 2 elemesek
    if (morseCode == "..")
        return 'I';
    if (morseCode == ".-")
        return 'A';
    if (morseCode == "-.")
        return 'N';
    if (morseCode == "--")
        return 'M';

    // 3 elemesek - gyakori betűk
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
    if (morseCode == "-.-")
        return 'K';
    if (morseCode == "--.")
        return 'G';
    if (morseCode == "---")
        return 'O';

    // 4 elemesek - további betűk és fontos karakterek
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

    // 5 elemesek - számok és ritkább karakterek
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

    // Ismeretlen kód
    DEBUG("[CW-MORSE] Ismeretlen morse kód: '%s'\n", morseCode.c_str());
    return '?';
}
/**
 * dekódolt szöveg visszaadása - csak az új karakterek
 */
String CwDecoder::getDecodedText() {
    // Ha nincs új karakter, üres stringet adunk vissza
    if (newCharacterAdded_ == false) {
        return "";
    }

    // Az összes új karaktert visszaadjuk
    String result = decodedText_;

    // A szöveget töröljük, mert a UI már átmásolta
    decodedText_ = "";
    newCharacterAdded_ = false;

    DEBUG("[CW-GET] Szöveg átadva UI-nak: '%s' (hossz: %d)\n", result.c_str(), result.length());

    return result;
} /**
   * dekódolt szöveg törlése (manuális használatra)
   */
void CwDecoder::clearDecodedText() {
    decodedText_ = "";
    newCharacterAdded_ = false;
    DEBUG("[CW-CLEAR] Dekódolt szöveg törölve\n");
}

/**
 * Adaptív szóköz időzítés frissítése a megfigyelt szünetekhez
 */
void CwDecoder::updateAdaptiveWordGap(unsigned long pauseLength) {
    // Minden szünetet számolunk, ami legalább betű gap
    if (pauseLength >= letterGapMs_) {
        pauseCount_++;

        if (pauseCount_ == 1) {
            averagePauseLength_ = pauseLength;
        } else {
            // Exponenciális átlag - gyorsabban reagál a változásokra
            averagePauseLength_ = (averagePauseLength_ * 0.7f) + (pauseLength * 0.3f);
        }

        // Adaptív szó gap: Ha az átlagos szünet > betű gap, akkor valószínűleg szó gap-ek vannak
        if (averagePauseLength_ > letterGapMs_ * 1.5f) {
            // Magas átlag = vannak hosszabb szünetek, tehát csökkentjük a szó gap-et
            adaptiveWordGap_ = (uint16_t)(averagePauseLength_ * 1.3f);
        } else {
            // Alacsony átlag = csak betű gap-ek, növeljük a szó gap-et
            adaptiveWordGap_ = (uint16_t)(letterGapMs_ * 2.2f);
        }

        // Biztonsági határok - reálisabb értékek
        adaptiveWordGap_ = max((uint16_t)(letterGapMs_ * 1.8f), adaptiveWordGap_); // Min 1.8x betű gap
        adaptiveWordGap_ = min((uint16_t)(letterGapMs_ * 4.0f), adaptiveWordGap_); // Max 4x betű gap

        DEBUG("[CW-ADAPTIVE] Átlag szünet: %u ms, adaptív szó gap: %u ms (alapértelmezett: %u ms), szünet_szám: %u\n", (uint16_t)averagePauseLength_, adaptiveWordGap_, wordGapMs_, pauseCount_);
    }
}

/**
 * Érvénytelen morse minta eldobása és szöveg folytatása
 */
void CwDecoder::discardCurrentPattern(const char *reason) {
    if (currentMorseBuffer_.length() > 0) {
        DEBUG("[CW-DISCARD] Morse minta eldobva ('%s'): '%s' - %s\n", currentMorseBuffer_.c_str(), currentMorseBuffer_.c_str(), reason);
        currentMorseBuffer_ = "";
    }
}

/**
 * CW állapotgép kezelése
 */
void CwDecoder::processCwStateMachine(bool tonePresent) {
    unsigned long currentTime = millis();

    switch (currentState_) {
        case CW_IDLE:
            if (tonePresent) {
                // Jel kezdete
                toneStartTime_ = currentTime;
                currentState_ = CW_TONE;
                DEBUG("[CW-STATE] IDLE -> TONE (jel kezdete)\n");
            } else {
                // IDLE állapotban is ellenőrizzük a nagyon hosszú szüneteket (mondatok közötti)
                if (lastToneEndTime_ > 0) { // Csak ha volt már előzőleg jel
                    unsigned long idleDuration = currentTime - lastToneEndTime_;

                    // Nagyon hosszú szünet = mondat vége (2 másodpercnél hosszabb)
                    if (idleDuration >= 2000) {
                        DEBUG("[CW-IDLE] Mondat szünet detektálva: %lu ms - új sor\n", idleDuration);
                        decodedText_ += "\n";
                        newCharacterAdded_ = true;
                        DEBUG("[CW-DECODE] Új sor hozzáadva (teljes hossz: %d)\n", decodedText_.length());
                        // Reset, hogy ne ismétlődjön
                        lastToneEndTime_ = 0;
                    }
                    // Közepes hosszú szünet = szó vége (adaptív szó gap alapján)
                    else if (idleDuration >= adaptiveWordGap_ && idleDuration < 2000) {
                        DEBUG("[CW-IDLE] Szó szünet detektálva IDLE-ban: %lu ms >= %u ms\n", idleDuration, adaptiveWordGap_);
                        // Szóköz hozzáadása - nem vizsgáljuk a decodedText_ hosszát
                        decodedText_ += " ";
                        newCharacterAdded_ = true;
                        DEBUG("[CW-DECODE] Szóköz hozzáadva IDLE-ban (teljes: '%s')\n", decodedText_.c_str());
                        // Frissítsük a lastToneEndTime_-ot, hogy ne ismétlődjön
                        lastToneEndTime_ = currentTime - adaptiveWordGap_ + 100; // Kis offset
                    }
                }
            }
            break;

        case CW_TONE:
            if (!tonePresent) {
                // Jel vége - időzítés elemzése
                unsigned long toneDuration = currentTime - toneStartTime_;
                lastToneEndTime_ = currentTime;

                if (toneDuration >= dotMinMs_ && toneDuration <= dotMaxMs_) {
                    // PONT detektálva - buffer ellenőrzés (5 karakternél lezárás)
                    if (currentMorseBuffer_.length() >= 5) {
                        // Már 5 karakter van, lezárjuk azonnal
                        if (currentMorseBuffer_.length() > 0) {
                            char decodedChar = morseToChar(currentMorseBuffer_);
                            if (decodedChar == '?') {
                                discardCurrentPattern("túl hosszú és ismeretlen");
                            } else {
                                decodedText_ += decodedChar;
                                newCharacterAdded_ = true;
                                DEBUG("[CW-DECODE] Betű lezárva hossz miatt (5+): '%s' -> '%c'\n", currentMorseBuffer_.c_str(), decodedChar);
                            }
                            currentMorseBuffer_ = "";
                        }
                    }
                    currentMorseBuffer_ += ".";
                    detectedDotsCount_++;
                    DEBUG("[CW-STATE] PONT detektálva (%lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                } else if (toneDuration >= dashMinMs_ && toneDuration <= dashMaxMs_) {
                    // VONAL detektálva - buffer ellenőrzés (5 karakternél lezárás)
                    if (currentMorseBuffer_.length() >= 5) {
                        // Már 5 karakter van, lezárjuk azonnal
                        if (currentMorseBuffer_.length() > 0) {
                            char decodedChar = morseToChar(currentMorseBuffer_);
                            if (decodedChar == '?') {
                                discardCurrentPattern("túl hosszú és ismeretlen");
                            } else {
                                decodedText_ += decodedChar;
                                newCharacterAdded_ = true;
                                DEBUG("[CW-DECODE] Betű lezárva hossz miatt (5+): '%s' -> '%c'\n", currentMorseBuffer_.c_str(), decodedChar);
                            }
                            currentMorseBuffer_ = "";
                        }
                    }
                    currentMorseBuffer_ += "-";
                    detectedDashesCount_++;
                    DEBUG("[CW-STATE] VONAL detektálva (%lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                } else if (toneDuration > dotMaxMs_ && toneDuration < dashMinMs_) {
                    // ÁTFEDŐ TARTOMÁNY - intelligens döntés
                    // Buffer ellenőrzés (5 karakternél lezárás)
                    if (currentMorseBuffer_.length() >= 5) {
                        // Már 5 karakter van, lezárjuk azonnal
                        if (currentMorseBuffer_.length() > 0) {
                            char decodedChar = morseToChar(currentMorseBuffer_);
                            if (decodedChar == '?') {
                                discardCurrentPattern("túl hosszú és ismeretlen");
                            } else {
                                decodedText_ += decodedChar;
                                newCharacterAdded_ = true;
                                DEBUG("[CW-DECODE] Betű lezárva hossz miatt (5+): '%s' -> '%c'\n", currentMorseBuffer_.c_str(), decodedChar);
                            }
                            currentMorseBuffer_ = "";
                        }
                    }

                    // Ha közelebb van a pont felső határához, pont; ha a vonal alsó határához, vonal
                    uint16_t distanceToMaxDot = toneDuration - dotMaxMs_;
                    uint16_t distanceToMinDash = dashMinMs_ - toneDuration;

                    if (distanceToMaxDot <= distanceToMinDash) {
                        // Közelebb a ponthoz - pontként kezeljük
                        currentMorseBuffer_ += ".";
                        detectedDotsCount_++;
                        DEBUG("[CW-STATE] PONT detektálva (átfedő, %lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                    } else {
                        // Közelebb a vonalhoz - vonalként kezeljük
                        currentMorseBuffer_ += "-";
                        detectedDashesCount_++;
                        DEBUG("[CW-STATE] VONAL detektálva (átfedő, %lu ms): '%s'\n", toneDuration, currentMorseBuffer_.c_str());
                    }
                } else {
                    DEBUG("[CW-STATE] Érvénytelen időzítés: %lu ms (pont: %u-%u, vonal: %u-%u)\n", toneDuration, dotMinMs_, dotMaxMs_, dashMinMs_, dashMaxMs_);
                    // Érvénytelen hosszú jel esetén a buffert eldobjuk
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
                // Újabb jel kezdete
                toneStartTime_ = currentTime;
                currentState_ = CW_TONE;
                DEBUG("[CW-STATE] PAUSE -> TONE (újabb jel)\n");
            } else {
                // Szünet folytatódik - időzítés ellenőrzése
                unsigned long pauseDuration = currentTime - lastToneEndTime_;

                if (pauseDuration >= letterGapMs_) {
                    DEBUG("[CW-PAUSE] Betű gap detektálva: %lu ms >= %u ms\n", pauseDuration, letterGapMs_);

                    // Betű vége - ha van morse buffer tartalma
                    if (currentMorseBuffer_.length() > 0) {
                        char decodedChar = morseToChar(currentMorseBuffer_);
                        if (decodedChar == '?') {
                            discardCurrentPattern("ismeretlen morse kód betű gap után");
                        } else {
                            decodedText_ += decodedChar;
                            newCharacterAdded_ = true;
                            DEBUG("[CW-DECODE] Betű dekódolva: '%s' -> '%c' (teljes: '%s')\n", currentMorseBuffer_.c_str(), decodedChar, decodedText_.c_str());
                        }
                        currentMorseBuffer_ = "";
                    }

                    // Adaptív szóköz frissítése
                    updateAdaptiveWordGap(pauseDuration);

                    // Szó vége ellenőrzése - adaptív szünet használata
                    if (pauseDuration >= adaptiveWordGap_) {
                        DEBUG("[CW-PAUSE] Adaptív szó gap detektálva: %lu ms >= %u ms (fix: %u ms)\n", pauseDuration, adaptiveWordGap_, wordGapMs_);
                        // Szó vége - szóköz hozzáadása
                        decodedText_ += " ";
                        newCharacterAdded_ = true;
                        DEBUG("[CW-DECODE] Szóköz hozzáadva (teljes: '%s')\n", decodedText_.c_str());
                    }

                    // Nagyon hosszú szünet esetén (3x az adaptív érték) - mintha szó vége lenne
                    if (pauseDuration >= adaptiveWordGap_ * 3) {
                        DEBUG("[CW-PAUSE] Nagyon hosszú szünet detektálva: %lu ms >= %u ms - szó vége\n", pauseDuration, adaptiveWordGap_ * 3);
                        decodedText_ += " ";
                        newCharacterAdded_ = true;
                        DEBUG("[CW-DECODE] Szóköz hozzáadva hosszú szünet után\n");
                    }

                    currentState_ = CW_IDLE;
                    DEBUG("[CW-STATE] PAUSE -> IDLE (betű/szó vége)\n");
                }
            }
            break;
    }
}

/**
 * Detektálja a CW hangot az FFT adatok alapján
 * @param fftData FFT amplitúdó tömb
 * @param fftSize FFT méret
 * @param binWidth Frekvencia bin szélesség (Hz)
 */
bool CwDecoder::detectTone(const float *fftData, uint16_t fftSize, float binWidth) {

    // Lekérjük CW a középfrekvenciát a konfigurációból
    uint16_t centerFreqHz = config.data.cwToneFrequencyHz;

    // Még nagyobb keresési ablak a CW jelekhez
    constexpr uint16_t SEARCH_WINDOW_HZ = 200; // +-200 Hz keresési ablak
    uint16_t startFreqHz = (centerFreqHz > SEARCH_WINDOW_HZ) ? (centerFreqHz - SEARCH_WINDOW_HZ) : 0;
    uint16_t endFreqHz = centerFreqHz + SEARCH_WINDOW_HZ;

    // Keresés a megadott frekvencia ablakban
    int startBin = static_cast<int>(startFreqHz / binWidth);
    int endBin = static_cast<int>(endFreqHz / binWidth);
    if (endBin >= (int)fftSize / 2) {
        endBin = (fftSize / 2) - 1; // Ne lépjünk túl a bin határokon
    }

    // Keresés a legnagyobb amplitúdójú frekvenciára a megadott ablakban
    float maxMagnitude = 0.0f;
    int peakBin = -1;
    for (int i = startBin; i <= endBin; ++i) {
        if (fftData[i] > maxMagnitude) {
            maxMagnitude = fftData[i];
            peakBin = i;
        }
    }

    float peakMagnitude_ = maxMagnitude; // Legnagyobb amplitúdó érték

    // --- Javított Noise level számítása: robusztusabb módszer ---
    float noiseSum = 0.0f;
    int noiseCount = 0;

    // Csak akkor zárjunk ki bin-eket a csúcs körül, ha van detektált csúcs
    if (peakBin != -1) {
        // Dinamikus excludeRange: a keresési ablak méretétől függően
        int totalBins = endBin - startBin + 1;
        int excludeRange = (totalBins > 10) ? 2 : 1; // Kis ablak esetén csak 1 bin kizárása

        for (int i = startBin; i <= endBin; ++i) {
            // Matematikai abszolút érték használata
            if (i < (peakBin - excludeRange) || i > (peakBin + excludeRange)) {
                noiseSum += fftData[i];
                ++noiseCount;
            }
        }

        // Ha túl kevés bin maradt a zajszámításhoz, használjunk alternatív módszert
        if (noiseCount < 3) {
            noiseSum = 0.0f;
            noiseCount = 0;
            // Alternatív módszer: csak a legkisebb excludeRange-t használjuk
            for (int i = startBin; i <= endBin; ++i) {
                if (i != peakBin) { // Csak a peak bin-t zárjuk ki
                    noiseSum += fftData[i];
                    ++noiseCount;
                }
            }
        }
    } else {
        // Ha nincs csúcs detektálva, minden bin-t vegyünk figyelembe zajként
        for (int i = startBin; i <= endBin; ++i) {
            noiseSum += fftData[i];
            ++noiseCount;
        }
    }

    // Kiszámítjuk a mért zajszintet - garantáltan pozitív érték
    float measuredNoise = 1e-6f; // Alapértelmezett minimum zajszint
    if (noiseCount > 0 && noiseSum > 0.0f) {
        measuredNoise = noiseSum / noiseCount;
        // További biztosíték: ha túl kicsi, akkor állítsunk be minimum értéket
        if (measuredNoise < 1e-6f) {
            measuredNoise = 1e-6f;
        }
    }

    // Egyszerű SNR számítás - robosztusabb megoldás
    float snrDb = 0.0f;
    if (measuredNoise > 1e-9f && peakMagnitude_ > 0.0f) {
        snrDb = 10.0f * log10(peakMagnitude_ / measuredNoise);
    } else {
        // Ha valamelyik érték túl kicsi, akkor nagy negatív SNR-t adunk
        snrDb = -60.0f;
    }

    // DEBUG üzenet visszaengedése a diagnosztizáláshoz
    // DEBUG("[CW Decoder] CW: %dHz, ablak: [%d Hz - %d Hz], Peak: %s Hz,  SNR: %s dB, (Ampl: %s, Noise: %s)\n", centerFreqHz, startFreqHz, endFreqHz, Utils::floatToString(peakFrequencyHz_).c_str(),
    //      Utils::floatToString(snrDb).c_str(), Utils::floatToString(peakMagnitude_).c_str(), Utils::floatToString(measuredNoise).c_str());

    // JAVÍTOTT küszöbök: adaptív SNR és reális amplitúdó 70000 felett
    bool isToneDetected = (snrDb >= adaptiveSnrThreshold_) && (peakMagnitude_ >= 70000.0f);

    // Adaptív küszöb frissítése
    updateAdaptiveThreshold(isToneDetected, snrDb);

    return isToneDetected;
}

/**
 * Fő jelfeldolgozó függvény: FFT adatokból morze jelek detektálása és állapotgép futtatása
 * @param fftData FFT amplitúdó tömb
 * @param fftSize FFT méret
 * @param binWidth Frekvencia bin szélesség (Hz)
 */
void CwDecoder::processCwFftData(const float *fftData, uint16_t fftSize, float binWidth) {

    // Debug: CW dekóder adat érkezése
    static unsigned long cwProcessCount = 0;
    static unsigned long lastCwProcessReport = millis();
    cwProcessCount++;
    unsigned long nowProcess = millis();
    if (nowProcess - lastCwProcessReport > 5000) { // 5 másodpercenként
        DEBUG("[CW-PROCESS] CW dekóder feldolgozás: %lu / 5sec (%s FPS), adaptív SNR: %s dB\n", cwProcessCount, Utils::floatToString(cwProcessCount / 5.0f).c_str(), Utils::floatToString(adaptiveSnrThreshold_).c_str());
        cwProcessCount = 0;
        lastCwProcessReport = nowProcess;
    }

    // Megkeressük a CW hang frekvenciájának megfelelő bin index
    bool currentToneDetected = detectTone(fftData, fftSize, binWidth);

    // CW állapotgép futtatása
    processCwStateMachine(currentToneDetected);

    if (config.data.cwRttyLedDebugEnabled) {
        digitalWrite(LED_BUILTIN, currentToneDetected ? HIGH : LOW);
    }

    // Statisztikák periodikus kiírása
    static unsigned long lastStatsReport = millis();
    if (nowProcess - lastStatsReport > 10000) { // 10 másodpercenként
        DEBUG("[CW-STATS] Pontok: %u, vonalak: %u, dekódolt: '%s'\n", detectedDotsCount_, detectedDashesCount_, decodedText_.c_str());
        lastStatsReport = nowProcess;
    }
}
