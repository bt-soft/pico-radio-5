#include "ScreenMemory.h"
#include "MessageDialog.h"
#include "Si4735Manager.h"
#include "VirtualKeyboardDialog.h"

// ===================================================================
// Vízszintes gombsor azonosítók - Képernyő-specifikus navigáció
// ===================================================================
namespace ScreenMemoryHorizontalButtonIDs {
static constexpr uint8_t ADD_CURRENT_BUTTON = 30;
static constexpr uint8_t EDIT_BUTTON = 31;
static constexpr uint8_t DELETE_BUTTON = 32;
static constexpr uint8_t BACK_BUTTON = 33;
} // namespace ScreenMemoryHorizontalButtonIDs

// ===================================================================
// Konstruktor és inicializálás
// ===================================================================

ScreenMemory::ScreenMemory() : UIScreen(SCREEN_NAME_MEMORY) {

    // Aktuális sáv típus meghatározása
    isFmMode = isCurrentBandFm();

    layoutComponents();
    loadStations();
}

ScreenMemory::~ScreenMemory() {}

// ===================================================================
// UI komponensek layout és elhelyezés
// ===================================================================

/**
 * @brief Elrendezi a képernyő komponenseit
 */
void ScreenMemory::layoutComponents() {

    // Lista komponens létrehozása
    // Rect listBounds(5, 25, 400, 250);

    // Képernyő dimenzióinak és margóinak meghatározása
    const int16_t margin = 5;
    const int16_t buttonHeight = UIButton::DEFAULT_BUTTON_HEIGHT;
    const int16_t listTopMargin = 30;                            // Hely a címnek
    const int16_t listBottomPadding = buttonHeight + margin * 2; // Hely az Exit gombnak    // Görgethető lista komponens létrehozása és hozzáadása a gyermek komponensekhez
    Rect listBounds(margin, listTopMargin, ::SCREEN_W - (2 * margin), ::SCREEN_H - listTopMargin - listBottomPadding);
    memoryList = std::make_shared<UIScrollableListComponent>(listBounds, this, 6, 27); // itemHeight megnövelve 20-ról 26-ra
    addChild(memoryList);

    // Vízszintes gombsor létrehozása
    createHorizontalButtonBar();
}

/**
 * @brief Létrehozza a vízszintes gombsor komponensét
 */
void ScreenMemory::createHorizontalButtonBar() {
    using namespace ScreenMemoryHorizontalButtonIDs; // Gombsor pozíció számítása (képernyő alján)
    constexpr int16_t margin = 5;
    uint16_t buttonY = ::SCREEN_H - UIButton::DEFAULT_BUTTON_HEIGHT - margin;
    uint16_t buttonWidth = 90;
    uint16_t buttonHeight = UIButton::DEFAULT_BUTTON_HEIGHT;
    uint16_t spacing = 5;

    // Csak az első 3 gombhoz számítjuk a szélességet (Back gomb külön lesz)
    uint16_t totalWidth = 3 * buttonWidth + 2 * spacing;
    uint16_t startX = margin;

    Rect buttonRect(startX, buttonY, totalWidth, buttonHeight);

    // Gomb konfigurációk létrehozása (Back gomb nélkül)
    std::vector<UIHorizontalButtonBar::ButtonConfig> buttonConfigs = {
        {ADD_CURRENT_BUTTON, "Add Curr", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off, [this](const UIButton::ButtonEvent &event) { handleAddCurrentButton(event); }},
        {EDIT_BUTTON, "Edit", UIButton::ButtonType::Pushable, UIButton::ButtonState::Disabled, [this](const UIButton::ButtonEvent &event) { handleEditButton(event); }},
        {DELETE_BUTTON, "Delete", UIButton::ButtonType::Pushable, UIButton::ButtonState::Disabled, [this](const UIButton::ButtonEvent &event) { handleDeleteButton(event); }}};

    horizontalButtonBar = std::make_shared<UIHorizontalButtonBar>(buttonRect, buttonConfigs, buttonWidth, buttonHeight, spacing);
    addChild(horizontalButtonBar);

    // Back gomb külön, jobbra igazítva
    uint16_t backButtonWidth = 60;
    uint16_t backButtonX = ::SCREEN_W - backButtonWidth - margin;
    Rect backButtonRect(backButtonX, buttonY, backButtonWidth, buttonHeight);

    backButton = std::make_shared<UIButton>(BACK_BUTTON, backButtonRect, "Back", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off, [this](const UIButton::ButtonEvent &event) { handleBackButton(event); });
    addChild(backButton);
}

