
#include "SystemInfoDialog.h"
#include "EepromLayout.h" // EEPROM konstansokhoz

/**
 * @brief SystemInfoDialog konstruktor
 * @param parentScreen Szülő képernyő komponens
 * @param tft TFT kijelző referencia
 * @param bounds Dialógus területének koordinátái és méretei
 * @param cs Színséma a dialógus megjelenítéséhez
 * @details A konstruktor inicializálja a dialógust üres üzenet stringgel,
 * mert a tartalmat saját drawSelf metódussal rajzoljuk ki.
 * Az aktuális oldal nulláról indul (első oldal).
 * FONTOS: Explicit layoutDialogContent() hívás szükséges, mert a virtuális
 * hívás a MessageDialog konstruktorban a MessageDialog::layoutDialogContent()-et
 * hívná meg, nem a mi felülírt változatunkat.
 */
SystemInfoDialog::SystemInfoDialog(UIScreen *parentScreen, const Rect &bounds, const ColorScheme &cs) //
    : MessageDialog(parentScreen, "System Information", "", ButtonsType::Ok, bounds, cs), currentPage(0) {

    // Üres message stringet adunk át, mert saját drawSelf-fel rajzoljuk a tartalmat
    // Lapozós navigáció gombokkal történik
    // A navigációs gombokat a layoutDialogContent()-ben hozzuk létre

    // FONTOS: Explicit layoutDialogContent() hívás szükséges, mert a virtuális
    // hívás a MessageDialog konstruktorban a MessageDialog::layoutDialogContent()-et
    // hívná meg, nem a mi felülírt változatunkat.
    layoutDialogContent();
}

/**
 * @brief Visszaadja az összes dialógus gombot (kivéve a bezáró X gombot).
 * @return A gombok listája shared_ptr-ekben.
 * @details Kombinálja a MessageDialog gombokat (OK) és a SystemInfoDialog navigációs gombokat (Previous, Next).
 * Ez biztosítja, hogy az egységes getButtonsList() interfész minden gombot visszaadjon.
 */
std::vector<std::shared_ptr<UIButton>> SystemInfoDialog::getButtonsList() const {
    // Kezdjük a MessageDialog gombok listájával (OK gomb)
    std::vector<std::shared_ptr<UIButton>> allButtons = MessageDialog::getButtonsList();

    // Adjuk hozzá a navigációs gombokat, ha léteznek
    if (prevButton) {
        allButtons.push_back(prevButton);
    }
    if (nextButton) {
        allButtons.push_back(nextButton);
    }

    return allButtons;
}

/**
 * @brief Az aktuális oldal tartalmának lekérése
 * @return Az aktuális oldalhoz tartozó formázott string tartalom
 * @details Az aktuális oldal számának (currentPage) megfelelően visszaadja
 * a megfelelő formázott információs tartalmat. 4 különböző oldal van:
 * - 0: Program információk (név, verzió, szerző, build idő)
 * - 1: Memória állapot (Flash, RAM, EEPROM használat)
 * - 2: Hardware információk (MCU, kijelző adatok)
 * - 3: Si4735 rádio chip információk
 */
String SystemInfoDialog::getCurrentPageContent() {
    switch (currentPage) {
        case 0:
            return formatProgramInfo();
        case 1:
            return formatMemoryInfo();
        case 2:
            return formatHardwareInfo();
        case 3:
            return formatSi4735Info();
        default:
            return "Invalid page";
    }
}

/**
 * @brief Program információk formázása első oldalhoz
 * @return Formázott string a program adataival
 * @details Megjeleníti a program nevét, verzióját, szerzőjét és a build időpontját.
 * Az adatok a defines.h fájlban definiált makrókból és a fordító beépített
 * __DATE__ és __TIME__ makróiból származnak.
 */
String SystemInfoDialog::formatProgramInfo() {
    String info = "";
    info += "=== Program Information ===\n";
    info += "Name: " PROGRAM_NAME "\n";
    info += "Version: " PROGRAM_VERSION "\n";
    info += "Author: " PROGRAM_AUTHOR "\n";
    info += "Built: " __DATE__ " " __TIME__ "\n\n";
    return info;
}

/**
 * @brief Memória információk formázása második oldalhoz
 * @return Formázott string a memória állapottal
 * @details Részletes memória használati információkat jelenít meg:
 * - Flash memória: teljes méret, használt rész, szabad terület százalékokkal
 * - RAM (Heap): aktuális heap használat valós időben
 * - EEPROM: konfigurációs adatok tárolási helyének használata
 *
 * A memória adatok a PicoMemoryInfo::getMemoryStatus() függvényből származnak,
 * amely valós időben lekéri az aktuális memória állapotot.
 * Az EEPROM használat kiszámítása a definiált címtartományok alapján történik.
 */
