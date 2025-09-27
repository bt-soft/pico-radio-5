#include "ScreenAM.h"
#include "CwDecoder.h"
#include "MultiButtonDialog.h"

#include <RPi_Pico_TimerInterrupt.h>
extern RPI_PICO_Timer audioDecoderTimer;
constexpr int AUDIO_DECODER_TIMER_INTERVAL = 20; // Audio dekóder időzítő intervallum (milliszekundumban) - gyorsabb CW válaszért

bool ScreenAM::audioDecoderRun = false;
ScreenAM *ScreenAM::that = nullptr;

/**
 * @brief  Hardware timer interrupt service routine az audio dekóder számára
 */
bool audioDecoderTimerHardwareInterruptHandler(struct repeating_timer *t) {
    ScreenAM::audioDecoderRun = true;

    // Audio dekóder feldolgozása
    ScreenAM::processAudioDecoder();

    return true;
}

/**
 * @brief Javított audio dekóder feldolgozás - optimalizált és hibakezeléssel
 */
void ScreenAM::processAudioDecoder() {

    unsigned long currentTime = millis();

    // Null pointer ellenőrzések error handling-gel
    if (ScreenAM::that == nullptr) {
        DEBUG("ScreenAM::processAudioDecoder() - HIBA: ScreenAM::that nullptr\n");
        return;
    }

    if (ScreenAM::that->spectrumComp == nullptr) {
        DEBUG("ScreenAM::processAudioDecoder() - HIBA: spectrumComp nullptr\n");
        return;
    }

    if (ScreenAM::that->cwDecoder == nullptr) {
        DEBUG("ScreenAM::processAudioDecoder() - HIBA: cwDecoder nullptr\n");
        return;
    }

    // Lekérjük a jelenlegi spektrum vizualizáció módot (egyszeri ellenőrzés)
    SpectrumVisualizationComponent::DisplayMode currentMode = ScreenAM::that->spectrumComp->getCurrentMode();

    // CW és RTTY mód flag beállítása a Core1 számára (Core0 -> Core1 kommunikáció)
    static SpectrumVisualizationComponent::DisplayMode lastCwMode = static_cast<SpectrumVisualizationComponent::DisplayMode>(-1);
    bool isCwMode = (currentMode == SpectrumVisualizationComponent::DisplayMode::CWWaterfall || currentMode == SpectrumVisualizationComponent::DisplayMode::CwSnrCurve);
    bool isRttyMode = (currentMode == SpectrumVisualizationComponent::DisplayMode::RTTYWaterfall || currentMode == SpectrumVisualizationComponent::DisplayMode::RttySnrCurve);
    bool isCwOrRttyDecoderMode = isCwMode || isRttyMode; // CW vagy RTTY dekóder módok

    if (currentMode != lastCwMode) {
        // A CW mód flag-et CW VAGY RTTY módhoz állítjuk - mindkettő ugyanazt az FFT-t használja
        AudioCore1Manager::setCwModeEnabled(isCwOrRttyDecoderMode);
        DEBUG("[DECODER-DEBUG] CW/RTTY dekóder FFT flag beállítva: %s (mód: %d -> %d)\n", isCwOrRttyDecoderMode ? "ENABLE" : "DISABLE", (int)lastCwMode, (int)currentMode);
        lastCwMode = currentMode;
    }

    // Debug: spektrum mód ellenőrzése
    static SpectrumVisualizationComponent::DisplayMode lastDebugMode = static_cast<SpectrumVisualizationComponent::DisplayMode>(-1);
    if (currentMode != lastDebugMode) {
        DEBUG("[DECODER-DEBUG] Spektrum mód változás: %d -> %d (CW=%s, RTTY=%s, DecoderMode=%s)\n", (int)lastDebugMode, (int)currentMode, isCwMode ? "IGEN" : "NEM", isRttyMode ? "IGEN" : "NEM",
              isCwOrRttyDecoderMode ? "IGEN" : "NEM");
        lastDebugMode = currentMode;
    }

    // Csak CW vagy RTTY dekóder módokban dolgozunk
    if (!isCwOrRttyDecoderMode) {
        return; // Gyors kilépés ha nincs dekóder mód aktív
    }

    // FFT adatok lekérése - DUPLEX rendszer használata
    const float *magnitudeData = nullptr;
    uint16_t fftSize = 512; // Waterfall FFT méret
    float binWidth = 0.0f;
    float autoGain = 1.0f;

    // WATERFALL FFT adatok (mindig 512-es, minden módban)
    if (!AudioCore1Manager::getLatestSpectrumData(&magnitudeData, &fftSize, &binWidth, &autoGain)) {
        static unsigned long lastWarning = 0;
        unsigned long now = millis();
        if (now - lastWarning > 5000) {
            DEBUG("ScreenAM::processAudioDecoder() - Nincs új waterfall FFT adat 5 sec óta\n");
            lastWarning = now;
        }
        return;
    }

    // CW DEKÓDER FFT adatok (csak CW módban, külön CW_DECODER_FFT_SIZE-as)
    if (currentMode == SpectrumVisualizationComponent::DisplayMode::CWWaterfall) {
        const float *cwMagnitudeData = nullptr;
        float cwBinWidth = 0.0f;

        if (AudioCore1Manager::getFastCwData(&cwMagnitudeData, &cwBinWidth)) {
            // CW dekóder feldolgozás (CW_DECODER_FFT_SIZE-as FFT)
            if (ScreenAM::that && ScreenAM::that->cwDecoder) {
                ScreenAM::that->cwDecoder->processCwFftData(cwMagnitudeData, CW_DECODER_FFT_SIZE, cwBinWidth);
            }
        }
    }

    // RTTY DEKÓDER FFT adatok (RTTYWaterfall és RttySnrCurve módokban)
    if (isRttyMode) {
        const float *rttyMagnitudeData = nullptr;
        float rttyBinWidth = 0.0f;

        // RTTY-hoz is használhatjuk a CW FFT adatokat, mivel hasonló frekvencia tartomány kell
        if (AudioCore1Manager::getFastCwData(&rttyMagnitudeData, &rttyBinWidth)) {

            // RTTY dekóder feldolgozás
            if (ScreenAM::that && ScreenAM::that->rttyDecoder) {
                ScreenAM::that->rttyDecoder->processRttyFftData(rttyMagnitudeData, CW_DECODER_FFT_SIZE, rttyBinWidth);
            } else {
                // Debug: dekóder hiányzik
                static unsigned long lastDecoderErrorDebug = 0;
                if (currentTime - lastDecoderErrorDebug > 10000) { // 10 másodpercenként
                    DEBUG("[SCREEN-AM] HIBA: RTTY dekóder nem elérhető - that=%p, rttyDecoder=%p\n", ScreenAM::that, ScreenAM::that ? ScreenAM::that->rttyDecoder.get() : nullptr);
                    lastDecoderErrorDebug = currentTime;
                }
            }
        } else {
            // Debug: FFT adatok nem elérhetők
            static unsigned long lastNoFftDebug = 0;
            if (currentTime - lastNoFftDebug > 5000) { // 5 másodpercenként
                DEBUG("[SCREEN-AM] RTTY FFT adatok NEM elérhetők\n");
                lastNoFftDebug = currentTime;
            }
        }
    }

    // Validálás: FFT adatok érvényessége
    if (magnitudeData == nullptr || fftSize == 0 || binWidth <= 0.0f) {
        DEBUG("ScreenAM::processAudioDecoder() - HIBA: Érvénytelen FFT adatok (ptr:%p, size:%u, binWidth:%s)\n", magnitudeData, fftSize, Utils::floatToString(binWidth).c_str());
        return;
    }
}

