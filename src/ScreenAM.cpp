#include "ScreenAM.h"
#include "MultiButtonDialog.h"

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
}

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
    // TODO: S-Meter statikus skála kirajzolása AM módban
    // if (smeterComp) {
    //     smeterComp->drawAmeterScale(); // AM-specifikus skála
    // }

    // TODO: Band információs terület kirajzolása
    // drawBandInfoArea();

    // TODO: Statikus címkék és UI elemek
    // drawStaticLabels();

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

    // ===================================================================
    // *** EGYETLEN GOMBÁLLAPOT SZINKRONIZÁLÁSI PONT - Event-driven ***
    // ===================================================================
    updateAllVerticalButtonStates();      // Univerzális funkcionális gombok (mixin method)
    updateCommonHorizontalButtonStates(); // Közös gombok szinkronizálása
    updateHorizontalButtonStates();       // AM-specifikus gombok szinkronizálása
    updateFreqDisplayWidth();             // FreqDisplay szélességének frissítése
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

    // Finomhangolás jel (alulvonás) elrejtése a frekvencia kijelzőn, ha nem HAM sávban vagyunk
    freqDisplayComp->setHideUnderline(!::pSi4735Manager->isCurrentHamBand());

    // ===================================================================
    // S-Meter komponens létrehozása - RadioScreen közös implementáció
    // ===================================================================
    Rect smeterBounds(2, FreqDisplayY + FreqDisplay::FREQDISPLAY_HEIGHT, SMeterConstants::SMETER_WIDTH, 70);
    createSMeterComponent(smeterBounds);

    // ===================================================================
    // Spektrum vizualizáció komponens létrehozása
    // ===================================================================
    Rect spectrumBounds(255, FreqDisplayY + FreqDisplay::FREQDISPLAY_HEIGHT - 10, 150, 80);
    createSpectrumComponent(spectrumBounds, RadioMode::AM);

    // ===================================================================
    // Függőleges gombok létrehozása - CommonVerticalButtons mixin használata
    // ===================================================================
    createCommonVerticalButtons();   // ButtonsGroupManager használata
    createCommonHorizontalButtons(); // Alsó közös + AM specifikus vízszintes gombsor
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

            if (currDemodMod == AM_DEMOD_TYPE) {
                config.data.bwIdxAM = ::pSi4735Manager->getBandWidthIndexByLabel(Band::bandWidthAM, buttonLabel);
            } else if (currDemodMod == FM_DEMOD_TYPE) {
                config.data.bwIdxFM = ::pSi4735Manager->getBandWidthIndexByLabel(Band::bandWidthFM, buttonLabel);
            } else {
                config.data.bwIdxSSB = ::pSi4735Manager->getBandWidthIndexByLabel(Band::bandWidthSSB, buttonLabel);
            }

            // Beállítjuk a rádió chip-en a kiválasztott HF sávszélességet
            ::pSi4735Manager->setAfBandWidth();

            double bwFreqInHz = 0.0;
            if (currDemodMod == FM_DEMOD_TYPE) {
                bwFreqInHz = 15000.0; // 15KHz fix FM sávszélesség
            } else {
                double buttonValue = (double)String(buttonLabel).toFloat() * 1000.0; // kHz to Hz konverzió
                bwFreqInHz = max(1000.0, buttonValue);                               // Minimum 1KHz sávszélesség AM-ben
            }

            // A HF sávszélességnek megfelelően beállítjuk a mintavételezési frekvenciát
            AudioCore1Manager::setSamplingFrequency(bwFreqInHz * 2);

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

            // Demodulációs mód bellítása
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
