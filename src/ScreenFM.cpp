#include "ScreenFM.h"

// ===================================================================
// FM képernyő specifikus vízszintes gomb azonosítók
// ===================================================================
namespace ScreenFMHorizontalButtonIDs {
static constexpr uint8_t SEEK_DOWN_BUTTON = 60; ///< Seek lefelé (pushable) - FM specifikus
static constexpr uint8_t SEEK_UP_BUTTON = 61;   ///< Seek felfelé (pushable) - FM specifikus
} // namespace ScreenFMHorizontalButtonIDs

// ===================================================================
// Konstruktor és inicializálás
// ===================================================================

/**
 * @brief ScreenFM konstruktor - FM rádió képernyő létrehozása
 *
 * @details Inicializálja az FM rádió képernyőt:
 * - Si4735 chip inicializálása
 * - UI komponensek elrendezése (gombsorok, kijelzők)
 * - Event-driven gombkezelés beállítása
 */
ScreenFM::ScreenFM() : ScreenRadioBase(SCREEN_NAME_FM) {

    // UI komponensek elhelyezése
    layoutComponents();
}

/**
 * @brief ScreenFM destruktor - Automatikus cleanup
 */
ScreenFM::~ScreenFM() { DEBUG("ScreenFM::~ScreenFM() - Destruktor hívása\n"); }

// ===================================================================
// UI komponensek layout és elhelyezés
// ===================================================================

/**
 * @brief UI komponensek létrehozása és képernyőn való elhelyezése
 * @details Létrehozza és elhelyezi az összes UI elemet:
 * - Állapotsor (felül)
 * - Frekvencia kijelző (középen)
 * - S-Meter (jelerősség mérő)
 * - Függőleges gombsor (jobb oldal)
 * - Vízszintes gombsor (alul)
 */
void ScreenFM::layoutComponents() {

    // Állapotsor komponens létrehozása (felső sáv)
    ScreenRadioBase::createStatusLine();

    // ===================================================================
    // Frekvencia kijelző pozicionálás (képernyő közép)
    // ===================================================================
    uint16_t FreqDisplayY = 20;
    Rect freqBounds(0, FreqDisplayY, FreqDisplay::FREQDISPLAY_WIDTH - 60, FreqDisplay::FREQDISPLAY_HEIGHT - 20);
    ScreenFrequDisplayBase::createFreqDisplay(freqBounds);
    freqDisplayComp->setHideUnderline(true); // Alulvonás elrejtése a frekvencia kijelzőn

    // ===================================================================
    // STEREO/MONO jelző létrehozása
    // ===================================================================
    uint16_t stereoY = FreqDisplayY;
    Rect stereoBounds(FreqDisplay::FREQDISPLAY_WIDTH - 130, stereoY, 50, 20);
    stereoIndicator = std::make_shared<StereoIndicator>(stereoBounds);
    addChild(stereoIndicator);

    // ===================================================================
    // RDS komponens létrehozása és pozicionálása
    // ===================================================================
    rdsComponent = std::make_shared<RDSComponent>(Rect(0, 0, 0, 0));
    addChild(rdsComponent);

    // RDS Állomásnév közvetlenül a frekvencia kijelző alatt
    uint16_t currentY = FreqDisplayY + FreqDisplay::FREQDISPLAY_HEIGHT - 15;
    rdsComponent->setStationNameArea(Rect(2, currentY, 180, 32));

    // RDS Program típus közvetlenül az állomásnév alatt
    currentY += 32 + 5; // 18px magasság + 5px kisebb hézag
    rdsComponent->setProgramTypeArea(Rect(2, currentY, 135, 18));

    // Dátum/idő
    rdsComponent->setDateTimeArea(Rect(2 + 130 + 5, currentY, 105, 18)); // Ugyanaz az Y pozíció, mint a program típus

    // RDS Radio text
    currentY += 18 + 5;
    rdsComponent->setRadioTextArea(Rect(2, currentY, SMeterConstants::SMETER_WIDTH, 24));

    // ===================================================================
    // S-Meter komponens létrehozása - RadioScreen közös implementáció
    // ===================================================================
    currentY += 24 + 5;
    Rect smeterBounds(2, currentY, SMeterConstants::SMETER_WIDTH, 60);
    createSMeterComponent(smeterBounds);

    // ===================================================================
    // Spektrum vizualizáció komponens létrehozása
    // ===================================================================
    Rect spectrumBounds(280, 80, 120, 80);
    createSpectrumComponent(spectrumBounds, RadioMode::FM);

    // ===================================================================
    // Gombsorok létrehozása - Event-driven architektúra
    // ===================================================================
    createCommonVerticalButtons();   // ButtonsGroupManager alapú függőleges gombsor egyedi Memo kezelővel
    createCommonHorizontalButtons(); // Alsó közös + FM specifikus vízszintes gombsor
}