// ===================================================================
// Vízszintes gombsor azonosítók - Képernyő-specifikus navigáció
// ===================================================================

/**
 * @brief AM képernyő specifikus vízszintes gomb azonosítók
 * @details Alsó vízszintes gombsor - AM specifikus funkcionalitás
 *
 * **ID tartomány**: 70-74 (nem ütközik a közös 50-52 és FM 60-61 tartománnyal)
 * **Funkció**: AM specifikus rádió funkciók
 * **Gomb típus**: Pushable (egyszeri nyomás → funkció végrehajtása)
 */
namespace ScreenAMHorizontalButtonIDs {
static constexpr uint8_t BFO_BUTTON = 70;    ///< Beat Frequency Oscillator
static constexpr uint8_t AFBW_BUTTON = 71;   ///< Audio Filter Bandwidth
static constexpr uint8_t ANTCAP_BUTTON = 72; ///< Antenna Capacitor
static constexpr uint8_t DEMOD_BUTTON = 73;  ///< Demodulation
static constexpr uint8_t STEP_BUTTON = 74;   ///< Frequency Step
} // namespace ScreenAMHorizontalButtonIDs

// =====================================================================
// Konstruktor és inicializálás
// =====================================================================

/**
 * @brief ScreenAM konstruktor implementáció - RadioScreen alaposztályból származik
 * @param tft TFT display referencia
 * @param si4735Manager Si4735 rádió chip kezelő referencia
 */
ScreenAM::ScreenAM() : ScreenRadioBase(SCREEN_NAME_AM) {

    // UI komponensek létrehozása és elhelyezése
    layoutComponents();
}

/**
 * @brief ScreenAM destruktor - MiniAudioDisplay parent pointer törlése
 * @details Biztosítja, hogy az MiniAudioDisplay ne próbáljon hozzáférni
 * a törölt screen objektumhoz képernyőváltáskor
 */
ScreenAM::~ScreenAM() { DEBUG("ScreenAM::~ScreenAM() - Destruktor hívása\n"); }

/**
 * @brief Statikus képernyő tartalom kirajzolása - AM képernyő specifikus elemek
 * @details Csak a statikus UI elemeket rajzolja ki (nem változó tartalom):
 * - S-Meter skála vonalak és számok (AM módhoz optimalizálva)
 * - Band információs terület (AM/MW/LW/SW jelzők)
 * - Statikus címkék és szövegek
 *
 * A dinamikus tartalom (pl. S-Meter érték, frekvencia) a loop()-ban frissül.
 *
 * **TODO implementációk**:
 * - S-Meter skála: RSSI alapú AM skála (0-60 dB tartomány)
 * - Band indikátor: Aktuális band típus megjelenítése
 * - Frekvencia egység: kHz/MHz megfelelő formátumban
 */
void ScreenAM::drawContent() {
    // DEBUG("ScreenAM::drawContent() - Statikus tartalom kirajzolása\n");

    //  Finomhangolás jel (aláhúzás) megjelenítése SSB/CW módokban, elrejtése egyéb módokban
    //  Itt állítjuk be, mert ha vált SW módban SSB-re, akkor is frissíteni kell a frekvencia kijelzőt a dialógus bezárásakor
    BandTable &currentband = ::pSi4735Manager->getCurrentBand();
    bool isSSBorCW = currentband.currDemod == LSB_DEMOD_TYPE || currentband.currDemod == USB_DEMOD_TYPE || currentband.currDemod == CW_DEMOD_TYPE;
    freqDisplayComp->setHideUnderline(!::pSi4735Manager->isCurrentDemodSSBorCW() && !isSSBorCW);

    // Spektrum vizualizáció komponens border frissítése
    if (spectrumComp) {
        spectrumComp->setBorderDrawn();
    }
}

/**
 * @brief Képernyő aktiválása - Event-driven gombállapot szinkronizálás
 * @details Meghívódik, amikor a felhasználó erre a képernyőre vált.
 *
 * Ez az EGYETLEN hely, ahol a gombállapotokat szinkronizáljuk a rendszer állapotával:
 * - Függőleges gombok: Mute, AGC, Attenuator állapotok
 * - Vízszintes gombok: Navigációs gombok állapotai
 *
 * **Event-driven előnyök**:
 * - NINCS folyamatos polling a loop()-ban
 * - Csak aktiváláskor történik szinkronizálás
 * - Jelentős teljesítményjavulás
 * - Univerzális gombkezelés (CommonVerticalButtons)
 *
 * **Szinkronizált állapotok**:
 * - MUTE gomb ↔ rtv::muteStat
 * - AGC gomb ↔ Si4735 AGC állapot (TODO)
 * - ATTENUATOR gomb ↔ Si4735 attenuator állapot (TODO)
 */