String SystemInfoDialog::formatMemoryInfo() {
    // Aktuális memória állapot lekérése
    PicoMemoryInfo::MemoryStatus_t memStatus = PicoMemoryInfo::getMemoryStatus();

    // EEPROM használat számítása
    uint32_t eepromUsed = EEPROM_AM_STATIONS_ADDR + AM_STATIONS_REQUIRED_SIZE;
    uint32_t eepromFree = EEPROM_SIZE - eepromUsed;
    float eepromUsedPercent = (eepromUsed * 100.0f) / EEPROM_SIZE;

    String info = "";
    info += "=== Memory Information ===\n\n";

    info += "Flash Memory:\n";
    info += "  Total: " + String(FULL_FLASH_SIZE / 1024) + " kB\n";
    info += "  Used: " + String(memStatus.programSize / 1024) + " kB (" + String(memStatus.programPercent, 1) + "%)\n";
    info += "  Free: " + String(memStatus.freeFlash / 1024) + " kB\n\n";

    info += "RAM (Heap):\n";
    info += "  Total: " + String(memStatus.heapSize / 1024) + " kB\n";
    info += "  Used: " + String(memStatus.usedHeap / 1024) + " kB (" + String(memStatus.usedHeapPercent, 1) + "%)\n";
    info += "  Free: " + String(memStatus.freeHeap / 1024) + " kB\n\n";

    info += "EEPROM Storage:\n";
    info += "  Total: " + String(EEPROM_SIZE) + " B\n";
    info += "  Used: " + String(eepromUsed) + " B (" + String(eepromUsedPercent, 1) + "%)\n";
    info += "  Free: " + String(eepromFree) + " B";

    return info;
}

/**
 * @brief Hardware információk formázása harmadik oldalhoz
 * @return Formázott string a hardware adatokkal
 * @details Megjeleníti a rendszer hardware specifikációit:
 * - MCU típusa és órajele (RP2040 @ 133MHz)
 * - Kijelző típusa és felbontása (TFT 320x240)
 * További hardware információk később bővíthetők.
 */
String SystemInfoDialog::formatHardwareInfo() {
    String info = "";
    info += "=== Hardware Information ===\n";
    info += "MCU: RP2040 @ " + String(F_CPU / 1000000) + "MHz\n";
    info += "Display: TFT " + String(tft.width()) + "x" + String(tft.height()) + "\n\n";
    return info;
}

/**
 * @brief Si4735 rádio chip információk formázása negyedik oldalhoz
 * @return Formázott string a rádio chip adataival
 * @details Megjeleníti a Si4735 rádio chip információit.
 * Jelenleg alapvető információkat tartalmaz, de később kibővíthető
 * valós chip státusz lekérdezéssel, verzió információkkal, stb.
 */
String SystemInfoDialog::formatSi4735Info() {
    String info = "";
    info += "=== Radio Information ===\n";
    info += "Chip: Si4735\n";
    info += "Status: (To be implemented)\n";
    return info;
}

/**
 * @brief A dialógus tartalmának elrendezése - gombok létrehozása és pozicionálása
 * @details Ez a metódus felelős az összes gomb (OK, navigációs) létrehozásáért és konfigurálásáért.
 * Először meghívja a szülő osztály layout metódusát az OK gomb létrehozásához,
 * majd létrehozza a lapozáshoz szükséges Previous és Next gombokat.
 * Minden gombra mini fontot állít be a kompakt megjelenés érdekében.
 */