// ===================================================================
// Adatok kezelése
// ===================================================================

/**
 * @brief Betölti az állomásokat a memóriából
 */
void ScreenMemory::loadStations() {
    stations.clear();

    if (isFmMode) {
        uint8_t count = fmStationStore.getStationCount();
        for (uint8_t i = 0; i < count; i++) {
            const StationData *stationData = fmStationStore.getStationByIndex(i);
            if (stationData) {
                stations.push_back(*stationData);
            }
        }
    } else {
        uint8_t count = amStationStore.getStationCount();
        for (uint8_t i = 0; i < count; i++) {
            const StationData *stationData = amStationStore.getStationByIndex(i);
            if (stationData) {
                stations.push_back(*stationData);
            }
        }
    }
    DEBUG("Loaded %d stations for %s mode\n", stations.size(), isFmMode ? "FM" : "AM");

    // Első elem automatikus kiválasztása, ha van állomás
    if (stations.size() > 0) {
        selectedIndex = 0;
        // A UIScrollableListComponent automatikusan 0-ra állítja a selectedItemIndex-et
    } else {
        selectedIndex = -1;
    }

    // Inicializáljuk a lastTunedIndex változót
    lastTunedIndex = -1;
    for (int i = 0; i < stations.size(); i++) {
        if (isStationCurrentlyTuned(stations[i])) {
            lastTunedIndex = i;
            break;
        }
    }
}

/**
 * @brief Frissíti a lista tartalmát és a gombok állapotát
 */
void ScreenMemory::refreshList() {
    loadStations();
    if (memoryList) {
        memoryList->markForRedraw();
    }
    updateHorizontalButtonStates();
}

/**
 * @brief Frissíti a vízszintes gombok állapotát
 */
void ScreenMemory::refreshCurrentTunedIndication() {
    // Csak a lista megjelenítését frissítjük, nem töltjük újra az adatokat
    if (memoryList) {
        memoryList->markForRedraw();
    }
    updateHorizontalButtonStates();
}

/**
 * @brief Optimalizált frissítés a behangolt állomás jelzésére
 */
void ScreenMemory::refreshTunedIndicationOptimized() {
    if (!memoryList) {
        return;
    }

    // Megkeressük a jelenleg behangolt állomás indexét
    int currentTunedIndex = -1;
    for (int i = 0; i < stations.size(); i++) {
        if (isStationCurrentlyTuned(stations[i])) {
            currentTunedIndex = i;
            break;
        }
    }

    // Ha megváltozott a behangolt állomás, frissítjük az érintett elemeket
    if (currentTunedIndex != lastTunedIndex) {
        // Korábbi behangolt elem frissítése (ha volt)
        if (lastTunedIndex >= 0 && lastTunedIndex < stations.size()) {
            memoryList->redrawListItem(lastTunedIndex);
        }

        // Új behangolt elem frissítése (ha van)
        if (currentTunedIndex >= 0 && currentTunedIndex < stations.size()) {
            memoryList->redrawListItem(currentTunedIndex);
        }

        // Frissítjük a nyilvántartást
        lastTunedIndex = currentTunedIndex;
    }

    updateHorizontalButtonStates();
}

// ===================================================================
// IScrollableListDataSource interface
// ===================================================================

/**
 * @brief Visszaadja a lista elemeinek teljes számát
 */
int ScreenMemory::getItemCount() const { return stations.size(); }

/**
 * @brief Visszaadja az adott indexű elem adatait
 * @param index Az elem indexe
 */
String ScreenMemory::getItemLabelAt(int index) const {
    if (index < 0 || index >= stations.size()) {
        return "";
    }
    const StationData &station = stations[index];

    // Fix formátum: mindig ugyanolyan pozícióban kezdődik a szöveg
    String label = "";
    if (const_cast<ScreenMemory *>(this)->isStationCurrentlyTuned(station)) {
        label = String(CURRENT_TUNED_ICON) + String(station.name);
    } else {
        // Szóközök ugyanolyan hosszban mint a CURRENT_TUNED_ICON ("> ")
        label = "   " + String(station.name); // 2 szóköz a "> " helyett
    }

    return label;
}