void ScreenAM::activate() {
    DEBUG("ScreenAM::activate() - Képernyő aktiválása\n");

    // Szülő osztály aktiválása (ScreenRadioBase -> ScreenFrequDisplayBase -> UIScreen)
    ScreenRadioBase::activate();

    lastSpectrumMode_ = SpectrumVisualizationComponent::DisplayMode::Off; // Reset on activate
    if (cwDecoder)
        cwDecoder->clear();
    if (rttyDecoder)
        rttyDecoder->clear();
    if (decodedTextBox)
        decodedTextBox->setText("");

    // ===================================================================
    // *** EGYETLEN GOMBÁLLAPOT SZINKRONIZÁLÁSI PONT - Event-driven ***
    // ===================================================================
    updateAllVerticalButtonStates();      // Univerzális funkcionális gombok (mixin method)
    updateCommonHorizontalButtonStates(); // Közös gombok szinkronizálása
    updateHorizontalButtonStates();       // AM-specifikus gombok szinkronizálása
    updateFreqDisplayWidth();             // FreqDisplay szélességének frissítése

    // --- Start audioDecoderTimer ha a képernyő aktiválódik ---
    audioDecoderTimer.detachInterrupt(); // Biztonság kedvéért leállítjuk először
    audioDecoderTimer.attachInterruptInterval(AUDIO_DECODER_TIMER_INTERVAL * 1000, audioDecoderTimerHardwareInterruptHandler);
    ScreenAM::that = this; // Beállítjuk a statikus pointert az aktuális ScreenAM példányra

    // ===================================================================
    // CW/RTTY mód azonnali beállítása screensaver után is
    // ===================================================================
    if (spectrumComp) {
        SpectrumVisualizationComponent::DisplayMode currentMode = spectrumComp->getCurrentMode();
        bool isCwMode = (currentMode == SpectrumVisualizationComponent::DisplayMode::CWWaterfall || currentMode == SpectrumVisualizationComponent::DisplayMode::CwSnrCurve);
        bool isRttyMode = (currentMode == SpectrumVisualizationComponent::DisplayMode::RTTYWaterfall || currentMode == SpectrumVisualizationComponent::DisplayMode::RttySnrCurve);
        bool isCwOrRttyDecoderMode = isCwMode || isRttyMode;

        if (isCwOrRttyDecoderMode) {
            AudioCore1Manager::setCwModeEnabled(true);
            DEBUG("[ACTIVATE-DEBUG] CW/RTTY mód azonnal beállítva az activate()-ban: mód=%d\n", (int)currentMode);
        } else {
            AudioCore1Manager::setCwModeEnabled(false);
        }
    }
}

/**
 * @brief AM képernyő deaktiválása - audioDecoderTimer leállítása
 * @details Hívja meg a képernyőváltó logika, amikor elhagyjuk az AM képernyőt!
 */
void ScreenAM::deactivate() {
    DEBUG("ScreenAM::deactivate() - Képernyő deaktiválása\n");

    // --- Stop audioDecoderTimer ha a képernyő deaktiválódik
    audioDecoderTimer.detachInterrupt();
    ScreenAM::that = nullptr; // töröljük a statikus pointert

    // CW mód flag kikapcsolása
    AudioCore1Manager::setCwModeEnabled(false);

    // Szülő osztály deaktiválása
    ScreenRadioBase::deactivate();
}

// =====================================================================
// UIScreen interface megvalósítás
// =====================================================================

/**
 * @brief Rotary encoder eseménykezelés - AM frekvencia hangolás implementáció
 * @param event Rotary encoder esemény (forgatás irány, érték, gombnyomás)
 * @return true ha sikeresen kezelte az eseményt, false egyébként
 *
 * @details AM frekvencia hangolás logika:
 * - Csak akkor reagál, ha nincs aktív dialógus
 * - Rotary klikket figyelmen kívül hagyja (más funkciókhoz)
 * - AM/MW/LW/SW frekvencia léptetés és mentés a band táblába
 * - Frekvencia kijelző azonnali frissítése
 * - Hasonló az FMScreen rotary kezeléshez, de AM-specifikus tartományokkal
 */
bool ScreenAM::handleRotary(const RotaryEvent &event) {

    // Biztonsági ellenőrzés: csak aktív dialógus nélkül és nem klikk eseménykor
    if (isDialogActive() || event.buttonState == RotaryEvent::ButtonState::Clicked) {
        // Nem kezeltük az eseményt, továbbítjuk a szülő osztálynak (dialógusokhoz)
        return UIScreen::handleRotary(event);
    }

    uint16_t newFreq;

    BandTable &currentBand = ::pSi4735Manager->getCurrentBand();
    // Az SI4735 osztály cache-ból olvassuk az aktuális frekvenciát, nem használunk chip olvasást
    uint16_t currentFrequency = ::pSi4735Manager->getSi4735().getCurrentFrequency();

    bool isCurrentDemodSSBorCW = ::pSi4735Manager->isCurrentDemodSSBorCW();

    if (isCurrentDemodSSBorCW) {

        if (rtv::bfoOn) {

            int16_t step = rtv::currentBFOStep;
            rtv::currentBFOmanu += (event.direction == RotaryEvent::Direction::Up) ? step : -step;
            rtv::currentBFOmanu = constrain(rtv::currentBFOmanu, -999, 999);

        } else {

            // Hangolás felfelé

            if (event.direction == RotaryEvent::Direction::Up) {

                rtv::freqDec = rtv::freqDec - rtv::freqstep;
                uint32_t freqTot = (uint32_t)(currentFrequency * 1000) + (rtv::freqDec * -1);

                if (freqTot > (uint32_t)(currentBand.maximumFreq * 1000)) {
                    ::pSi4735Manager->getSi4735().setFrequency(currentBand.maximumFreq);
                    rtv::freqDec = 0;
                }

                if (rtv::freqDec <= -16000) {
                    rtv::freqDec = rtv::freqDec + 16000;
                    int16_t freqPlus16 = currentFrequency + 16;
                    ::pSi4735Manager->hardwareAudioMuteOn();
                    ::pSi4735Manager->getSi4735().setFrequency(freqPlus16);
                    delay(10);
                }

            } else {

                // Hangolás lefelé

                rtv::freqDec = rtv::freqDec + rtv::freqstep;
                uint32_t freqTot = (uint32_t)(currentFrequency * 1000) - rtv::freqDec;
                if (freqTot < (uint32_t)(currentBand.minimumFreq * 1000)) {
                    ::pSi4735Manager->getSi4735().setFrequency(currentBand.minimumFreq);
                    rtv::freqDec = 0;
                }

                if (rtv::freqDec >= 16000) {
                    rtv::freqDec = rtv::freqDec - 16000;
                    int16_t freqMin16 = currentFrequency - 16;
                    ::pSi4735Manager->hardwareAudioMuteOn();
                    ::pSi4735Manager->getSi4735().setFrequency(freqMin16);
                    delay(10);
                }
            }
            rtv::currentBFO = rtv::freqDec;
            rtv::lastBFO = rtv::currentBFO;
        }

        // Lekérdezzük a beállított frekvenciát
        // Az SI4735 osztály cache-ból olvassuk az aktuális frekvenciát, nem használunk chip olvasást
        newFreq = ::pSi4735Manager->getSi4735().getCurrentFrequency();

        // SSB hangolás esetén a BFO eltolás beállítása
        const int16_t cwBaseOffset = (currentBand.currDemod == CW_DEMOD_TYPE) ? 750 : 0; // Ideiglenes konstans CW offset
        int16_t bfoToSet = cwBaseOffset + rtv::currentBFO + rtv::currentBFOmanu;
        ::pSi4735Manager->getSi4735().setSSBBfo(bfoToSet);

    } else {
        // Léptetjük a rádiót, ez el is menti a band táblába
        newFreq = ::pSi4735Manager->stepFrequency(event.value);
    }

    // AGC
    ::pSi4735Manager->checkAGC();

    // Frekvencia kijelző azonnali frissítése
    if (freqDisplayComp) {
        // SSB/CW módban mindig frissítjük a kijelzőt, mert a finomhangolás (rtv::freqDec)
        // változhat anélkül, hogy a chip frekvencia megváltozna
        freqDisplayComp->setFrequency(newFreq, isCurrentDemodSSBorCW);
    }

    // Memória státusz ellenőrzése és frissítése
    checkAndUpdateMemoryStatus();

    return true; // Esemény sikeresen kezelve
}