void SystemInfoDialog::layoutDialogContent() {
    // Közös konstansok definiálása a jobb karbantarthatóság érdekében
    constexpr int16_t COMPACT_BUTTON_HEIGHT = 25; // Minden gomb magassága
    constexpr int16_t COMPACT_BUTTON_WIDTH = 60;  // Navigációs gombok szélessége
    constexpr int16_t BOTTOM_MARGIN = 10;         // Alsó margó
    constexpr int16_t SIDE_SPACING = 10;          // Oldalsó távolság

    // Számított Y pozíció minden gombhoz (OK és navigációs egyaránt)
    const int16_t buttonY = bounds.y + bounds.height - COMPACT_BUTTON_HEIGHT - BOTTOM_MARGIN;

    // Először hívjuk meg a szülő osztály layout metódusát, amely létrehozza az OK gombot
    MessageDialog::layoutDialogContent();

    // Korábbi navigációs gombok eltávolítása
    if (prevButton) {
        removeChild(prevButton);
        prevButton = nullptr;
    }
    if (nextButton) {
        removeChild(nextButton);
        nextButton = nullptr;
    } // Közös lambda az oldalváltáshoz és UI frissítéshez - helyben definiáljuk
    auto updatePageAndUI = [this]() {
        updateNavigationButtons();
        markForRedraw(true); // Az összes gyereket is újrarajzolja
    };

    // Previous gomb létrehozása (bal oldal)
    int16_t prevButtonX = bounds.x + SIDE_SPACING;
    prevButton = std::make_shared<UIButton>(                                     //
        200,                                                                     // A gomb ID-je 200
        Rect(prevButtonX, buttonY, COMPACT_BUTTON_WIDTH, COMPACT_BUTTON_HEIGHT), // A gomb területe
        "< Prev",                                                                // A gomb szövege
        UIButton::ButtonType::Pushable,                                          // A típus Pushable
        [this, updatePageAndUI](const UIButton::ButtonEvent &event) {            // A gomb esemény callbackje
            if (event.state == UIButton::EventButtonState::Clicked && currentPage > 0) {
                currentPage--;
                updatePageAndUI();
            }
        },
        UIColorPalette::createDefaultButtonScheme() // A gomb színsémája
    );
    prevButton->setUseMiniFont(true);

    // Next gomb létrehozása (jobb oldal)
    int16_t nextButtonX = bounds.x + bounds.width - SIDE_SPACING - COMPACT_BUTTON_WIDTH;
    nextButton = std::make_shared<UIButton>(                                     //
        201,                                                                     // A gomb ID-je 201
        Rect(nextButtonX, buttonY, COMPACT_BUTTON_WIDTH, COMPACT_BUTTON_HEIGHT), // A gomb területe
        "Next >",                                                                // A gomb szövege
        UIButton::ButtonType::Pushable,                                          // A típus Pushable
        [this, updatePageAndUI](const UIButton::ButtonEvent &event) {            // A gomb esemény callbackje
            if (event.state == UIButton::EventButtonState::Clicked && currentPage < TOTAL_PAGES - 1) {
                currentPage++;
                updatePageAndUI();
            }
        },
        UIColorPalette::createDefaultButtonScheme() // A gomb színsémája
    );
    nextButton->setUseMiniFont(true);

    // Navigációs gombok hozzáadása a dialógushoz
    addChild(prevButton);
    addChild(nextButton);

    // Az OK gomb konfigurálása - sokkal egyszerűbb módszer a helper metódussal
    auto okButton = MessageDialog::getOkButton();
    if (okButton) {

        // A gomb mini fontot használjon
        okButton->setUseMiniFont(true);
        // A gombok Y pozíciójának beállítása, hogy a gombok azonos vonalban legyenek
        Rect currentBounds = okButton->getBounds();
        okButton->setBounds(Rect(currentBounds.x, buttonY, currentBounds.width, COMPACT_BUTTON_HEIGHT));
    }

    // Navigációs gombok kezdeti állapotának beállítása
    updateNavigationButtons();
}

/**
 * @brief Navigációs gombok állapotának frissítése
 * @details Az aktuális oldal alapján engedélyezi vagy letiltja a Previous és Next gombokat.
 * - Previous gomb: csak akkor aktív, ha nem az első oldalon (currentPage > 0) vagyunk
 * - Next gomb: csak akkor aktív, ha nem az utolsó oldalon (currentPage < TOTAL_PAGES-1) vagyunk
 *
 * Ez a metódus minden oldal váltás után meghívódik, hogy biztosítsa a gombok
 * megfelelő állapotát a felhasználói interakció szempontjából.
 */
void SystemInfoDialog::updateNavigationButtons() {

    // Previous gomb aktiválása/deaktiválása - csak akkor engedélyezett ha nem az első oldalon vagyunk
    if (prevButton) {
        bool prevEnabled = (currentPage > 0);
        prevButton->setEnabled(prevEnabled);
    }

    // Next gomb aktiválása/deaktiválása - csak akkor engedélyezett ha nem az utolsó oldalon vagyunk
    if (nextButton) {
        bool nextEnabled = (currentPage < TOTAL_PAGES - 1);
        nextButton->setEnabled(nextEnabled);
    }
}