/**
 * @brief Visszaadja az adott indexű elem értékét (pl. frekvencia és moduláció)
 * @param index Az elem indexe
 */
String ScreenMemory::getItemValueAt(int index) const {
    if (index < 0 || index >= stations.size()) {
        return "";
    }

    const StationData &station = stations[index];
    String freq = formatFrequency(station.frequency, isFmMode);
    String mod = getModulationName(station.modulation);

    return freq + " " + mod;
}

/**
 * @brief Kezeli az elemre való kattintást
 * @param index Az elem indexe
 */
bool ScreenMemory::onItemClicked(int index) {
    selectedIndex = index;
    tuneToStation(index);
    updateHorizontalButtonStates();
    // Optimalizált frissítés - csak az érintett elemek újrarajzolása
    refreshTunedIndicationOptimized();
    return false; // Nincs szükség teljes újrarajzolásra
}

// ===================================================================
// UIScreen interface
// ===================================================================

/**
 * @brief Kezeli a forgatógomb eseményeket
 * @param event A forgatógomb esemény
 */
bool ScreenMemory::handleRotary(const RotaryEvent &event) {
    // Ha van aktív dialógus, továbbítjuk neki
    if (isDialogActive()) {
        return UIScreen::handleRotary(event);
    }

    // Lista scrollozás
    if (memoryList) {
        return memoryList->handleRotary(event);
    }

    return false;
}

/**
 * @brief Saját loop logika kezelése
 */
void ScreenMemory::handleOwnLoop() {
    // Nincs speciális loop logika szükséges
}

/**
 * @brief Kirajzolja a képernyő tartalmát
 */
void ScreenMemory::drawContent() {
    // Cím kirajzolása memória állapottal
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);

    // Memória állapot hozzáadása a címhez
    uint8_t currentCount = getCurrentStationCount();
    uint8_t maxCount = getMaxStationCount();
    String title = String(isFmMode ? "FM Memory" : "AM Memory") + " (" + String(currentCount) + "/" + String(maxCount) + ")";
    tft.drawString(title, ::SCREEN_W / 2, 5);

    // Komponensek már automatikusan kirajzolódnak
}

/**
 * @brief Aktiválja a képernyőt és frissíti a sáv típusát
 */
void ScreenMemory::activate() {
    DEBUG("ScreenMemory activated\n"); // Sáv típus frissítése
    bool newFmMode = isCurrentBandFm();
    if (newFmMode != isFmMode) {
        isFmMode = newFmMode;
        refreshList();
    } else {
        // Ha a sáv típus nem változott, csak a behangolt állomás jelzését frissítjük optimalizáltan
        refreshTunedIndicationOptimized();
    }
    updateHorizontalButtonStates();
    UIScreen::activate();

    // FMScreen-ből való automatikus station add ellenőrzése
    if (rdsStationName.length() > 0) {
        // Automatikus "Add Curr" gomb megnyomás szimulálása
        handleAddCurrentButton(UIButton::ButtonEvent{ADD_CURRENT_BUTTON, "Add Curr", UIButton::EventButtonState::Clicked});
        // Paraméterek törlése, hogy ne ismétlődjön
    }
}

/**
 * @brief Kezeli a dialógus bezárását
 * @param closedDialog A bezárt dialógus
 */
void ScreenMemory::onDialogClosed(UIDialogBase *closedDialog) {
    UIScreen::onDialogClosed(closedDialog);

    // Dialógus állapot resetelése
    currentDialogState = DialogState::None;

    // Gombok állapotának frissítése és explicit újrarajzolás
    updateHorizontalButtonStates();

    // A gombsor konténer teljes újrarajzolása, hogy biztosan megjelenjenek a gombok
    if (horizontalButtonBar) {
        horizontalButtonBar->markForRedraw(true);
    }
}

/**
 * @brief Paraméterek beállítása FMScreen-ből való navigáláskor
 * @param params ScreenMemoryParams struktúra pointere
 */
void ScreenMemory::setParameters(void *params) {
    if (params) {

        // Átveszi a tulajdonjogot
        auto stationNamePtr = static_cast<std::shared_ptr<char> *>(params);
        rdsStationName = String(stationNamePtr->get()); // std::shared_ptr<char> -> String konverzió
        DEBUG("ScreenMemory: Received RDS station name: %s\n", rdsStationName);
        stationNamePtr->reset(); // Reseteljük a std::shared_ptr-t, hogy felszabaduljon a memória
    } else {
        // Paraméter resetelése
        rdsStationName = "";
    }
}