// ===================================================================
// Felhasználói események kezelése - Event-driven architektúra
// ===================================================================

/**
 * @brief Rotary encoder eseménykezelés - Frekvencia hangolás
 * @param event Rotary encoder esemény (forgatás irány, érték, gombnyomás)
 * @return true ha sikeresen kezelte az eseményt, false egyébként
 *
 * @details FM frekvencia hangolás logika:
 * - Csak akkor reagál, ha nincs aktív dialógus
 * - Rotary klikket figyelmen kívül hagyja (más funkciókhoz)
 * - Frekvencia léptetés és mentés a band táblába
 * - Frekvencia kijelző azonnali frissítése
 */
bool ScreenFM::handleRotary(const RotaryEvent &event) {

    // Biztonsági ellenőrzés: csak aktív dialógus nélkül és nem klikk eseménykor
    if (!isDialogActive() && event.buttonState != RotaryEvent::ButtonState::Clicked) {

        // Frekvencia léptetés és automatikus mentés a band táblába
        // Beállítjuk a chip-en és le is mentjük a band táblába a frekvenciát
        uint16_t currFreq = ::pSi4735Manager->stepFrequency(event.value); // Léptetjük a rádiót
        ::pSi4735Manager->getCurrentBand().currFreq = currFreq;           // Beállítjuk a band táblában a frekit

        // RDS cache törlése frekvencia változás miatt
        if (rdsComponent) {
            rdsComponent->clearRdsOnFrequencyChange();
        }

        // Frekvencia kijelző azonnali frissítése
        if (freqDisplayComp) {
            freqDisplayComp->setFrequency(currFreq);
        }

        // Memória státusz ellenőrzése és frissítése
        checkAndUpdateMemoryStatus();

        return true; // Esemény sikeresen kezelve
    }

    // Ha nem kezeltük az eseményt, továbbítjuk a szülő osztálynak (dialógusokhoz)
    return UIScreen::handleRotary(event);
}

// ===================================================================
// Loop ciklus - Optimalizált teljesítmény
// ===================================================================

/**
 * @brief Folyamatos loop hívás - Csak valóban szükséges frissítések
 * @details Event-driven architektúra: NINCS folyamatos gombállapot pollozás!
 *
 * Csak az alábbi komponenseket frissíti minden ciklusban:
 * - S-Meter (jelerősség) - valós idejű adat
 *
 * Gombállapotok frissítése CSAK:
 * - Képernyő aktiválásakor (activate() metódus)
 * - Specifikus eseményekkor (eseménykezelőkben)
 */