/**
 * @brief Folyamatos loop hívás - Event-driven optimalizált implementáció
 * @details Csak valóban szükséges frissítések - NINCS folyamatos gombállapot pollozás!
 *
 * Csak az alábbi komponenseket frissíti minden ciklusban:
 * - S-Meter (jelerősség) - valós idejű adat AM módban
 *
 * Gombállapotok frissítése CSAK:
 * - Képernyő aktiválásakor (activate() metódus)
 * - Specifikus eseményekkor (eseménykezelőkben)
 *
 * **Event-driven előnyök**:
 * - Jelentős teljesítményjavulás a korábbi polling-hoz képest
 * - CPU terhelés csökkentése
 * - Univerzális gombkezelés (CommonVerticalButtons)
 */
void ScreenAM::handleOwnLoop() {

    // ===================================================================
    // S-Meter (jelerősség) időzített frissítése - Közös RadioScreen implementáció
    // ===================================================================
    updateSMeter(false /* AM mód */);

    // Spektrum és dekóder frissítés
    if (spectrumComp && cwDecoder && rttyDecoder && decodedTextBox) {
        SpectrumVisualizationComponent::DisplayMode currentMode = spectrumComp->getCurrentMode();

        // Ha a mód megváltozott, töröljük a dekódereket
        if (currentMode != lastSpectrumMode_) {
            if (currentMode == SpectrumVisualizationComponent::DisplayMode::CWWaterfall) {
                cwDecoder->clear();
                rttyDecoder->clear();
                decodedTextBox->setText("");
            } else if (currentMode == SpectrumVisualizationComponent::DisplayMode::RTTYWaterfall || currentMode == SpectrumVisualizationComponent::DisplayMode::RttySnrCurve) {
                rttyDecoder->clear();
                cwDecoder->clear();
                decodedTextBox->setText("");
                // RTTY konfigurálása
                rttyDecoder->setMarkFrequency(config.data.rttyMarkFrequencyHz);
                rttyDecoder->setShiftFrequency(config.data.rttyShiftHz);
            } else {
                // Ha nem CW/RTTY módban vagyunk és van tartalom a szövegdobozban, töröljük
                if (decodedTextBox->getText().length() > 0) {
                    decodedTextBox->setText("");
                }
            }
            lastSpectrumMode_ = currentMode;
        }

        // Ha a CW dekóder mód aktív
        if (currentMode == SpectrumVisualizationComponent::DisplayMode::CWWaterfall) {

            // A Dekódolt szöveg lekérése és megjelenítése
            if (cwDecoder) {
                String newText = cwDecoder->getDecodedText();
                if (newText.length() > 0) {
                    // Duplikált szóközök szűrése az új szövegből
                    String filteredNewText = "";
                    for (int i = 0; i < newText.length(); i++) {
                        char currentChar = newText.charAt(i);
                        if (currentChar == ' ' && filteredNewText.length() > 0 && filteredNewText.charAt(filteredNewText.length() - 1) == ' ') {
                            // Duplikált szóköz, kihagyjuk
                            continue;
                        }
                        filteredNewText += currentChar;
                    }

                    // Hozzáfűzzük a szűrt új szöveget a meglévőhöz
                    String currentText = decodedTextBox->getText();

                    // Duplikált szóköz ellenőrzése a csatlakozási pontnál is
                    if (filteredNewText.length() > 0 && currentText.length() > 0 && currentText.charAt(currentText.length() - 1) == ' ' && filteredNewText.charAt(0) == ' ') {
                        filteredNewText = filteredNewText.substring(1); // Első szóköz eltávolítása
                    }

                    String updatedText = currentText + filteredNewText;

                    // Egyszerű karakterszám alapú scrollozás
                    const int maxChars = 80; // Kisebb limit a teszteléshez (kb. 2 sor x 40 karakter)

                    // Debug: kiírjuk a hosszt
                    if (updatedText.length() > 100) { // Már 100 karakternél is kiírjuk
                        DEBUG("[CW-UI] Szöveg hossz: %d/%d karakter\n", updatedText.length(), maxChars);
                    }

                    // Ha túl hosszú, akkor elölről vágunk le
                    if (updatedText.length() > maxChars) {
                        // Az első szó végéig keresünk egy szóközt a vágáshoz
                        int cutPos = updatedText.length() - maxChars + 20; // Egy kicsit több helyet hagyunk
                        int spacePos = updatedText.indexOf(' ', cutPos);
                        if (spacePos > 0) {
                            updatedText = updatedText.substring(spacePos + 1);
                            DEBUG("[CW-UI] Scroll: szó alapján vágva, új hossz: %d\n", updatedText.length());
                        } else {
                            // Ha nincs szóköz, akkor durván vágjuk
                            updatedText = updatedText.substring(cutPos);
                            DEBUG("[CW-UI] Scroll: durva vágás, új hossz: %d\n", updatedText.length());
                        }
                    }

                    decodedTextBox->setText(updatedText);
                }
            }
        }

        // Ha az RTTY dekóder mód aktív (RTTYWaterfall vagy RttySnrCurve)
        else if (currentMode == SpectrumVisualizationComponent::DisplayMode::RTTYWaterfall || currentMode == SpectrumVisualizationComponent::DisplayMode::RttySnrCurve) {

            // A Dekódolt szöveg lekérése és megjelenítése
            if (rttyDecoder) {
                String newText = rttyDecoder->getDecodedText();
                if (newText.length() > 0) {
                    // RTTY szöveg feldolgozása (kevesebb szűrés szükséges mint CW-nél)
                    String currentText = decodedTextBox->getText();
                    String updatedText = currentText + newText;

                    // Karakterszám alapú scrollozás
                    const int maxChars = 120; // Hosszabb limit RTTY-hoz, mert folyamatosabb szöveg

                    // Ha túl hosszú, akkor elölről vágunk le
                    if (updatedText.length() > maxChars) {
                        // Sor alapján vágás RTTY-nál (\n vagy \r karakter keresése)
                        int cutPos = updatedText.length() - maxChars + 30;
                        int newlinePos = max(updatedText.indexOf('\n', cutPos), updatedText.indexOf('\r', cutPos));
                        if (newlinePos > 0) {
                            updatedText = updatedText.substring(newlinePos + 1);
                            DEBUG("[RTTY-UI] Scroll: sor alapján vágva, új hossz: %d\n", updatedText.length());
                        } else {
                            // Szóköz alapján vágás ha nincs sortörés
                            int spacePos = updatedText.indexOf(' ', cutPos);
                            if (spacePos > 0) {
                                updatedText = updatedText.substring(spacePos + 1);
                                DEBUG("[RTTY-UI] Scroll: szó alapján vágva, új hossz: %d\n", updatedText.length());
                            } else {
                                // Durva vágás utolsó lehetőségként
                                updatedText = updatedText.substring(cutPos);
                                DEBUG("[RTTY-UI] Scroll: durva vágás, új hossz: %d\n", updatedText.length());
                            }
                        }
                    }

                    decodedTextBox->setText(updatedText);
                }
            }
        }
    }
}