// ===================================================================
// Gomb eseménykezelők
// ===================================================================

void ScreenMemory::handleAddCurrentButton(const UIButton::ButtonEvent &event) {
    if (event.state == UIButton::EventButtonState::Clicked) {
        // Ellenőrizzük, hogy az aktuális állomás már létezik-e
        if (isCurrentStationInMemory()) {
            // Figyelmeztető dialógus megjelenítése
            showStationExistsDialog();
        } else {
            showAddStationDialog();
        }
    }
}

void ScreenMemory::handleEditButton(const UIButton::ButtonEvent &event) {
    if (event.state == UIButton::EventButtonState::Clicked && selectedIndex >= 0) {
        showEditStationDialog();
    }
}

void ScreenMemory::handleDeleteButton(const UIButton::ButtonEvent &event) {
    if (event.state == UIButton::EventButtonState::Clicked && selectedIndex >= 0) {
        showDeleteConfirmDialog();
    }
}

void ScreenMemory::handleBackButton(const UIButton::ButtonEvent &event) {
    if (event.state == UIButton::EventButtonState::Clicked) {
        // Visszatérés az előző képernyőre
        if (getScreenManager()) {
            getScreenManager()->goBack();
        }
    }
}

// ===================================================================
// Dialógus kezelők
// ===================================================================

/**
 * @brief Megjeleníti az állomás hozzáadása dialógust
 * @details A dialógus megjeleníti az aktuális RDS állomás nevét, ha van, és lehetőséget ad új állomás hozzáadására.
 */
void ScreenMemory::showAddStationDialog() {
    currentDialogState = DialogState::AddingStation;
    pendingStation = getCurrentStationData(); // RDS állomásnév használata kezdeti értékként, ha van
    auto keyboardDialog = std::make_shared<VirtualKeyboardDialog>(this, "Add Station", rdsStationName, MAX_STATION_NAME_LEN, [this](const String &newText) {
        // Szöveg változás callback - itt nem csinálunk semmit
    });

    // Dialog bezárás kezelése
    keyboardDialog->setDialogCallback([this, keyboardDialog](UIDialogBase *dialog, UIDialogBase::DialogResult result) {
        if (result == UIDialogBase::DialogResult::Accepted && currentDialogState == DialogState::AddingStation) {
            String stationName = keyboardDialog->getCurrentText();
            if (stationName.length() > 0) {
                addCurrentStation(stationName);
            }
        }
        currentDialogState = DialogState::None;
    });

    showDialog(keyboardDialog);
}

/**
 * @brief Megjeleníti az állomás szerkesztése dialógust
 * @details A dialógus lehetőséget ad a kiválasztott állomás nevének szerkesztésére.
 */
void ScreenMemory::showEditStationDialog() {
    if (selectedIndex < 0 || selectedIndex >= stations.size()) {
        return;
    }

    currentDialogState = DialogState::EditingStationName;
    String currentName = stations[selectedIndex].name;

    auto keyboardDialog = std::make_shared<VirtualKeyboardDialog>(this, "Edit Name", currentName, MAX_STATION_NAME_LEN, [this](const String &newText) {
        // Szöveg változás callback
    });

    keyboardDialog->setDialogCallback([this, keyboardDialog](UIDialogBase *dialog, UIDialogBase::DialogResult result) {
        if (result == UIDialogBase::DialogResult::Accepted && currentDialogState == DialogState::EditingStationName && selectedIndex >= 0) {
            String newName = keyboardDialog->getCurrentText();
            if (newName.length() > 0) {
                updateStationName(selectedIndex, newName);
            }
        }
        currentDialogState = DialogState::None;
    });

    showDialog(keyboardDialog);
}

/**
 * @brief Törlés megerősítő dialógus megjelenítése
 * @details Megjeleníti a törlés megerősítő dialógust a kiválasztott állomás törléséhez
 */