void ScreenFM::handleOwnLoop() {

    // ===================================================================
    // S-Meter (jelerősség) időzített frissítése - Közös RadioScreen implementáció
    // ===================================================================
    updateSMeter(true /* FM mód */);

    // ===================================================================
    // RDS adatok valós idejű frissítése
    // ===================================================================
    if (rdsComponent) {
        static uint32_t lastRdsCall = 0;
        uint32_t currentTime = millis();

        // 500ms frissítési időköz az RDS adatokhoz
        if (currentTime - lastRdsCall >= 500) {
            rdsComponent->updateRDS();
            lastRdsCall = currentTime;
        }
    }

    // Néhány adatot csak ritkábban frissítünk
#define SCREEN_COMPS_REFRESH_TIME_MSEC 1000 // Frissítési időköz
    static uint32_t elapsedTimedValues = 0; // Kezdőérték nulla
    if ((millis() - elapsedTimedValues) >= SCREEN_COMPS_REFRESH_TIME_MSEC) {

        // ===================================================================
        // STEREO/MONO jelző frissítése
        // ===================================================================
        if (stereoIndicator) {
            // Si4735 stereo állapot lekérdezése
            bool isStereo = ::pSi4735Manager->getSi4735().getCurrentPilot();
            stereoIndicator->setStereo(isStereo);
        }

        // Frissítjük az időbélyeget
        elapsedTimedValues = millis();
    }
}

// ===================================================================
// Képernyő rajzolás és aktiválás
// ===================================================================

/**
 * @brief Statikus képernyő tartalom kirajzolása
 * @details Csak a statikus elemeket rajzolja ki (nem változó tartalom):
 * - S-Meter skála (vonalak, számok)
 *
 * A dinamikus tartalom (pl. S-Meter érték, spektrum oszlopok) a loop()-ban frissül.
 */
void ScreenFM::drawContent() {
    // S-Meter statikus skála kirajzolása (egyszer, a kezdetekkor)
    if (smeterComp) {
        smeterComp->drawSmeterScale();
    }
}

/**
 * @brief Képernyő aktiválása - Event-driven gombállapot szinkronizálás
 * @details Meghívódik, amikor a felhasználó erre a képernyőre vált.
 *
 * Ez az EGYETLEN hely, ahol a gombállapotokat szinkronizáljuk a rendszer állapotával:
 * - Mute gomb szinkronizálása rtv::muteStat-tal
 * - AM gomb szinkronizálása aktuális band típussal
 * - További állapotok szinkronizálása (AGC, Attenuator, stb.)
 */
void ScreenFM::activate() {

    DEBUG("ScreenFM::activate() - Képernyő aktiválása\n");

    // Szülő osztály aktiválása (ScreenRadioBase -> ScreenFrequDisplayBase -> UIScreen)
    ScreenRadioBase::activate();

    // StatusLine frissítése
    checkAndUpdateMemoryStatus();
}

/**
 * @brief Dialógus bezárásának kezelése - Gombállapot szinkronizálás
 * @details Az utolsó dialógus bezárásakor frissíti a gombállapotokat
 *
 * Ez a metódus biztosítja, hogy a gombállapotok konzisztensek maradjanak
 * a dialógusok bezárása után. Különösen fontos a ValueChangeDialog-ok
 * (Volume, Attenuator, Squelch, Frequency) után.
 */
void ScreenFM::onDialogClosed(UIDialogBase *closedDialog) {

    // Először hívjuk a RadioScreen implementációt (band váltás kezelés)
    ScreenRadioBase::onDialogClosed(closedDialog);

    // Ha ez volt az utolsó dialógus, frissítsük a gombállapotokat
    if (!isDialogActive()) {
        updateAllVerticalButtonStates();      // Függőleges gombok szinkronizálása
        updateCommonHorizontalButtonStates(); // Közös gombok szinkronizálása
        updateHorizontalButtonStates();       // FM specifikus gombok szinkronizálása

        // A gombsor konténer teljes újrarajzolása, hogy biztosan megjelenjenek a gombok
        if (horizontalButtonBar) {
            horizontalButtonBar->markForRedraw(true); // A konténert és annak összes gyerekét újra kell rajzolni.
        }
    }
}

// ===================================================================
// Vízszintes gombsor - Alsó navigációs gombok
// ===================================================================

/**
 * @brief FM specifikus gombok hozzáadása a közös gombokhoz
 * @param buttonConfigs A már meglévő gomb konfigurációk vektora
 * @details Felülírja az ős metódusát, hogy hozzáadja az FM specifikus gombokat
 */