/**
 * @brief Dialógus bezárásának kezelése - Gombállapot szinkronizálás
 * @details Az utolsó dialógus bezárásakor frissíti a gombállapotokat
 *
 * Ez a metódus biztosítja, hogy a gombállapotok konzisztensek maradjanak
 * a dialógusok bezárása után. Különösen fontos a ValueChangeDialog-ok
 * (Volume, Attenuator, Squelch, Frequency) után.
 */
void ScreenAM::onDialogClosed(UIDialogBase *closedDialog) {

    // Először hívjuk a RadioScreen implementációt (band váltás kezelés)
    ScreenRadioBase::onDialogClosed(closedDialog);

    // Ha ez volt az utolsó dialógus, frissítsük a gombállapotokat
    if (!isDialogActive()) {
        updateAllVerticalButtonStates();      // Függőleges gombok szinkronizálása
        updateCommonHorizontalButtonStates(); // Közös gombok szinkronizálása
        updateHorizontalButtonStates();       // AM specifikus gombok szinkronizálása
        updateFreqDisplayWidth();             // FreqDisplay szélességének frissítése

        // A gombsor konténer teljes újrarajzolása, hogy biztosan megjelenjenek a gombok
        if (horizontalButtonBar) {
            horizontalButtonBar->markForRedraw(true);
        }
    }
}

// =====================================================================
// UI komponensek layout és management
// =====================================================================

/**
 * @brief UI komponensek létrehozása és képernyőn való elhelyezése
 */
void ScreenAM::layoutComponents() {

    // Állapotsor komponens létrehozása (felső sáv)
    ScreenRadioBase::createStatusLine();

    // ===================================================================
    // Frekvencia kijelző pozicionálás (képernyő közép)
    // ===================================================================
    uint16_t FreqDisplayY = 20;
    Rect freqBounds(0, FreqDisplayY, FreqDisplay::FREQDISPLAY_WIDTH, FreqDisplay::FREQDISPLAY_HEIGHT + 10);
    ScreenFrequDisplayBase::createFreqDisplay(freqBounds);

    // Dinamikus szélesség beállítása band típus alapján
    updateFreqDisplayWidth();

    // ===================================================================
    // S-Meter komponens létrehozása - RadioScreen közös implementáció
    // ===================================================================
    Rect smeterBounds(2, FreqDisplayY + FreqDisplay::FREQDISPLAY_HEIGHT, SMeterConstants::SMETER_WIDTH, 70);
    createSMeterComponent(smeterBounds);

    // ===================================================================
    // Spektrum vizualizáció komponens létrehozása
    // ===================================================================
    // FFT méret beállítása: duplex rendszer használata (512 + CW_DECODER_FFT_SIZE decimált)
    AudioCore1Manager::setFftSize(512); // Nagy FFT a jó minőségű waterfall-hez

    Rect spectrumBounds(255, FreqDisplayY + FreqDisplay::FREQDISPLAY_HEIGHT - 10, 150, 80);
    createSpectrumComponent(spectrumBounds, RadioMode::AM);
    ScreenRadioBase::setFftSamplingFrequencyAndSpektrumMaxDisplayFrequency();

    // ===================================================================
    // Függőleges gombok létrehozása - CommonVerticalButtons mixin használata
    // ===================================================================
    createCommonVerticalButtons();   // ButtonsGroupManager használata
    createCommonHorizontalButtons(); // Alsó közös + AM specifikus vízszintes gombsor

    // CW Dekóder példányosítása
    cwDecoder = std::make_shared<CwDecoder>();

    // RTTY Dekóder példányosítása
    rttyDecoder = std::make_shared<RttyDecoder>();

    // Dekódolt szöveg doboz létrehozása
    Rect textBoxBounds(2, 165, 405, 75); // Pozíció
    decodedTextBox = std::make_shared<UITextBox>(textBoxBounds, "");
    decodedTextBox->setTextColor(TFT_CYAN, TFT_BLACK);
    decodedTextBox->setTextSize(2);
    decodedTextBox->setMaxCharsPerLine(30); // 30 karakter szélesség
    addChild(decodedTextBox);
}