void ScreenMemory::showDeleteConfirmDialog() {
    if (selectedIndex < 0 || selectedIndex >= stations.size()) {
        return;
    }

    currentDialogState = DialogState::ConfirmingDelete;

    // Stabil tagváltozó buffer használata a string életciklus biztosításához
    const char *stationName = stations[selectedIndex].name;
    String freqStr = formatFrequency(stations[selectedIndex].frequency, isFmMode);

    snprintf(deleteMessageBuffer, sizeof(deleteMessageBuffer), "Delete station:\n%s\n%s?", stationName, freqStr.c_str());
    auto confirmDialog = std::make_shared<MessageDialog>(this, "Confirm Delete", deleteMessageBuffer, MessageDialog::ButtonsType::YesNo, Rect(-1, -1, 250, 0));

    confirmDialog->setDialogCallback([this](UIDialogBase *dialog, UIDialogBase::DialogResult result) {
        if (result == UIDialogBase::DialogResult::Accepted && currentDialogState == DialogState::ConfirmingDelete && selectedIndex >= 0) {
            deleteStation(selectedIndex);
            selectedIndex = -1;
        }
        currentDialogState = DialogState::None;
    });

    showDialog(confirmDialog);
}

/**
 *  @brief Állomás már létezik dialógus megjelenítése
 */
void ScreenMemory::showStationExistsDialog() {
    StationData currentStation = getCurrentStationData();
    String freqStr = formatFrequency(currentStation.frequency, isFmMode);
    String modStr = getModulationName(currentStation.modulation);

    String message = "Station already exists in memory:\n" + freqStr + " " + modStr;
    auto infoDialog = std::make_shared<MessageDialog>(this, "Station Exists", message.c_str(), MessageDialog::ButtonsType::Ok, Rect(-1, -1, 280, 0));
    infoDialog->setDialogCallback([this](UIDialogBase *dialog, UIDialogBase::DialogResult result) {
        // Nincs teendő, csak tájékoztatás
    });

    showDialog(infoDialog);
}

// ===================================================================
// Állomás műveletek
// ===================================================================

void ScreenMemory::tuneToStation(int index) {
    if (index < 0 || index >= stations.size()) {
        return;
    }

    const StationData &station = stations[index];

    DEBUG("Tuning to station: %s, freq: %d\n", station.name, station.frequency);

    // Si4735Manager::tuneMemoryStation használata (öröklés Si4735Band-ből)
    if (::pSi4735Manager->isCurrentBandFM()) {
        ::pSi4735Manager->clearRdsCache(); // RDS törlése FM módban
    }
    ::pSi4735Manager->tuneMemoryStation(station.bandIndex, station.frequency, station.modulation, station.bandwidthIndex);
}

void ScreenMemory::addCurrentStation(const String &name) {
    // Memória telített ellenőrzése
    if (isMemoryFull()) {
        auto dialog = std::make_shared<MessageDialog>(this, "Hiba", "Memória megtelt!\nTöröljön állomásokat.", MessageDialog::ButtonsType::Ok, Rect(-1, -1, 300, 120));
        showDialog(dialog);
        return;
    }

    StationData newStation = getCurrentStationData();
    strncpy(newStation.name, name.c_str(), MAX_STATION_NAME_LEN);
    newStation.name[MAX_STATION_NAME_LEN] = '\0'; // Store-ba mentés
    if (isFmMode) {
        if (fmStationStore.addStation(newStation)) {
            DEBUG("FM station added: %s\n", newStation.name);
        } else {
            DEBUG("Failed to add FM station\n");
        }
    } else {
        if (amStationStore.addStation(newStation)) {
            DEBUG("AM station added: %s\n", newStation.name);
        } else {
            DEBUG("Failed to add AM station\n");
        }
    }

    refreshList();
}

void ScreenMemory::updateStationName(int index, const String &newName) {
    if (index < 0 || index >= stations.size()) {
        return;
    }

    StationData updatedStation = stations[index];
    strncpy(updatedStation.name, newName.c_str(), MAX_STATION_NAME_LEN);
    updatedStation.name[MAX_STATION_NAME_LEN] = '\0'; // Store-ban frissítés
    if (isFmMode) {
        if (fmStationStore.updateStation(index, updatedStation)) {
            DEBUG("FM station updated: %s\n", updatedStation.name);
        } else {
            DEBUG("Failed to update FM station\n");
        }
    } else {
        if (amStationStore.updateStation(index, updatedStation)) {
            DEBUG("AM station updated: %s\n", updatedStation.name);
        } else {
            DEBUG("Failed to update AM station\n");
        }
    }

    refreshList();
}