void ScreenFM::addSpecificHorizontalButtons(std::vector<UIHorizontalButtonBar::ButtonConfig> &buttonConfigs) {
    // FM specifikus gombok hozzáadása a közös gombok után

    // 1. SEEK DOWN - Automatikus hangolás lefelé
    buttonConfigs.push_back({ScreenFMHorizontalButtonIDs::SEEK_DOWN_BUTTON, //
                             "Seek-",                                       //
                             UIButton::ButtonType::Pushable,                //
                             UIButton::ButtonState::Off,                    //
                             [this](const UIButton::ButtonEvent &event) {   //
                                 handleSeekDownButton(event);
                             }});

    // 2. SEEK UP - Automatikus hangolás felfelé
    buttonConfigs.push_back({                                             //
                             ScreenFMHorizontalButtonIDs::SEEK_UP_BUTTON, //
                             "Seek+",                                     //
                             UIButton::ButtonType::Pushable,              //
                             UIButton::ButtonState::Off,                  //
                             [this](const UIButton::ButtonEvent &event) { //
                                 handleSeekUpButton(event);
                             }});
}

/**
 * @brief Vízszintes gombsor létrehozása a képernyő alján
 * @details 4 navigációs gomb elhelyezése vízszintes elrendezésben:
 *
 * Gombsor pozíció: Bal alsó sarok, 4 gomb szélessége
 * Gombok (balról jobbra):
 * 1. SEEK DOWN - Automatikus hangolás lefelé (Pushable)
 * 2. SEEK UP - Automatikus hangolás felfelé (Pushable)
 * 3. AM - AM képernyőre váltás (Pushable)
 * 4. Test - Test képernyőre váltás (Pushable)
 */
/**
 * @brief FM specifikus vízszintes gombsor állapotainak szinkronizálása
 * @details Event-driven architektúra: CSAK aktiváláskor hívódik meg!
 *
 * Szinkronizált állapotok:
 * - FM specifikus gombok (Seek-, Seek+) alapértelmezett állapotai
 */
void ScreenFM::updateHorizontalButtonStates() {
    if (!horizontalButtonBar) {
        return; // Biztonsági ellenőrzés
    }

    // ===================================================================
    // FM specifikus gombok állapot szinkronizálása
    // ===================================================================

    // Seek gombok alapértelmezett állapotban (kikapcsolva)
    horizontalButtonBar->setButtonState(ScreenFMHorizontalButtonIDs::SEEK_DOWN_BUTTON, UIButton::ButtonState::Off);
    horizontalButtonBar->setButtonState(ScreenFMHorizontalButtonIDs::SEEK_UP_BUTTON, UIButton::ButtonState::Off);
}

// ===================================================================
// Vízszintes gomb eseménykezelők - Seek és navigációs funkciók
// ===================================================================

/**
 * @brief SEEK DOWN gomb eseménykezelő - Automatikus hangolás lefelé
 * @param event Gomb esemény (Clicked)
 *
 * @details Pushable gomb: Automatikus állomáskeresés lefelé
 * A seek során valós időben frissíti a frekvencia kijelzőt
 */
void ScreenFM::handleSeekDownButton(const UIButton::ButtonEvent &event) {
    if (event.state == UIButton::EventButtonState::Clicked) {
        // RDS cache törlése seek indítása előtt
        clearRDSCache(); // Seek lefelé a RadioScreen metódusával
        seekStationDown();

        // Seek befejezése után: RDS és memória státusz frissítése
        clearRDSCache();
        checkAndUpdateMemoryStatus();
    }
}

/**
 * @brief SEEK UP gomb eseménykezelő - Automatikus hangolás felfelé
 * @param event Gomb esemény (Clicked)
 *
 * @details Pushable gomb: Automatikus állomáskeresés felfelé
 * A seek során valós időben frissíti a frekvencia kijelzőt
 */