/**
 * @brief AM specifikus gombok hozzáadása a közös gombokhoz
 * @param buttonConfigs A már meglévő gomb konfigurációk vektora
 * @details Felülírja az ős metódusát, hogy hozzáadja az AM specifikus gombokat
 */
void ScreenAM::addSpecificHorizontalButtons(std::vector<UIHorizontalButtonBar::ButtonConfig> &buttonConfigs) {

    // AM specifikus gombok hozzáadása a közös gombok után

    // 1. BFO - Beat Frequency Oscillator
    buttonConfigs.push_back({ScreenAMHorizontalButtonIDs::BFO_BUTTON, "BFO", UIButton::ButtonType::Toggleable, UIButton::ButtonState::Off, [this](const UIButton::ButtonEvent &event) { handleBFOButton(event); }});

    // 2. AfBW - Audio Filter Bandwidth
    buttonConfigs.push_back({ScreenAMHorizontalButtonIDs::AFBW_BUTTON, "AfBW", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off, [this](const UIButton::ButtonEvent &event) { handleAfBWButton(event); }});

    // 3. AntCap - Antenna Capacitor
    buttonConfigs.push_back({ScreenAMHorizontalButtonIDs::ANTCAP_BUTTON, "AntCap", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off, [this](const UIButton::ButtonEvent &event) { handleAntCapButton(event); }});

    // 4. Demod - Demodulation
    buttonConfigs.push_back({ScreenAMHorizontalButtonIDs::DEMOD_BUTTON, "Demod", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off, [this](const UIButton::ButtonEvent &event) { handleDemodButton(event); }});

    // 5. Step - Frequency Step
    buttonConfigs.push_back({ScreenAMHorizontalButtonIDs::STEP_BUTTON, "Step", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off, [this](const UIButton::ButtonEvent &event) { handleStepButton(event); }});
}

// =====================================================================
// EVENT-DRIVEN GOMBÁLLAPOT SZINKRONIZÁLÁS
// =====================================================================

/**
 * @brief AM specifikus vízszintes gombsor állapotainak szinkronizálása
 * @details Event-driven architektúra: CSAK aktiváláskor hívódik meg!
 *
 * Szinkronizált állapotok:
 * - AM specifikus gombok alapértelmezett állapotai
 */
void ScreenAM::updateHorizontalButtonStates() {

    if (!horizontalButtonBar) {
        return; // Biztonsági ellenőrzés
    }

    // ===================================================================
    // AM specifikus gombok állapot szinkronizálása
    // ===================================================================

    // BFO és Step gombok speciális logikája: használjuk a dedikált metódusokat
    updateBFOButtonState();
    updateStepButtonState();

    // Többi AM specifikus gomb alapértelmezett állapotban
    horizontalButtonBar->setButtonState(ScreenAMHorizontalButtonIDs::AFBW_BUTTON, UIButton::ButtonState::Off);
    horizontalButtonBar->setButtonState(ScreenAMHorizontalButtonIDs::ANTCAP_BUTTON, UIButton::ButtonState::Off);
    horizontalButtonBar->setButtonState(ScreenAMHorizontalButtonIDs::DEMOD_BUTTON, UIButton::ButtonState::Off);

    // ===================================================================
    // Step gomb speciális logika: használjuk a dedikált metódust
    // ===================================================================
    updateStepButtonState();
}

/**
 * @brief Step gomb állapotának frissítése
 * @details SSB/CW módban csak akkor engedélyezett, ha BFO be van kapcsolva
 */
void ScreenAM::updateStepButtonState() {

    if (!horizontalButtonBar) {
        return; // Biztonsági ellenőrzés
    }

    UIButton::ButtonState stepButtonState = UIButton::ButtonState::Off;

    if (::pSi4735Manager->isCurrentDemodSSBorCW()) {
        // SSB/CW módban: csak akkor engedélyezett, ha BFO be van kapcsolva
        stepButtonState = rtv::bfoOn ? UIButton::ButtonState::Off : UIButton::ButtonState::Disabled;
    } else {
        // AM/egyéb módban: mindig engedélyezett
        stepButtonState = UIButton::ButtonState::Off;
    }

    horizontalButtonBar->setButtonState(ScreenAMHorizontalButtonIDs::STEP_BUTTON, stepButtonState);
}

/**
 * @brief BFO gomb állapotának frissítése
 * @details Csak SSB/CW módban engedélyezett
 */
void ScreenAM::updateBFOButtonState() {
    if (!horizontalButtonBar) {
        return; // Biztonsági ellenőrzés
    }

    UIButton::ButtonState bfoButtonState;

    if (::pSi4735Manager->isCurrentDemodSSBorCW()) {
        // SSB/CW módban: BFO állapot szerint be/ki kapcsolva
        bfoButtonState = rtv::bfoOn ? UIButton::ButtonState::On : UIButton::ButtonState::Off;
        // DEBUG("ScreenAM::updateBFOButtonState - SSB/CW mode, BFO button: %s\n", rtv::bfoOn ? "ON" : "OFF");
    } else {
        // AM/egyéb módban: letiltva
        bfoButtonState = UIButton::ButtonState::Disabled;
        // DEBUG("ScreenAM::updateBFOButtonState - Non-SSB mode, BFO button: DISABLED\n");
    }

    horizontalButtonBar->setButtonState(ScreenAMHorizontalButtonIDs::BFO_BUTTON, bfoButtonState);
}

// =====================================================================
// AM specifikus gomb eseménykezelők
// =====================================================================

/**
 * @brief BFO gomb eseménykezelő - Beat Frequency Oscillator
 * @param event Gomb esemény (Clicked)
 * @details AM specifikus funkcionalitás - BFO állapot váltása és Step gomb frissítése
 */