void ScreenMemory::deleteStation(int index) {
    if (index < 0 || index >= stations.size()) {
        return;
    } // Store-ból törlés
    if (isFmMode) {
        if (fmStationStore.deleteStation(index)) {
            DEBUG("FM station deleted at index %d\n", index);
        } else {
            DEBUG("Failed to delete FM station\n");
        }
    } else {
        if (amStationStore.deleteStation(index)) {
            DEBUG("AM station deleted at index %d\n", index);
        } else {
            DEBUG("Failed to delete AM station\n");
        }
    }

    refreshList();
}

// ===================================================================
// Segéd metódusok
// ===================================================================

StationData ScreenMemory::getCurrentStationData() {
    StationData station = {0};

    if (pSi4735Manager) {
        // Si4735Manager közvetlenül örökli a Band metódusokat
        auto &currentBand = pSi4735Manager->getCurrentBand();

        station.bandIndex = config.data.currentBandIdx; // Band index a config-ból
        station.frequency = currentBand.currFreq;
        station.modulation = currentBand.currDemod;
        station.bandwidthIndex = 0; // TODO: Ha van bandwidth index tárolás
    }

    return station;
}

String ScreenMemory::formatFrequency(uint16_t frequency, bool isFm) const {
    if (isFm) {
        // FM: 10kHz egységben tárolt, MHz-ben megjelenítve
        return String(frequency / 100.0, 1) + " MHz";
    } else {
        // AM: kHz-ben tárolt és megjelenített
        return String(frequency) + " kHz";
    }
}

String ScreenMemory::getModulationName(uint8_t modulation) const {
    switch (modulation) {
        case 0:
            return "FM";
        case 1:
            return "AM";
        case 2:
            return "LSB";
        case 3:
            return "USB";
        case 4:
            return "CW";
        default:
            return "???";
    }
}

bool ScreenMemory::isCurrentBandFm() {
    if (pSi4735Manager) {
        return pSi4735Manager->isCurrentBandFM();
    }
    return true; // Default FM
}

void ScreenMemory::updateHorizontalButtonStates() {
    if (!horizontalButtonBar) {
        return;
    }

    using namespace ScreenMemoryHorizontalButtonIDs;

    // Edit és Delete gombok csak akkor aktívak, ha van kiválasztott elem
    bool hasSelection = (selectedIndex >= 0 && selectedIndex < stations.size());

    horizontalButtonBar->setButtonState(EDIT_BUTTON, hasSelection ? UIButton::ButtonState::Off : UIButton::ButtonState::Disabled);
    horizontalButtonBar->setButtonState(DELETE_BUTTON, hasSelection ? UIButton::ButtonState::Off
                                                                    : UIButton::ButtonState::Disabled); // Add Current gomb állapota - letiltva, ha az aktuális állomás már a memóriában van VAGY a memória tele van
    bool currentStationExists = isCurrentStationInMemory();
    bool memoryFull = isMemoryFull();
    horizontalButtonBar->setButtonState(ADD_CURRENT_BUTTON, (currentStationExists || memoryFull) ? UIButton::ButtonState::Disabled : UIButton::ButtonState::Off);
}

bool ScreenMemory::isCurrentStationInMemory() {
    StationData currentStation = getCurrentStationData();

    // Ellenőrizzük, hogy az aktuális frekvencia és moduláció már létezik-e a memóriában
    for (const StationData &station : stations) {
        if (station.frequency == currentStation.frequency && station.modulation == currentStation.modulation) {
            return true;
        }
    }

    return false;
}

bool ScreenMemory::isStationCurrentlyTuned(const StationData &station) {
    StationData currentStation = getCurrentStationData();

    // Alapvető összehasonlítás: frekvencia és moduláció
    bool basicMatch = (station.frequency == currentStation.frequency && station.modulation == currentStation.modulation);

    return basicMatch;
}

uint8_t ScreenMemory::getCurrentStationCount() const { return isFmMode ? fmStationStore.getStationCount() : amStationStore.getStationCount(); }

uint8_t ScreenMemory::getMaxStationCount() const { return isFmMode ? MAX_FM_STATIONS : MAX_AM_STATIONS; }

bool ScreenMemory::isMemoryFull() const { return getCurrentStationCount() >= getMaxStationCount(); }