/**
 * @brief A dialógus teljes újrarajzolása
 * @details Ez a metódus felelős a dialógus teljes vizuális megjelenítéséért:
 * 1. Alapértelmezett dialógus háttér és fejléc rajzolása (UIDialogBase::drawSelf())
 * 2. Aktuális oldal tartalmának lekérése és megjelenítése
 * 3. Oldalszám kiírása a fejlécbe (Page X/Y formátumban)
 * 4. Szöveges tartalom soronkénti kirajzolása megfelelő formázással
 * 5. Összes gomb újrarajzolása (X, OK, Previous, Next)
 *
 * A metódus gondoskodik a szöveg tördeléséről, megfelelő pozicionálásról
 * és a különböző UI elemek helyes megjelenítéséről.
 */
void SystemInfoDialog::drawSelf() {
    // Előbb rajzoljuk az alapértelmezett dialógus hátteret és fejlécet (de nem a message-t)
    UIDialogBase::drawSelf();

    // Aktuális oldal tartalmának lekérése
    String infoContent = getCurrentPageContent();

    // Oldalszám megjelenítése a fejlécben - megfelelő pozícióban és háttérrel
    String pageInfo = "(Page " + String(currentPage + 1) + "/" + String(TOTAL_PAGES) + ")";

    // Oldalszám kirajzolása a fejlécbe, megfelelő színekkel és pozícióval
    tft.setTextColor(UIColorPalette::DIALOG_HEADER_TEXT, UIColorPalette::DIALOG_HEADER_BACKGROUND);
    tft.setFreeFont(); // Kisebb alapértelmezett font a lapoinfo számára
    tft.setTextSize(1);
    tft.setTextDatum(MR_DATUM); // Middle-Right pozicionálás

    // Pozíció: jobb oldal, fejléc közepén, X gomb előtt
    int16_t pageInfoX = bounds.x + bounds.width - 30;       // X gomb előtt 30 pixel
    int16_t pageInfoY = bounds.y + (getHeaderHeight() / 2); // Fejléc közepén, mint a cím
    tft.drawString(pageInfo.c_str(), pageInfoX, pageInfoY); // Szöveg tulajdonságok visszaállítása a tartalomhoz
    tft.setTextColor(colors.foreground, colors.background);
    tft.setTextDatum(TL_DATUM); // Top-Left pozicionálás
    tft.setTextSize(1);
    tft.setFreeFont(); // Alapértelmezett kisebb font a jobb olvashatósághoz

    // Szöveges terület számítása (fejléc és gomb terület nélkül)
    const int16_t margin = 8;
    const int16_t buttonHeight = UIButton::DEFAULT_BUTTON_HEIGHT;
    const int16_t textStartX = bounds.x + margin;
    const int16_t textStartY = bounds.y + getHeaderHeight() + margin;
    const int16_t textWidth = bounds.width - (2 * margin);
    const int16_t textHeight = bounds.height - getHeaderHeight() - buttonHeight - (3 * margin);

    // Tartalom soronkénti kirajzolása megfelelő formázással és tördeléssel
    int16_t currentY = textStartY;
    const int16_t lineHeight = tft.fontHeight() + 2; // Kis extra hely a sorok között

    // Szöveges tartalom soronkénti feldolgozása és kirajzolása
    String remainingText = infoContent;
    int lineStart = 0;

    // Ciklus a szöveg soronkénti kirajzolásához a rendelkezésre álló területen belül
    while (lineStart < remainingText.length() && currentY < (textStartY + textHeight - lineHeight)) {

        // Következő sortörés keresése
        int lineEnd = remainingText.indexOf('\n', lineStart);
        if (lineEnd == -1) {
            lineEnd = remainingText.length(); // Ha nincs több sortörés, a szöveg végéig
        }

        String line = remainingText.substring(lineStart, lineEnd);

        // Hosszú sorok automatikus csonkítása, ha nem férnek el a területen
        int16_t textWidth_px = tft.textWidth(line.c_str());
        if (textWidth_px > textWidth) {
            // Fokozatos csonkítás "..." hozzáadásával, amíg elfér a területen
            while (line.length() > 3 && tft.textWidth(line.c_str()) > textWidth - 20) {
                line = line.substring(0, line.length() - 1);
            }
            line += "...";
        }

        // Sor kirajzolása a számított pozícióra
        tft.drawString(line.c_str(), textStartX, currentY);
        currentY += lineHeight;
        lineStart = lineEnd + 1; // Következő sor kezdésének beállítása
    }

    // FONTOS: Összes gomb újrarajzolása, mert a UIDialogBase::drawSelf() felülírja őket

    // UIDialogBase bezárás gomb (closeButton) újrarajzolása
    if (closeButton) {
        closeButton->draw();
    }

    // Összes dialógus gomb újrarajzolása (OK + navigációs gombok)
    const auto &buttonsList = getButtonsList();
    for (const auto &button : buttonsList) {
        if (button) {
            button->draw();
        }
    }
}