void ScreenAM::handleBFOButton(const UIButton::ButtonEvent &event) {

    // Csak véltoztatásra reagálunk, nem kattintásra
    if (event.state != UIButton::EventButtonState::On && event.state != UIButton::EventButtonState::Off) {
        return;
    }

    // Csak SSB/CW módban működik
    if (!::pSi4735Manager->isCurrentDemodSSBorCW()) {
        return;
    }

    // BFO állapot váltása
    rtv::bfoOn = !rtv::bfoOn;
    rtv::bfoTr = true; // BFO animáció trigger beállítása

    // A Step gombok állapotának frissítése
    updateStepButtonState();

    // Frissítjük a frekvencia kijelzőt is, hogy BFO állapot változás volt
    freqDisplayComp->forceFullRedraw();
}

/**
 * @brief AfBW gomb eseménykezelő - Audio Frequency Bandwidth
 * @param event Gomb esemény (Clicked)
 * @details AM specifikus funkcionalitás - sávszélesség váltás
 */
void ScreenAM::handleAfBWButton(const UIButton::ButtonEvent &event) {
    if (event.state != UIButton::EventButtonState::Clicked) {
        return; // Csak kattintásra reagálunk
    }

    // Aktuális demodulációs mód
    uint8_t currDemodMod = ::pSi4735Manager->getCurrentBand().currDemod;
    // Jelenlegi sávszélesség felirata
    const char *currentBw = ::pSi4735Manager->getCurrentBandWidthLabel();

    // Megállapítjuk a lehetséges sávszélességek tömbjét
    const char *title;
    size_t labelsCount;
    const char **labels;
    uint16_t w = 250;
    uint16_t h = 170;

    if (currDemodMod == FM_DEMOD_TYPE) {
        title = "FM Filter in kHz";
        labels = ::pSi4735Manager->getBandWidthLabels(Band::bandWidthFM, labelsCount);

    } else if (currDemodMod == AM_DEMOD_TYPE) {
        title = "AM Filter in kHz";
        w = 350;
        h = 160;

        labels = ::pSi4735Manager->getBandWidthLabels(Band::bandWidthAM, labelsCount);

    } else {
        title = "SSB/CW Filter in kHz";
        w = 380;
        h = 130;

        labels = ::pSi4735Manager->getBandWidthLabels(Band::bandWidthSSB, labelsCount);
    }

    auto afBwDialog = std::make_shared<MultiButtonDialog>(
        this,                                                                                       // Képernyő referencia
        title, "",                                                                                  // Dialógus címe és üzenete
        labels, labelsCount,                                                                        // Gombok feliratai és számuk
        [this, currDemodMod](int buttonIndex, const char *buttonLabel, MultiButtonDialog *dialog) { // Gomb kattintás kezelése
            //

            if (currDemodMod == FM_DEMOD_TYPE) {
                config.data.bwIdxFM = ::pSi4735Manager->getBandWidthIndexByLabel(Band::bandWidthFM, buttonLabel);
            } else if (currDemodMod == AM_DEMOD_TYPE) {
                config.data.bwIdxAM = ::pSi4735Manager->getBandWidthIndexByLabel(Band::bandWidthAM, buttonLabel);
            } else {
                config.data.bwIdxSSB = ::pSi4735Manager->getBandWidthIndexByLabel(Band::bandWidthSSB, buttonLabel);
            }

            // Beállítjuk a rádió chip-en a kiválasztott HF sávszélességet
            ::pSi4735Manager->setAfBandWidth();

            ScreenRadioBase::setFftSamplingFrequencyAndSpektrumMaxDisplayFrequency();
        },
        true,              // Automatikusan bezárja-e a dialógust gomb kattintáskor
        currentBw,         // Az alapértelmezett (jelenlegi) gomb felirata
        true,              // Ha true, az alapértelmezett gomb le van tiltva; ha false, csak vizuálisan kiemelve
        Rect(-1, -1, w, h) // Dialógus mérete (ha -1, akkor automatikusan a képernyő közepére igazítja)
    );
    this->showDialog(afBwDialog);
}

/**
 * @brief AntCap gomb eseménykezelő - Antenna Capacitor
 * @param event Gomb esemény (Clicked)
 */
void ScreenAM::handleAntCapButton(const UIButton::ButtonEvent &event) {
    if (event.state != UIButton::EventButtonState::Clicked) {
        return;
    }
    BandTable &currband = ::pSi4735Manager->getCurrentBand(); // Kikeressük az aktuális Band rekordot

    // Az antCap értékének eltárolása egy helyi int változóban, hogy a dialógus működjön vele.
    static int antCapTempValue; // Statikus, hogy a dialógus élettartama alatt megmaradjon.
    antCapTempValue = static_cast<int>(currband.antCap);

    auto antCapDialog = std::make_shared<ValueChangeDialog>(
        this,                                                                                                                        //
        "Antenna Tuning capacitor", "Capacitor value [pF]:",                                                                         //
        &antCapTempValue,                                                                                                            //
        1, currband.currDemod == FM_DEMOD_TYPE ? Si4735Constants::SI4735_MAX_ANT_CAP_FM : Si4735Constants::SI4735_MAX_ANT_CAP_AM, 1, //
        [this](const std::variant<int, float, bool> &liveNewValue) {
            if (std::holds_alternative<int>(liveNewValue)) {
                int currentDialogVal = std::get<int>(liveNewValue);
                ::pSi4735Manager->getCurrentBand().antCap = static_cast<uint16_t>(currentDialogVal);
                ::pSi4735Manager->getSi4735().setTuneFrequencyAntennaCapacitor(currentDialogVal);
            }
        },
        nullptr, // Callback a változásra
        Rect(-1, -1, 280, 0));
    this->showDialog(antCapDialog);
}

/**
 * @brief Demod gomb eseménykezelő - Demodulation
 * @param event Gomb esemény (Clicked)
 * @details AM specifikus funkcionalitás - alapértelmezett implementáció
 */