void ScreenFM::handleSeekUpButton(const UIButton::ButtonEvent &event) {
    if (event.state == UIButton::EventButtonState::Clicked) {
        // RDS cache törlése seek indítása előtt
        clearRDSCache(); // Seek felfelé a RadioScreen metódusával
        seekStationUp();

        // Seek befejezése után: RDS és memória státusz frissítése
        clearRDSCache();
        checkAndUpdateMemoryStatus();
    }
}

/**
 * @brief Egyedi MEMO gomb eseménykezelő - Intelligens memória kezelés
 * @param event Gomb esemény (Clicked)
 *
 * @details Ha az aktuális állomás még nincs a memóriában és van RDS állomásnév,
 * akkor automatikusan megnyitja a MemoryScreen-t név szerkesztő dialógussal
 */
void ScreenFM::handleMemoButton(const UIButton::ButtonEvent &event) {

    if (event.state != UIButton::EventButtonState::Clicked) {
        return;
    }

    auto screenManager = UIScreen::getScreenManager();

    // Ellenőrizzük, hogy az aktuális állomás már a memóriában van-e
    bool isInMemory = checkCurrentFrequencyInMemory();                // RDS állomásnév lekérése (ha van)
    String rdsStationName = ::pSi4735Manager->getCachedStationName(); // Ha új állomás és van RDS név, akkor automatikus hozzáadás

    DEBUG("ScreenFM::handleMemoButton() - Current frequency in memory: %s, RDS station name: %s\n", isInMemory ? "Yes" : "No", rdsStationName.c_str());

    // Paraméter átadása a MemoryScreen-nek
    if (!isInMemory && rdsStationName.length() > 0) {
        auto stationNamePtr = new std::shared_ptr<char>(new char[rdsStationName.length() + 1], std::default_delete<char[]>());
        strcpy(stationNamePtr->get(), rdsStationName.c_str());
        DEBUG("ScreenFM::handleMemoButton() - Navigating to MemoryScreen with RDS station name: %s\n", rdsStationName.c_str());
        screenManager->switchToScreen(SCREEN_NAME_MEMORY, stationNamePtr);
    } else {
        // Ha már a memóriában van, akkor csak visszalépünk a Memória képernyőre
        DEBUG("ScreenFM::handleMemoButton() - Navigating to MemoryScreen without RDS station name\n");
        screenManager->switchToScreen(SCREEN_NAME_MEMORY);
    }
}

/**
 * @brief Egyedi függőleges gombok létrehozása - Memo gomb override-dal
 * @details Felülírja a CommonVerticalButtons alapértelmezett Memo kezelőjét
 */
void ScreenFM::createCommonVerticalButtons() {

    // Alapértelmezett gombdefiníciók lekérése
    const auto &baseDefs = CommonVerticalButtons::getButtonDefinitions();

    // Egyedi gombdefiníciók lista létrehozása
    std::vector<ButtonGroupDefinition> customDefs;
    customDefs.reserve(baseDefs.size());

    // Végigmegyünk az alapértelmezett definíciókon
    for (const auto &def : baseDefs) {
        std::function<void(const UIButton::ButtonEvent &)> callback;

        // Memo gomb speciális kezelése
        if (def.id == VerticalButtonIDs::MEMO) {
            // Egyedi Memo handler használata
            callback = [this](const UIButton::ButtonEvent &e) { this->handleMemoButton(e); };
        } else if (def.handler != nullptr) {
            // Többi gomb: eredeti handler használata
            callback = [screen = this, handler = def.handler](const UIButton::ButtonEvent &e) { handler(e, screen); };
        } else {
            // No-op callback üres handlerekhez
            callback = [](const UIButton::ButtonEvent &e) { /* no-op */ };
        }

        // Gombdefiníció hozzáadása a listához
        customDefs.push_back({def.id, def.label, def.type, callback, def.initialState,
                              60, // uniformWidth
                              def.height});
    }

    // Gombok létrehozása és elhelyezése
    ButtonsGroupManager<ScreenFM>::layoutVerticalButtonGroup(customDefs, &createdVerticalButtons, 0, 0, 5, 60, 32, 3, 4);
}