void ScreenAM::handleDemodButton(const UIButton::ButtonEvent &event) {
    if (event.state != UIButton::EventButtonState::Clicked) {
        return;
    }

    uint8_t labelsCount;
    const char **labels = ::pSi4735Manager->getAmDemodulationModes(labelsCount);

    auto demodDialog = std::make_shared<MultiButtonDialog>(
        this,                                                                         // Képernyő referencia
        "Demodulation Mode", "",                                                      // Dialógus címe és üzenete
        labels, labelsCount,                                                          // Gombok feliratai és számuk
        [this](int buttonIndex, const char *buttonLabel, MultiButtonDialog *dialog) { // Gomb kattintás kezelése
            // Kikeressük az aktuális Band rekordot
            BandTable &currentband = ::pSi4735Manager->getCurrentBand();

            // FONTOS: Mentjük a jelenlegi frekvenciát, mielőtt megváltoztatjuk a demodulációs módot
            uint16_t currentFrequency = ::pSi4735Manager->getSi4735().getCurrentFrequency();
            currentband.currFreq = currentFrequency;

            // Demodulációs mód beállítása
            currentband.currDemod = buttonIndex + 1; // Az FM  mód indexe 0, azt kihagyjuk

            // Újra beállítjuk a sávot az új móddal (false -> ne a preferáltat töltse)
            ::pSi4735Manager->bandSet(false);

            // A demod mód változása után frissítjük a BFO és Step gombok állapotát
            // (fontos, mert SSB/CW módban mindkét gomb állapota más)
            updateBFOButtonState();
            updateStepButtonState();

        },
        true,                                           // Automatikusan bezárja-e a dialógust gomb kattintáskor
        ::pSi4735Manager->getCurrentBandDemodModDesc(), // Az alapértelmezett (jelenlegi) gomb felirata
        true,                                           // Ha true, az alapértelmezett gomb le van tiltva; ha false, csak vizuálisan kiemelve
        Rect(-1, -1, 320, 130)                          // Dialógus mérete (ha -1, akkor automatikusan a képernyő közepére igazítja)
    );

    this->showDialog(demodDialog);
}

/**
 * @brief Step gomb eseménykezelő - Frequency Step
 * @param event Gomb esemény (Clicked)
 * @details AM specifikus funkcionalitás - alapértelmezett implementáció
 */
void ScreenAM::handleStepButton(const UIButton::ButtonEvent &event) {
    if (event.state != UIButton::EventButtonState::Clicked) {
        return;
    }

    // Aktuális demodulációs mód
    uint8_t currMod = ::pSi4735Manager->getCurrentBand().currDemod;

    // Az aktuális freki lépés felirata
    const char *currentStepStr = ::pSi4735Manager->currentStepSizeStr();

    // Megállapítjuk a lehetséges lépések méretét
    const char *title;
    size_t labelsCount;
    const char **labels;
    uint16_t w = 290;
    uint16_t h = 130;

    if (rtv::bfoOn) {
        title = "Step tune BFO";
        labels = ::pSi4735Manager->getStepSizeLabels(Band::stepSizeBFO, labelsCount);

    } else if (currMod == FM_DEMOD_TYPE) {
        title = "Step tune FM";
        labels = ::pSi4735Manager->getStepSizeLabels(Band::stepSizeFM, labelsCount);
        w = 300;
        h = 100;
    } else {
        title = "Step tune AM/SSB";
        labels = ::pSi4735Manager->getStepSizeLabels(Band::stepSizeAM, labelsCount);
        w = 290;
        h = 130;
    }

    auto stepDialog = std::make_shared<MultiButtonDialog>(
        this,                                                                                  // Képernyő referencia
        title, "",                                                                             // Dialógus címe és üzenete
        labels, labelsCount,                                                                   // Gombok feliratai és számuk
        [this, currMod](int buttonIndex, const char *buttonLabel, MultiButtonDialog *dialog) { // Gomb kattintás kezelése
            // Kikeressük az aktuális Band rekordot
            BandTable &currentband = ::pSi4735Manager->getCurrentBand();

            // Kikeressük az aktuális Band típust
            uint8_t currentBandType = currentband.bandType;

            // SSB módban a BFO be van kapcsolva?
            if (rtv::bfoOn && ::pSi4735Manager->isCurrentDemodSSBorCW()) {

                // BFO step állítás - a buttonIndex közvetlenül használható
                rtv::currentBFOStep = ::pSi4735Manager->getStepSizeByIndex(Band::stepSizeBFO, buttonIndex);

            } else { // Nem SSB + BFO módban vagyunk

                // Beállítjuk a konfigban a stepSize-t - a buttonIndex közvetlenül használható
                if (currMod == FM_DEMOD_TYPE) {
                    // FM módban
                    config.data.ssIdxFM = buttonIndex;
                    currentband.currStep = ::pSi4735Manager->getStepSizeByIndex(Band::stepSizeFM, buttonIndex);

                } else {
                    // AM módban
                    if (currentBandType == MW_BAND_TYPE or currentBandType == LW_BAND_TYPE) {
                        // MW vagy LW módban
                        config.data.ssIdxMW = buttonIndex;
                    } else {
                        // Sima AM vagy SW módban
                        config.data.ssIdxAM = buttonIndex;
                    }
                }
                currentband.currStep = ::pSi4735Manager->getStepSizeByIndex(Band::stepSizeAM, buttonIndex);
            }
        },
        true,              // Automatikusan bezárja-e a dialógust gomb kattintáskor
        currentStepStr,    // Az alapértelmezett (jelenlegi) gomb felirata
        true,              // Ha true, az alapértelmezett gomb le van tiltva; ha false, csak vizuálisan kiemelve
        Rect(-1, -1, w, h) // Dialógus mérete (ha -1, akkor automatikusan a képernyő közepére igazítja)
    );

    this->showDialog(stepDialog);
}

/**
 * @brief Frissíti a FreqDisplay szélességét az aktuális band típus alapján
 * @details Dinamikusan állítja be a frekvencia kijelző szélességét
 */
void ScreenAM::updateFreqDisplayWidth() {
    if (!freqDisplayComp) {
        return; // Biztonsági ellenőrzés
    }
    auto bandType = ::pSi4735Manager->getCurrentBandType();
    uint16_t newWidth;

    switch (bandType) {
        case MW_BAND_TYPE:
        case LW_BAND_TYPE:
            newWidth = FreqDisplay::AM_BAND_WIDTH;
            break;
        case FM_BAND_TYPE:
            newWidth = FreqDisplay::FM_BAND_WIDTH;
            break;
        case SW_BAND_TYPE:
            newWidth = FreqDisplay::SW_BAND_WIDTH;
            break;
        default:
            newWidth = FreqDisplay::FREQDISPLAY_WIDTH - 25; // Alapértelmezett
            break;
    }

    freqDisplayComp->setWidth(newWidth);
}
