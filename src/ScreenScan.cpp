/**
 * @file ScreenScan.cpp
 * @brief Spektrum analizátor scan képernyő implementáció
 *
 * Ez az osztály a spektrum analizátor fő funkcionalitását valósítja meg:
 * - Frekvencia spektrum vizualizáció
 * - Dinamikus zoom és pan funkcionalitás
 * - Érintőképernyős és rotary encoder navigáció
 * - RSSI és SNR mérések megjelenítése
 * - Állomás észlelés és jelölés
 * - Intelligens adatmegőrzés zoom műveletekkor
 */

#include "ScreenScan.h"
#include "ScreenManager.h"
#include "defines.h"
#include "rtVars.h"

// Az állomás detektálásához szükséges SNR (jel-zaj viszony) küszöbértéket.
// Ez azt jelenti, hogy csak azoknál a mérési pontoknál lesz állomásnak jelölve a frekvencia, ahol az SNR érték legalább ennyi.
// Ez a küszöbérték határozza meg, hogy mennyire legyen érzékeny az állomáskeresés.
constexpr uint8_t MIN_STATION_SNR_VALUE = 8;

// ===================================================================
// Konstruktor és inicializálás
// ===================================================================

/**
 * @brief ScreenScan konstruktor
 * @param tft TFT kijelző referencia
 * @param si4735Manager Si4735 rádió chip kezelő
 *
 * Inicializálja az összes scan paraméter alapértelmezett értékével,
 * létrehozza a scan adattömböket és beállítja a UI komponenseket.
 */
ScreenScan::ScreenScan() : UIScreen(SCREEN_NAME_SCAN) { // Scan állapot inicializálása
    scanState = ScanState::Idle;
    scanMode = ScanMode::Spectrum;
    scanPaused = true;
    lastScanTime = 0;

    // Frekvencia beállítások inicializálása
    currentScanFreq = 0;
    scanStartFreq = 0;
    scanEndFreq = 0;
    scanStep = 1.0f;
    zoomLevel = 1.0f;
    currentScanPos = 0;
    zoomGeneration = 0; // Zoom interpoláció generáció számláló

    // Sáv határok inicializálása
    scanBeginBand = -1;
    scanEndBand = SCAN_RESOLUTION;

    // Az állomás detektálásához szükséges SNR (jel-zaj viszony) küszöbértéket.
    // Ez azt jelenti, hogy csak azoknál a mérési pontoknál lesz állomásnak jelölve a frekvencia, ahol az SNR érték legalább ennyi.
    // Ez a küszöbérték határozza meg, hogy mennyire legyen érzékeny az állomáskeresés.
    scanMarkSNR = MIN_STATION_SNR_VALUE;

    scanEmpty = true;

    // Mérési konfiguráció
    countScanSignal = 3;
    signalScale = 2.0f;

    // UI cache inicializálása
    lastStatusText = "";

    // Scan adattömbök inicializálása - minden érték üres állapotra
    for (int i = 0; i < SCAN_RESOLUTION; i++) {
        scanValueRSSI[i] = SCAN_AREA_Y + SCAN_AREA_HEIGHT; // Spektrum alján (nincs jel)
        scanValueSNR[i] = 0;
        scanMark[i] = false;
        scanScaleLine[i] = 0;
        scanDataValid[i] = false; // Nincs érvényes adat inicializáláskor
    }

    // UI komponensek létrehozása
    layoutComponents();
}

// ===================================================================
// UIScreen interface implementáció
// ===================================================================

/**
 * @brief Képernyő aktiválása
 *
 * Meghívódik amikor ez a képernyő aktívvá válik.
 * Inicializálja a scan paramétereket és visszaállítja az alapállapotot,
 * de megőrzi a meglévő scan adatokat ha vannak (pl. screensaver után).
 */
void ScreenScan::activate() {
    UIScreen::activate();
    initializeScan();
    calculateScanParameters();

    // Csak akkor reseteljük a scant, ha még nincs érvényes adat
    // Ez megőrzi a scan állapotot screensaver után
    if (scanEmpty) {
        resetScan();
    }
}

/**
 * @brief Képernyő deaktiválása
 *
 * Meghívódik amikor elhagyjuk ezt a képernyőt.
 * Leállítja a scant és felszabadítja az erőforrásokat.
 */
void ScreenScan::deactivate() {
    stopScan();
    UIScreen::deactivate();
}

/**
 * @brief UI komponensek elrendezésének beállítása
 *
 * Létrehozza a vízszintes gombsort a képernyő alján.
 */
void ScreenScan::layoutComponents() {
    // Vízszintes gombsor létrehozása
    createHorizontalButtonBar();
}

/**
 * @brief Vízszintes gombsor létrehozása
 *
 * Létrehozza a Start/Pause, Zoom+, Zoom-, Reset és Back gombokat
 * a képernyő alján megfelelő eseménykezelőkkel.
 */
void ScreenScan::createHorizontalButtonBar() {
    constexpr int16_t margin = 5;
    uint16_t buttonHeight = UIButton::DEFAULT_BUTTON_HEIGHT;
    uint16_t buttonY = ::SCREEN_H - UIButton::DEFAULT_BUTTON_HEIGHT - margin;
    uint16_t buttonWidth = 70;
    uint16_t buttonSpacing = 5;

    // Start/Pause gomb - scan indítása/megállítása
    uint16_t playPauseX = margin;
    Rect playPauseRect(playPauseX, buttonY, buttonWidth, buttonHeight);
    playPauseButton = std::make_shared<UIButton>( //
        PLAY_PAUSE_BUTTON_ID,                     //
        playPauseRect,                            //
        "Start",                                  //
        UIButton::ButtonType::Pushable,           //
        UIButton::ButtonState::Off,               //
        [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                if (scanPaused) {
                    startScan();
                } else {
                    pauseScan();
                }
            }
        });
    addChild(playPauseButton);

    // Zoom In gomb - spektrum nagyítása
    uint16_t zoomInX = playPauseX + buttonWidth + buttonSpacing;
    Rect zoomInRect(zoomInX, buttonY, buttonWidth, buttonHeight);
    zoomInButton = std::make_shared<UIButton>( //
        ZOOM_IN_BUTTON_ID,                     //
        zoomInRect,                            //
        "Zoom+",                               //
        UIButton::ButtonType::Pushable,        //
        UIButton::ButtonState::Off,            //
        [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                zoomIn();
            }
        });
    addChild(zoomInButton);

    // Zoom Out gomb - spektrum kicsinyítése
    uint16_t zoomOutX = zoomInX + buttonWidth + buttonSpacing;
    Rect zoomOutRect(zoomOutX, buttonY, buttonWidth, buttonHeight);
    zoomOutButton = std::make_shared<UIButton>( //
        ZOOM_OUT_BUTTON_ID,                     //
        zoomOutRect,                            //
        "Zoom-",                                //
        UIButton::ButtonType::Pushable,         //
        UIButton::ButtonState::Off,             //
        [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                zoomOut();
            }
        });
    addChild(zoomOutButton);

    // Reset gomb - teljes visszaállítás
    uint16_t resetX = zoomOutX + buttonWidth + buttonSpacing;
    Rect resetRect(resetX, buttonY, buttonWidth, buttonHeight);
    resetButton = std::make_shared<UIButton>( //
        RESET_BUTTON_ID,                      //
        resetRect,                            //
        "Reset",                              //
        UIButton::ButtonType::Pushable,       //
        UIButton::ButtonState::Off,           //
        [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                resetScan();
            }
        });
    addChild(resetButton);

    // Back gomb - visszalépés a főmenübe (jobbra igazítva)
    uint16_t backButtonWidth = 60;
    uint16_t backButtonX = ::SCREEN_W - backButtonWidth - margin;
    Rect backButtonRect(backButtonX, buttonY, backButtonWidth, buttonHeight);
    backButton = std::make_shared<UIButton>( //
        BACK_BUTTON_ID,                      //
        backButtonRect,                      //
        "Back",                              //
        UIButton::ButtonType::Pushable,      //
        UIButton::ButtonState::Off,          //
        [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                if (getScreenManager()) {
                    getScreenManager()->goBack();
                }
            }
        });
    addChild(backButton);
}

/**
 * @brief Teljes képernyő tartalmának kirajzolása
 *
 * Kirajzolja a spektrum analizátor teljes felületét:
 * - Cím és spektrum terület kerete
 * - Spektrum adatok és skála vonalak
 * - Frekvencia címkék és sáv határok
 * - Információs panel statikus és dinamikus elemei
 */
void ScreenScan::drawContent() {
    tft.fillScreen(TFT_BLACK);

    // Cím rajzolása
    tft.setFreeFont();
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("SPECTRUM ANALYZER", tft.width() / 2, 10);

    // Spektrum terület kerete
    tft.drawRect(SCAN_AREA_X - 1, SCAN_AREA_Y - 1, SCAN_AREA_WIDTH + 2, SCAN_AREA_HEIGHT + 2, TFT_WHITE);

    // Spektrum rajzolása
    drawSpectrum();

    // Skála rajzolása
    drawScale();

    // Frekvencia címkék
    drawFrequencyLabels();

    // Sáv határok jelölése
    drawBandBoundaries();

    // Statikus információs címkék egyszeri kirajzolása
    drawScanInfoStatic();

    // Változó információk kirajzolása
    drawScanInfo();
}

/**
 * @brief Főciklus kezelése
 *
 * 50ms-enként frissíti a scant ha aktív.
 * Ez biztosítja a folyamatos spektrum pásztázást.
 */
void ScreenScan::handleOwnLoop() {
    if (scanState == ScanState::Scanning && !scanPaused) {
        uint32_t currentTime = millis();
        // 50ms késleltetés a mérések között
        if (currentTime - lastScanTime > 50) {
            updateScan();
            lastScanTime = currentTime;
        }
    }
}

/**
 * @brief Érintési események kezelése
 * @param event Érintési esemény adatai
 * @return true ha az eseményt kezeltük, false egyébként
 *
 * Prioritási sorrend:
 * 1. Spektrum terület érintése - kurzor mozgatás
 * 2. Gombok kezelése a szülő osztály által
 *
 * A spektrum területen való érintés:
 * - Automatikusan pauzálja a scant ha fut
 * - Csak érvényes adatpontokra mozgatja a kurzort
 * - Frissíti a frekvenciát és a kijelzést
 */
bool ScreenScan::handleTouch(const TouchEvent &event) {
    // Spektrum terület érintésének ellenőrzése (prioritás!)
    if (event.pressed && event.x >= SCAN_AREA_X && event.x < SCAN_AREA_X + SCAN_AREA_WIDTH && event.y >= SCAN_AREA_Y && event.y < SCAN_AREA_Y + SCAN_AREA_HEIGHT) {

        // Relatív pozíció számítás a spektrum területen belül
        uint16_t relativePixelX = event.x - SCAN_AREA_X;

        // Pozíció korlátozása a spektrum szélességen belül
        if (relativePixelX >= SCAN_AREA_WIDTH) {
            relativePixelX = SCAN_AREA_WIDTH - 1;
        }

        // Pixel pozíciót adatponttá konvertálás
        uint16_t relativeDataPos = (relativePixelX * SCAN_RESOLUTION) / SCAN_AREA_WIDTH;

        // Ha más pozíció, mint a jelenlegi kurzor
        if (relativeDataPos != currentScanPos) {
            // Ellenőrizzük, hogy a célpozícióban van-e érvényes mért adat
            if (!scanEmpty && !isDataValid(relativeDataPos)) {
                return true; // Eseményt kezeltük, de nem mozgattunk (nincs adat)
            }

            // Ha scan fut, automatikusan pause-oljuk
            if (!scanPaused) {
                pauseScan();
            }

            // Régi pozíció pixel koordinátája
            uint16_t oldPixelPos = (currentScanPos * SCAN_AREA_WIDTH) / SCAN_RESOLUTION;

            // Új kurzor pozíció beállítása (adatpont koordinátában)
            currentScanPos = relativeDataPos;

            // Új pixel pozíció számítása
            uint16_t newPixelPos = (currentScanPos * SCAN_AREA_WIDTH) / SCAN_RESOLUTION;

            // Új pozíció frekvenciájának beállítása
            uint32_t newFreq = positionToFreq(currentScanPos);
            setFrequency(newFreq);

            // Spektrum újrarajzolása a kurzor változás miatt
            if (oldPixelPos != newPixelPos) {
                drawSpectrumLine(oldPixelPos); // Régi kurzor törlése
                drawSpectrumLine(newPixelPos); // Új kurzor rajzolása
            } else {
                // Ugyanazon a pixelen belül vagyunk, csak újrarajzoljuk
                drawSpectrumLine(newPixelPos);
            }

            // Információs panel frissítése
            drawScanInfo();
        }

        return true; // Spektrum területen történt érintés - NE adja tovább a gomboknak!
    }

    // Ha nem a spektrum területen történt, adjuk át a szülő osztálynak (gombok kezelése)
    if (UIScreen::handleTouch(event)) {
        return true;
    }

    return false;
}

/**
 * @brief Rotary encoder események kezelése
 * @param event Rotary encoder esemény adatai
 * @return true ha az eseményt kezeltük, false egyébként
 *
 * Rotary encoder működése:
 * - Ha van scan adat: kurzor mozgatás az érvényes adatpontok között
 * - Ha nincs scan adat: frekvencia léptetés
 * - Scan közben nincs navigáció (biztonsági védelem)
 * - Klikk: zoom in (ugyanaz mint a Zoom+ gomb)
 *
 * Irányok:
 * - Up (jobbra): következő pozíció/magasabb frekvencia
 * - Down (balra): előző pozíció/alacsonyabb frekvencia
 * - Click: zoom nagyítás
 */
bool ScreenScan::handleRotary(const RotaryEvent &event) {
    // Rotary encoder klikk kezelése - zoom in funkció
    if (event.buttonState == RotaryEvent::ButtonState::Clicked) {
        zoomIn(); // Ugyanaz a funkció mint a Zoom+ gomb
        return true;
    }

    if (event.direction == RotaryEvent::Direction::Up) {
        // Jobbra forgatás - előre lépés
        if (scanPaused) {
            if (!scanEmpty) {
                // Van scan adat - kurzor mozgatás a következő érvényes pozícióra
                uint16_t newPos = currentScanPos + 1;
                if (newPos < SCAN_RESOLUTION) {
                    // Ellenőrizzük, hogy az új pozícióban van-e érvényes mért adat
                    if (!isDataValid(newPos)) {
                        return true; // Eseményt kezeltük, de nem mozgattunk (nincs adat)
                    }

                    // Régi pozíció pixel koordinátája
                    uint16_t oldPixelPos = (currentScanPos * SCAN_AREA_WIDTH) / SCAN_RESOLUTION;

                    // Új kurzor pozíció beállítása
                    currentScanPos = newPos;

                    // Új pixel pozíció számítása
                    uint16_t newPixelPos = (currentScanPos * SCAN_AREA_WIDTH) / SCAN_RESOLUTION;

                    // Új pozíció frekvenciájának beállítása
                    uint32_t newFreq = positionToFreq(currentScanPos);
                    setFrequency(newFreq);

                    // Spektrum újrarajzolása a kurzor változás miatt
                    if (oldPixelPos != newPixelPos) {
                        drawSpectrumLine(oldPixelPos); // Régi kurzor törlése
                        drawSpectrumLine(newPixelPos); // Új kurzor rajzolása
                    } else {
                        // Ugyanazon a pixelen belül vagyunk, csak újrarajzoljuk
                        drawSpectrumLine(newPixelPos);
                    }

                    // Információs panel frissítése
                    drawScanInfo();
                }
            } else {
                // Nincs scan adat - frekvencia léptetés
                uint32_t newFreq = currentScanFreq + (uint32_t)(scanStep * 10);
                if (newFreq <= scanEndFreq) {
                    currentScanFreq = newFreq;
                    setFrequency(currentScanFreq);
                    drawScanInfo();
                }
            }
        }
        return true;
    } else if (event.direction == RotaryEvent::Direction::Down) {
        // Balra forgatás - vissza lépés
        if (scanPaused) {
            if (!scanEmpty) {
                // Van scan adat - kurzor mozgatás az előző érvényes pozícióra
                if (currentScanPos > 0) {
                    uint16_t newPos = currentScanPos - 1;

                    // Ellenőrizzük, hogy az új pozícióban van-e érvényes mért adat
                    if (!isDataValid(newPos)) {
                        return true; // Eseményt kezeltük, de nem mozgattunk (nincs adat)
                    }

                    // Régi pozíció pixel koordinátája
                    uint16_t oldPixelPos = (currentScanPos * SCAN_AREA_WIDTH) / SCAN_RESOLUTION;

                    // Új kurzor pozíció beállítása
                    currentScanPos = newPos;

                    // Új pixel pozíció számítása
                    uint16_t newPixelPos = (currentScanPos * SCAN_AREA_WIDTH) / SCAN_RESOLUTION;

                    // Új pozíció frekvenciájának beállítása
                    uint32_t newFreq = positionToFreq(currentScanPos);
                    setFrequency(newFreq);

                    // Spektrum újrarajzolása a kurzor változás miatt
                    if (oldPixelPos != newPixelPos) {
                        drawSpectrumLine(oldPixelPos); // Régi kurzor törlése
                        drawSpectrumLine(newPixelPos); // Új kurzor rajzolása
                    } else {
                        // Ugyanazon a pixelen belül vagyunk, csak újrarajzoljuk
                        drawSpectrumLine(newPixelPos);
                    }

                    // Információs panel frissítése
                    drawScanInfo();
                }
            } else {
                // Nincs scan adat - frekvencia léptetés
                uint32_t newFreq = currentScanFreq - (uint32_t)(scanStep * 10);
                if (newFreq >= scanStartFreq) {
                    currentScanFreq = newFreq;
                    setFrequency(currentScanFreq);
                    drawScanInfo();
                }
            }
        }
        return true;
    }
    return false;
}

// ===================================================================
// Scan inicializálás és vezérlés
// ===================================================================

/**
 * @brief Scan inicializálása
 *
 * Lekéri az aktuális sáv paramétereit a Si4735Manager-ből
 * és beállítja a scan frekvencia tartományát.
 */
void ScreenScan::initializeScan() {
    if (!pSi4735Manager)
        return;

    // Aktuális sáv információk lekérése a Si4735Manager-ből
    BandTable &currentBand = pSi4735Manager->getCurrentBand();

    scanStartFreq = currentBand.minimumFreq * 10; // kHz-ben
    scanEndFreq = currentBand.maximumFreq * 10;   // kHz-ben
    currentScanFreq = scanStartFreq;
}

/**
 * @brief Scan paraméterek számítása
 *
 * Kiszámítja a scan lépésközt az aktuális frekvencia tartomány
 * és a felbontás alapján.
 */
void ScreenScan::calculateScanParameters() {
    // Scan lépésköz számítása az aktuális tartomány alapján
    uint32_t totalBandwidth = scanEndFreq - scanStartFreq;
    scanStep = (float)totalBandwidth / SCAN_RESOLUTION;

    // Minimális lépésköz korlátozása
    if (scanStep < 1.0f)
        scanStep = 1.0f;
}

/**
 * @brief Scan teljes visszaállítása
 *
 * Visszaállítja a scan-t az alapállapotra:
 * - Leállítja a pásztázást
 * - Törli az összes mért adatot
 * - Visszaállítja a zoom szintet 1.0x-ra
 * - Visszaállítja a teljes sáv tartományt
 */
void ScreenScan::resetScan() {
    // Scan állapot teljes visszaállítása
    scanState = ScanState::Idle;
    scanPaused = true;
    scanEmpty = true;
    currentScanPos = 0;
    zoomLevel = 1.0f;   // Zoom visszaállítása 1.0x-ra
    zoomGeneration = 0; // Zoom generáció nullázása

    // Teljes sáv tartomány visszaállítása
    if (pSi4735Manager) {
        BandTable &currentBand = pSi4735Manager->getCurrentBand();
        scanStartFreq = currentBand.minimumFreq * 10; // Teljes sáv kezdete
        scanEndFreq = currentBand.maximumFreq * 10;   // Teljes sáv vége
        currentScanFreq = scanStartFreq;
    }

    // UI cache visszaállítása
    lastStatusText = "";

    // Scan paraméterek újraszámítása
    calculateScanParameters();

    // Összes mért adat törlése
    for (int i = 0; i < SCAN_RESOLUTION; i++) {
        scanValueRSSI[i] = SCAN_AREA_Y + SCAN_AREA_HEIGHT; // Spektrum alján (nincs jel)
        scanValueSNR[i] = 0;
        scanMark[i] = false;
        scanScaleLine[i] = 0;
        scanDataValid[i] = false; // Nincs érvényes adat
    }

    // Sáv határok újraszámítása
    scanBeginBand = -1;
    scanEndBand = SCAN_RESOLUTION;

    // Gomb állapot frissítése
    if (playPauseButton) {
        playPauseButton->setLabel("Start"); // Reset után Start gomb
    }

    // Hang visszakapcsolása reset után
    if (pSi4735Manager) {
        pSi4735Manager->getSi4735().setAudioMute(false);
    }

    // Megjegyzés: NE rajzoljunk itt semmit - az UI rendszer automatikusan
    // meghívja a drawContent()-et ami teljes képernyőt rajzol
}

/**
 * @brief Scan indítása
 *
 * Elindítja a spektrum pásztázást:
 * - Beállítja a scan állapotot aktívra
 * - Elnémítja a hangot a gyors frekvencia váltások miatt
 * - Frissíti a gomb feliratát "Pause"-ra
 * - Azonnal frissíti a státusz kijelzést
 */
void ScreenScan::startScan() {
    scanPaused = false;
    scanState = ScanState::Scanning;
    lastScanTime = millis();

    // Audio némítás a scan közben (gyors frekvencia váltások miatt)
    if (pSi4735Manager) {
        pSi4735Manager->getSi4735().setAudioMute(true);
    }
    if (playPauseButton) {
        playPauseButton->setLabel("Pause"); // Scan közben Pause gomb
    }

    // Státusz frissítése a képernyőn
    drawScanInfo();
}

/**
 * @brief Scan szüneteltetése
 *
 * Szünetelteti a spektrum pásztázást:
 * - Megállítja a méréseket
 * - Visszakapcsolja a hangot az aktuális frekvencián
 * - Frissíti a gomb feliratát "Start"-ra
 * - Azonnal frissíti a státusz kijelzést
 */
void ScreenScan::pauseScan() {
    scanPaused = true;

    // Hang visszakapcsolása pause módban, hogy hallhassuk az aktuális frekvenciát
    if (pSi4735Manager) {
        pSi4735Manager->getSi4735().setAudioMute(false);
    }
    if (playPauseButton) {
        playPauseButton->setLabel("Start"); // Pause után Start gomb
    }

    // Státusz frissítése a képernyőn
    drawScanInfo();
}

/**
 * @brief Scan leállítása
 *
 * Teljesen leállítja a spektrum pásztázást és idle állapotba kapcsol.
 * Ez általában a képernyő elhagyásakor hívódik meg.
 */
void ScreenScan::stopScan() {
    scanState = ScanState::Idle;
    scanPaused = true;
    if (playPauseButton) {
        playPauseButton->setLabel("Start"); // Stop után Start gomb
    }

    // Státusz frissítése a képernyőn
    drawScanInfo();
}

/**
 * @brief Scan frissítése (egy lépés)
 *
 * Minden meghíváskor egy frekvencia pontot mér le:
 * - Beállítja a frekvenciát
 * - Mér RSSI és SNR értékeket
 * - Értékeli az állomás jelenlétét
 * - Frissíti a spektrum megjelenítést
 * - Léptet a következő pozícióra
 */
void ScreenScan::updateScan() {
    if (!pSi4735Manager || scanPaused || currentScanPos >= SCAN_RESOLUTION) {
        return;
    }

    // Aktuális frekvencia számítása
    uint32_t scanFreq = positionToFreq(currentScanPos);
    setFrequency(scanFreq);

    // Jel mérése - optimalizált: egyszerre RSSI és SNR
    int16_t rssiY;
    uint8_t snr;
    getSignalQuality(rssiY, snr);
    scanValueRSSI[currentScanPos] = rssiY;
    scanValueSNR[currentScanPos] = snr;
    scanDataValid[currentScanPos] = true; // Megjelöljük, hogy érvényes adat van itt

    // Állomás jelzés SNR alapján - minden méréskor újra értékeljük
    if (scanValueSNR[currentScanPos] >= scanMarkSNR && currentScanPos > scanBeginBand && currentScanPos < scanEndBand) {
        scanMark[currentScanPos] = true;
    } else {
        scanMark[currentScanPos] = false; // Töröljük a régi jelzést ha már nincs elég jel
    }

    // Spektrum vonal rajzolása (pixel pozícióra konvertálás)
    uint16_t pixelPos = (currentScanPos * SCAN_AREA_WIDTH) / SCAN_RESOLUTION;
    drawSpectrumLine(pixelPos);

    // Előző pozíció kurzorának törlése (ha volt)
    if (currentScanPos > 0) {
        uint16_t prevPixelPos = ((currentScanPos - 1) * SCAN_AREA_WIDTH) / SCAN_RESOLUTION;
        if (prevPixelPos != pixelPos) {
            // Újrarajzoljuk az előző pozíciót kurzor nélkül
            drawSpectrumLine(prevPixelPos);
        }
    }

    // Pozíció léptetése
    currentScanPos++;

    // Ha végére értünk, újrakezdés
    if (currentScanPos >= SCAN_RESOLUTION) {
        currentScanPos = 0;
        scanEmpty = false;
    }

    // Információk frissítése
    drawScanInfo();
}

// ===================================================================
// Rajzolási funkciók
// ===================================================================

/**
 * @brief Teljes spektrum kirajzolása
 *
 * Kirajzolja az összes spektrum vonalat pixel-enkénti felbontásban.
 * Minden pixel reprezentálja egy vagy több mérési pontot.
 */
void ScreenScan::drawSpectrum() {
    // Spektrum terület törlése
    tft.fillRect(SCAN_AREA_X, SCAN_AREA_Y, SCAN_AREA_WIDTH, SCAN_AREA_HEIGHT, TFT_COLOR_BACKGROUND);

    // Minden pixel spektrum vonalának kirajzolása
    for (int x = 0; x < SCAN_AREA_WIDTH; x++) {
        drawSpectrumLine(x);
    }
}

/**
 * @brief Egy spektrum vonal kirajzolása
 * @param pixelX A vízszintes pixel pozíció (0-SCAN_AREA_WIDTH)
 *
 * Ez a függvény felelős egy vertikális spektrum vonal kirajzolásáért.
 * Több mérési pontot átlagol egy pixelre és különböző színekkel jeleníti meg:
 * - SNR alapú színkódolás (kék->narancs->piros)
 * - Skála vonalak (oliva háttér)
 * - Állomás jelzők (zöld pontok)
 * - Kurzor pozíció (piros vonal)
 */
void ScreenScan::drawSpectrumLine(uint16_t pixelX) {
    if (pixelX >= SCAN_AREA_WIDTH)
        return;

    uint16_t screenX = SCAN_AREA_X + pixelX;

    // Mérési pontok tartományának meghatározása ehhez a pixelhez
    uint16_t dataStart = (pixelX * SCAN_RESOLUTION) / SCAN_AREA_WIDTH;
    uint16_t dataEnd = ((pixelX + 1) * SCAN_RESOLUTION) / SCAN_AREA_WIDTH;

    // Átlagolási változók inicializálása
    int16_t avgRSSI = 0;
    uint8_t avgSNR = 0;
    bool hasStation = false;
    bool isMainScale = false;
    uint32_t avgFreq = 0;

    // Mérési pontok átlagolása ebben a pixelben
    int validSamples = 0;
    for (uint16_t i = dataStart; i < dataEnd && i < SCAN_RESOLUTION; i++) {
        // Csak valós mért adatokat vesszük figyelembe
        if (scanValueRSSI[i] < SCAN_AREA_Y + SCAN_AREA_HEIGHT) {
            avgRSSI += scanValueRSSI[i];
            avgSNR += scanValueSNR[i];
            validSamples++;
        }

        // Állomás jelzés ellenőrzése
        if (scanMark[i]) {
            hasStation = true;
        }

        // Frekvencia számítás az adatponthoz
        uint32_t freq = scanStartFreq + (uint32_t)(i * scanStep);
        avgFreq += freq; // Skála vonalak ellenőrzése - dinamikus ritkítás zoom szint alapján
        // MINDIG ellenőrizzük a skála vonalakat, függetlenül a mérési adatoktól
        if (scanScaleLine[i] == 0) {
            if (zoomLevel < 1.4f) {
                // 1x zoom: minden 2 MHz-nél
                if ((freq % 2000) < scanStep) {
                    scanScaleLine[i] = 2;
                    isMainScale = true;
                }
            } else if (zoomLevel < 2.5f) {
                // 2x zoom: minden 1 MHz-nél
                if ((freq % 1000) < scanStep) {
                    scanScaleLine[i] = 2;
                    isMainScale = true;
                }
            } else if (zoomLevel < 4.0f) {
                // 3x zoom: minden 500 kHz-nél
                if ((freq % 500) < scanStep) {
                    scanScaleLine[i] = 2;
                    isMainScale = true;
                }
            } else if (scanStep > 50) {
                // Nagy zoom (4x+), de még nagy lépésköz: minden 200 kHz-nél
                if ((freq % 200) < scanStep) {
                    scanScaleLine[i] = 2;
                    isMainScale = true;
                }
            } else {
                // Nagyon nagy zoom: minden 100 kHz-nél
                if ((freq % 100) < scanStep) {
                    scanScaleLine[i] = 2;
                    isMainScale = true;
                }
            }
        }
        if (scanScaleLine[i] == 2)
            isMainScale = true;
    }

    // Átlagok kiszámítása
    if (validSamples > 0) {
        avgRSSI /= validSamples;
        avgSNR /= validSamples;
    } else {
        avgRSSI = SCAN_AREA_Y + SCAN_AREA_HEIGHT; // Nincs jel
        avgSNR = 0;
    }
    avgFreq /= (dataEnd - dataStart); // Színek meghatározása alapértelmezett értékekkel
    uint16_t lineColor = TFT_NAVY;
    uint16_t bgColor = TFT_BLACK;

    // Skála vonalak esetén oliva háttér
    if (isMainScale) {
        lineColor = TFT_BLACK;
        bgColor = TFT_OLIVE;
    }

    // SNR alapú színkódolás a jel erősségének megjelenítéséhez
    if (avgSNR > 0) {
        lineColor = TFT_NAVY;
        if (avgSNR < 16) {
            // Gyenge jel: kék árnyalatok
            lineColor += (avgSNR * 2048);
        } else {
            lineColor = TFT_ORANGE;
            if (avgSNR < 24) {
                // Közepes jel: narancs árnyalatok
                lineColor += ((avgSNR - 16) * 258);
            } else {
                // Erős jel: piros
                lineColor = TFT_RED;
            }
        }
    }

    // Sáv határok ellenőrzése és jelölése
    if (avgFreq > scanEndFreq || avgFreq < scanStartFreq) {
        lineColor = TFT_DARKGREY;
        if (avgFreq > scanEndFreq && scanEndBand == SCAN_RESOLUTION) {
            scanEndBand = dataStart;
        }
        if (avgFreq < scanStartFreq && scanBeginBand < (int16_t)dataStart) {
            scanBeginBand = dataStart;
        }
    }

    // RSSI alapú spektrum háttér rajzolása
    int16_t rssiY = avgRSSI;
    if (rssiY >= SCAN_AREA_Y + SCAN_AREA_HEIGHT) {
        // Nincs jel - használjuk a megfelelő háttérszínt (oliva vonalaknál TFT_OLIVE)
        tft.drawLine(screenX, SCAN_AREA_Y, screenX, SCAN_AREA_Y + SCAN_AREA_HEIGHT, bgColor);
    } else {
        // Van mért jel, normál spektrum megjelenítés
        if (rssiY < SCAN_AREA_Y + 10)
            rssiY = SCAN_AREA_Y + 10;

        tft.drawLine(screenX, rssiY + 1, screenX, SCAN_AREA_Y + SCAN_AREA_HEIGHT, lineColor);
        tft.drawLine(screenX, SCAN_AREA_Y, screenX, rssiY - 1, bgColor);
    }

    // RSSI görbe rajzolása (simított) - az előző pixellel való összekötéshez
    if (rssiY < SCAN_AREA_Y + SCAN_AREA_HEIGHT && pixelX > 0) {
        // Előző pixel átlagolt RSSI értékének számítása
        uint16_t prevDataStart = ((pixelX - 1) * SCAN_RESOLUTION) / SCAN_AREA_WIDTH;
        uint16_t prevDataEnd = (pixelX * SCAN_RESOLUTION) / SCAN_AREA_WIDTH;

        int16_t prevAvgRSSI = SCAN_AREA_Y + SCAN_AREA_HEIGHT;
        int prevValidSamples = 0;
        for (uint16_t i = prevDataStart; i < prevDataEnd && i < SCAN_RESOLUTION; i++) {
            if (scanValueRSSI[i] < SCAN_AREA_Y + SCAN_AREA_HEIGHT) {
                prevAvgRSSI += scanValueRSSI[i];
                prevValidSamples++;
            }
        }
        if (prevValidSamples > 0) {
            prevAvgRSSI /= prevValidSamples;
        }

        // Simított vonal rajzolása az előző ponttal
        if (prevAvgRSSI < SCAN_AREA_Y + SCAN_AREA_HEIGHT) {
            if (prevAvgRSSI < SCAN_AREA_Y + 10)
                prevAvgRSSI = SCAN_AREA_Y + 10;
            tft.drawLine(screenX - 1, prevAvgRSSI, screenX, rssiY, TFT_SILVER);
        } else {
            tft.drawPixel(screenX, rssiY, TFT_SILVER);
        }
    }

    // Skála vonalak rajzolása - UTOLJÁRA, hogy mindig látszanak
    if (isMainScale) {
        tft.drawLine(screenX, SCAN_AREA_Y, screenX, SCAN_AREA_Y + 15, TFT_OLIVE);
    }

    // Aktuális kurzor pozíció jelzése (piros vonal)
    uint16_t currentPixelPos = (currentScanPos * SCAN_AREA_WIDTH) / SCAN_RESOLUTION;
    if (pixelX == currentPixelPos) {
        tft.drawLine(screenX, SCAN_AREA_Y, screenX, SCAN_AREA_Y + SCAN_AREA_HEIGHT, TFT_RED);
    }

    // Állomás jelzők (zöld pontok)
    if (hasStation) {
        tft.drawPixel(screenX, SCAN_AREA_Y + 5, TFT_GREEN);
        tft.drawPixel(screenX, SCAN_AREA_Y + 10, TFT_GREEN);
    }
}

/**
 * @brief Frekvencia skála vonal rajzolása
 *
 * Kirajzolja a vízszintes skála vonalat a spektrum alatt.
 */
void ScreenScan::drawScale() {
    // Skála vonal a spektrum alatt
    tft.drawLine(SCAN_AREA_X, SCAN_AREA_Y + SCAN_AREA_HEIGHT + 5, SCAN_AREA_X + SCAN_AREA_WIDTH, SCAN_AREA_Y + SCAN_AREA_HEIGHT + 5, TFT_WHITE);
}

/**
 * @brief Frekvencia címkék rajzolása
 *
 * Kirajzolja a frekvencia értékeket a spektrum alatt 6-7 helyen.
 * A címkék intelligensen igazodnak: első balra, utolsó jobbra, közepesek középre.
 * MHz és kHz egységeket használ a frekvencia nagyságától függően.
 */
void ScreenScan::drawFrequencyLabels() {
    // Régi címkék törlése
    tft.fillRect(SCAN_AREA_X, SCAN_AREA_Y + SCAN_AREA_HEIGHT + 10, SCAN_AREA_WIDTH, 20, TFT_BLACK);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont();
    tft.setTextSize(1);

    // Frekvencia címkék megjelenítése 6 helyen
    int labelCount = 6;
    for (int i = 0; i <= labelCount; i++) {
        uint16_t x = SCAN_AREA_X + (i * SCAN_AREA_WIDTH / labelCount);

        // Adatpont pozíció használata pixel pozíció helyett (pontosabb)
        uint16_t dataPos = (i * SCAN_RESOLUTION) / labelCount;
        uint32_t freq = positionToFreq(dataPos);

        // Frekvencia szöveg formázása
        String freqText;
        if (freq >= 1000) {
            freqText = String(freq / 1000.0f, 1) + "M";
        } else {
            freqText = String(freq) + "k";
        }

        // Szöveg igazítása
        if (i == 0) {
            tft.setTextDatum(TL_DATUM); // Első: bal szélhez igazított
        } else if (i == labelCount) {
            tft.setTextDatum(TR_DATUM); // Utolsó: jobb szélhez igazított
        } else {
            tft.setTextDatum(TC_DATUM); // Közepesek: középre igazított
        }

        tft.drawString(freqText, x, SCAN_AREA_Y + SCAN_AREA_HEIGHT + 15);
    }
}

void ScreenScan::drawBandBoundaries() {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);

    constexpr uint16_t BAND_LABEL_OFFSET = 20; // felirat offset a spektrum tetejétől

    // Sáv kezdete - spektrum felső részében
    if (scanBeginBand > 0 && scanBeginBand < SCAN_AREA_WIDTH - 50) {
        uint16_t x = SCAN_AREA_X + scanBeginBand + 3;
        uint16_t y = SCAN_AREA_Y + BAND_LABEL_OFFSET; // 20px a spektrum tetejétől
        tft.fillRect(x, y, 50, 20, TFT_COLOR_BACKGROUND);
        tft.drawString("START", x, y);
        tft.drawString("OF BAND", x, y + 10);
    }

    // Sáv vége - spektrum felső részében
    if (scanEndBand < SCAN_AREA_WIDTH - 5 && scanEndBand > 50) {
        uint16_t x = SCAN_AREA_X + scanEndBand - 45;
        uint16_t y = SCAN_AREA_Y + BAND_LABEL_OFFSET; // 20px a spektrum tetejétől
        tft.fillRect(x, y, 50, 20, TFT_COLOR_BACKGROUND);
        tft.drawString("END", x, y);
        tft.drawString("OF BAND", x, y + 10);
    }
}

void ScreenScan::drawScanInfoStatic() {
    tft.setTextColor(TFT_WHITE, TFT_COLOR_BACKGROUND);
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);

    // Statikus címkék az info területen
    tft.drawString("Freq: ", 10, INFO_AREA_Y);
    tft.drawString("MHz", 100, INFO_AREA_Y); // MHz egység fix helyen

    tft.drawString("Zoom: ", 10, INFO_AREA_Y + 15);
    tft.drawString("x", 70, INFO_AREA_Y + 15); // x egység fix helyen

    tft.drawString("Status: ", 170, INFO_AREA_Y);

    // Statikus címkék a jel információkhoz
    tft.drawString("RSSI: ", 330, INFO_AREA_Y);
    tft.drawString("dBuV", 380, INFO_AREA_Y); // dBuV egység fix helyen

    tft.setTextColor(TFT_ORANGE, TFT_COLOR_BACKGROUND);
    tft.drawString("SNR: ", 330, INFO_AREA_Y + 15);
    tft.drawString("dB", 380, INFO_AREA_Y + 15); // dB egység fix helyen
}

void ScreenScan::drawScanInfo() {
    tft.setTextColor(TFT_WHITE, TFT_COLOR_BACKGROUND);
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);

    constexpr uint32_t FONT_HEIGHT = 7;

    // Csak az értékeket frissítjük (statikus címkék már ki vannak írva)
    // Aktuális frekvencia megjelenítése - csak az érték részét frissítjük
    String freqText = String(currentScanFreq / 1000.0f, 3);
    tft.fillRect(50, INFO_AREA_Y, 50, FONT_HEIGHT, TFT_COLOR_BACKGROUND); // Régi érték törlése
    tft.drawString(freqText, 50, INFO_AREA_Y);

    // Zoom szint - csak az érték részét frissítjük
    String zoomText = String(zoomLevel, 1);
    tft.fillRect(50, INFO_AREA_Y + 15, 20, FONT_HEIGHT, TFT_COLOR_BACKGROUND); // Régi érték törlése
    tft.drawString(zoomText, 50, INFO_AREA_Y + 15);

    // Scan állapot - csak akkor írjuk ki, ha változott (villogás elkerülése)
    String statusText;
    if (scanState == ScanState::Scanning && !scanPaused) {
        statusText = "Scanning...";
    } else if (scanPaused) {
        statusText = "Paused";
    } else {
        statusText = "Idle";
    }
    // Csak akkor frissítjük, ha változott
    if (statusText != lastStatusText) {
        tft.fillRect(220, INFO_AREA_Y, 110, FONT_HEIGHT, TFT_COLOR_BACKGROUND); // Régi érték törlése
        tft.drawString(statusText, 220, INFO_AREA_Y);
        lastStatusText = statusText; // Cache frissítése
    }

    // RSSI érték - csak az érték részét frissítjük
    int16_t rssi;
    uint8_t snr;
    getSignalQuality(rssi, snr);
    int16_t rssiValue = (SCAN_AREA_Y + SCAN_AREA_HEIGHT - rssi) / signalScale;
    String rssiText = String(rssiValue);
    tft.fillRect(365, INFO_AREA_Y, 15, FONT_HEIGHT, TFT_COLOR_BACKGROUND); // Régi érték törlése
    tft.drawString(rssiText, 365, INFO_AREA_Y);

    // SNR érték - csak az érték részét frissítjük
    String snrText = String(snr);
    tft.setTextColor(TFT_ORANGE, TFT_COLOR_BACKGROUND);
    tft.fillRect(365, INFO_AREA_Y + 15, 15, FONT_HEIGHT, TFT_COLOR_BACKGROUND); // Régi érték törlése
    tft.drawString(snrText, 365, INFO_AREA_Y + 15);
}

// ===================================================================
// Jel mérés és frekvencia kezelés
// ===================================================================

// Közös jel mérés - egyszerre RSSI és SNR
void ScreenScan::getSignalQuality(int16_t &rssiY, uint8_t &snr) {
    if (!pSi4735Manager) {
        rssiY = SCAN_AREA_Y + SCAN_AREA_HEIGHT - 20;
        snr = 0;
        return;
    }

    // Együttes mérés a Si4735 chipről - hatékonyabb!
    int rssiSum = 0;
    int snrSum = 0;

    for (int i = 0; i < countScanSignal; i++) {
        // Egyszerre mérjük mind az RSSI-t, mind az SNR-t
        SignalQualityData signalQuality = pSi4735Manager->getSignalQualityRealtime();
        rssiSum += signalQuality.rssi;
        snrSum += signalQuality.snr;
    }

    // RSSI átlag és Y koordinátára konvertálás
    int avgRssi = rssiSum / countScanSignal;
    rssiY = SCAN_AREA_Y + SCAN_AREA_HEIGHT - (avgRssi * signalScale * 2);

    // RSSI korlátozás
    if (rssiY < SCAN_AREA_Y + 10)
        rssiY = SCAN_AREA_Y + 10;
    if (rssiY > SCAN_AREA_Y + SCAN_AREA_HEIGHT - 10)
        rssiY = SCAN_AREA_Y + SCAN_AREA_HEIGHT - 10; // SNR átlag
    snr = snrSum / countScanSignal;
}

void ScreenScan::setFrequency(uint32_t freq) {
    currentScanFreq = freq;
    // Spektrum analizátor hangol át minden frekvenciára a méréshez!
    if (pSi4735Manager) {
        // frekvencia beállítás a Si4735 chipen
        pSi4735Manager->getSi4735().setFrequency(freq / 10); // Si4735 10kHz egységekben dolgozik

        // Kis várakozás a ráhangolódáshoz (stabilizálódás)
        delay(5); // 5ms várakozás
    }
}

// ===================================================================
// Zoom és pozíció számítások
// ===================================================================

void ScreenScan::zoomIn() {
    float newZoom;

    // Intelligens zoom szintek: 1.0 → 1.5 → 2.25 → 3.4 → 5.1
    if (zoomLevel < 1.4f) {
        newZoom = 1.5f;
    } else if (zoomLevel < 2.0f) {
        newZoom = 2.25f;
    } else if (zoomLevel < 3.0f) {
        newZoom = 3.375f;
    } else if (zoomLevel < 4.5f) {
        newZoom = 5.0625f;
    } else {

        return;
    }

    handleZoom(newZoom);
}

void ScreenScan::zoomOut() {
    float newZoom;

    // Intelligens zoom szintek visszafelé: 5.1 → 3.4 → 2.25 → 1.5 → 1.0
    if (zoomLevel > 4.5f) {
        newZoom = 3.375f;
    } else if (zoomLevel > 3.0f) {
        newZoom = 2.25f;
    } else if (zoomLevel > 2.0f) {
        newZoom = 1.5f;
    } else if (zoomLevel > 1.2f) {
        newZoom = 1.0f;
    } else {

        return;
    }

    handleZoom(newZoom);
}

void ScreenScan::handleZoom(float newZoomLevel) {
    if (!pSi4735Manager)
        return;

    // Érvényesség ellenőrzése
    if (newZoomLevel < 0.5f || newZoomLevel > 10.0f) {

        return;
    }

    // Ha már ugyanez a zoom szint van beállítva, ne csináljunk semmit
    if (abs(zoomLevel - newZoomLevel) < 0.01f) {

        return;
    }

    // Debounce védelem - ne lehessen túl gyorsan zoom-olni
    static uint32_t lastZoomTime = 0;
    uint32_t currentTime = millis();
    if (currentTime - lastZoomTime < 500) { // 500ms minimális várakozás

        return;
    }
    lastZoomTime = currentTime;

    // Ne lehessen kizoómolni a teljes sávnál jobban
    if (newZoomLevel < 1.0f) {

        return;
    }

    // Aktuális sáv információk lekérése
    BandTable &currentBand = pSi4735Manager->getCurrentBand();
    uint32_t bandStartFreq = currentBand.minimumFreq * 10; // kHz-ben
    uint32_t bandEndFreq = currentBand.maximumFreq * 10;   // kHz-ben

    // Zoom központ meghatározása (aktuális frekvencia)
    uint32_t zoomCenter = currentScanFreq;

    // Új scan tartomány számítása a zoom körül
    uint32_t totalBandwidth = bandEndFreq - bandStartFreq;
    uint32_t newScanBandwidth = (uint32_t)(totalBandwidth / newZoomLevel);

    // Új scan határok számítása a központ körül
    uint32_t halfBandwidth = newScanBandwidth / 2;
    uint32_t newScanStart = (zoomCenter > halfBandwidth) ? zoomCenter - halfBandwidth : bandStartFreq;
    uint32_t newScanEnd = (zoomCenter + halfBandwidth < bandEndFreq) ? zoomCenter + halfBandwidth : bandEndFreq;

    // Sáv határokon belül tartás
    if (newScanStart < bandStartFreq) {
        newScanStart = bandStartFreq;
        newScanEnd = newScanStart + newScanBandwidth;
        if (newScanEnd > bandEndFreq) {
            newScanEnd = bandEndFreq;
        }
    }
    if (newScanEnd > bandEndFreq) {
        newScanEnd = bandEndFreq;
        newScanStart = newScanEnd - newScanBandwidth;
        if (newScanStart < bandStartFreq) {
            newScanStart = bandStartFreq;
        }
    }

    // Minimális zoom ellenőrzés (ne legyen túl kicsi a tartomány)
    uint32_t minBandwidth = totalBandwidth / 10; // Minimum a teljes sáv 10%-a
    if (minBandwidth < 500)
        minBandwidth = 500; // De legalább 500 kHz

    if (newScanEnd - newScanStart < minBandwidth) {

        return;
    }

    // Régi értékek mentése az adatok újrafelhasználásához
    uint32_t oldScanStartFreq = scanStartFreq;
    uint32_t oldScanEndFreq = scanEndFreq;
    float oldScanStep = scanStep;

    // Értékek frissítése
    zoomLevel = newZoomLevel;
    scanStartFreq = newScanStart;
    scanEndFreq = newScanEnd;

    // Új scan paraméterek számítása
    calculateScanParameters(); // INTELLIGENS ADATMEGŐRZÉS: Ellenőrizzük, hogy a régi adatok újrafelhasználhatók-e
    bool canReuseData = false; // Csak zoom IN esetén (szűkítés) próbálunk adatokat megőrizni
    // És csak akkor, ha az új tartomány teljesen a régi tartományon belül van
    // PLUSZ: Limitáljuk az interpoláció mélységét - max 2 generáció
    if (newScanStart >= oldScanStartFreq && newScanEnd <= oldScanEndFreq && !scanEmpty && zoomGeneration < 2) {
        canReuseData = true;

    } else if (zoomGeneration >= 2) {
    }
    if (canReuseData) {

        // BIZTONSÁGOS MEGKÖZELÍTÉS: In-place adatmozgatás batch-enként
        // Először létrehozunk egy "mapping" tömböt, ami megmondja, melyik régi index melyik új indexre kerül        // Határozunk fel egy kis buffer-t (csak 10 elem)
        const int BUFFER_SIZE = 10;
        uint8_t bufferRSSI[BUFFER_SIZE];
        uint8_t bufferSNR[BUFFER_SIZE];
        bool bufferMark[BUFFER_SIZE];
        bool bufferValid[BUFFER_SIZE]; // Új buffer az érvényességi adatoknak

        // Először jelöljük meg, mely pozíciók tartalmazzanak érvényes adatokat
        // és végezzük el a másolást batch-enként

        for (int targetStart = 0; targetStart < SCAN_RESOLUTION; targetStart += BUFFER_SIZE) {
            int targetEnd = min(targetStart + BUFFER_SIZE, SCAN_RESOLUTION);

            // Buffer feltöltése az új pozíciókra szánt adatokkal
            for (int t = targetStart; t < targetEnd; t++) {
                int bufferIndex = t - targetStart;
                uint32_t targetFreq = scanStartFreq + (uint32_t)(t * scanStep); // Alapértelmezett értékek
                bufferRSSI[bufferIndex] = SCAN_AREA_Y + SCAN_AREA_HEIGHT;
                bufferSNR[bufferIndex] = 0;
                bufferMark[bufferIndex] = false;
                bufferValid[bufferIndex] = false; // Alapból nincs érvényes adat                // Keressük meg a régi pozíciót
                if (targetFreq >= oldScanStartFreq && targetFreq <= oldScanEndFreq) {
                    // Pontosabb mapping: nem csak kerekítünk, hanem ellenőrizzük a frekvencia távolságokat is
                    float oldPosFloat = (float)(targetFreq - oldScanStartFreq) / oldScanStep;
                    int oldPos = static_cast<int>(oldPosFloat + 0.5f); // Kerekítés

                    // Dupla ellenőrzés: az oldPos pozíció frekvenciája közel van-e a target frekvenciához?
                    if (oldPos >= 0 && oldPos < SCAN_RESOLUTION) {
                        uint32_t oldPosFreq = oldScanStartFreq + (uint32_t)(oldPos * oldScanStep);
                        uint32_t freqDiff = (targetFreq > oldPosFreq) ? (targetFreq - oldPosFreq) : (oldPosFreq - targetFreq);

                        // Csak akkor másoljuk, ha a frekvencia különbség kisebb mint a régi lépésköz fele
                        // Ez megakadályozza a "szellem adók" kialakulását
                        if (freqDiff <= (oldScanStep / 2)) {
                            bufferRSSI[bufferIndex] = scanValueRSSI[oldPos];
                            bufferSNR[bufferIndex] = scanValueSNR[oldPos];
                            bufferMark[bufferIndex] = scanMark[oldPos];
                            bufferValid[bufferIndex] = scanDataValid[oldPos]; // Érvényességi adat másolása                            // Csak első néhány elemhez logolunk optimalizálás céljából
                            if (t < 5) {
                            }
                        } else { // Túl nagy a frekvencia eltérés - szellem adó elkerülése
                            if (t < 5) {
                            }
                        }
                    }
                }
            } // Buffer tartalom visszamásolása
            for (int t = targetStart; t < targetEnd; t++) {
                int bufferIndex = t - targetStart;
                scanValueRSSI[t] = bufferRSSI[bufferIndex];
                scanValueSNR[t] = bufferSNR[bufferIndex];
                scanMark[t] = bufferMark[bufferIndex];
                scanDataValid[t] = bufferValid[bufferIndex]; // Érvényességi adat visszamásolása
            }
        }
        scanEmpty = false;
        zoomGeneration++; // Növeljük a generáció számot

        // FONTOS: Az adatok megmaradtak, ezért NE rajzoljuk újra a teljes spektrumot!
        // Csak a skála vonalakat és címkéket frissítjük

        // Közös inicializálás
        currentScanPos = 0;

        // Skála vonalak újraszámítása (mindig szükséges zoom után)
        for (int i = 0; i < SCAN_RESOLUTION; i++) {
            scanScaleLine[i] = 0;
        }

        // Sáv határok újraszámítása
        scanBeginBand = -1;
        scanEndBand = SCAN_RESOLUTION;

        // CSAK a meglévő adatok újrarajzolása - NE töröljük a spektrumot!
        drawSpectrum(); // Ez a meglévő adatokat rajzolja ki
        drawScale();
        drawFrequencyLabels();
        drawBandBoundaries();
        drawScanInfo();

        return; // FONTOS: Kilépünk, ne fusson le a többi rajzolás!
    }
    if (!canReuseData) {
        // Ha nem lehet újrafelhasználni, törölni kell az adatokat

        scanEmpty = true;
        for (int i = 0; i < SCAN_RESOLUTION; i++) {
            scanValueRSSI[i] = SCAN_AREA_Y + SCAN_AREA_HEIGHT; // Spektrum alján kezdjük (nincs látható jel)
            scanValueSNR[i] = 0;
            scanMark[i] = false;
            scanDataValid[i] = false; // Nincs érvényes adat zoom out után
        }

        // Zoom generáció nullázása - friss adatok
        zoomGeneration = 0;

        // Közös inicializálás
        currentScanPos = 0;

        // Skála vonalak újraszámítása (mindig szükséges zoom után)
        for (int i = 0; i < SCAN_RESOLUTION; i++) {
            scanScaleLine[i] = 0;
        }

        // Sáv határok újraszámítása
        scanBeginBand = -1;
        scanEndBand = SCAN_RESOLUTION;

        // Spektrum és címkék frissítése (üres spektrum)
        drawSpectrum();
        drawScale();
        drawFrequencyLabels();
        drawBandBoundaries();
        drawScanInfo();
    }
}

uint32_t ScreenScan::positionToFreq(uint16_t dataPos) {
    if (dataPos >= SCAN_RESOLUTION)
        return scanEndFreq;

    float ratio = (float)dataPos / SCAN_RESOLUTION;
    uint32_t totalBandwidth = scanEndFreq - scanStartFreq;
    return scanStartFreq + (uint32_t)(ratio * totalBandwidth);
}

uint16_t ScreenScan::freqToPosition(uint32_t freq) {
    if (freq <= scanStartFreq)
        return 0;
    if (freq >= scanEndFreq)
        return SCAN_RESOLUTION - 1;

    uint32_t totalBandwidth = scanEndFreq - scanStartFreq;
    float ratio = (float)(freq - scanStartFreq) / totalBandwidth;
    return (uint16_t)(ratio * SCAN_RESOLUTION);
}

bool ScreenScan::isDataValid(uint16_t scanPos) const {
    // Bounds check
    if (scanPos >= SCAN_RESOLUTION) {
        return false;
    }

    // Ha scanEmpty true, akkor még egyáltalán nem kezdődött el a scan
    if (scanEmpty) {
        return false;
    }

    // Ellenőrizzük, hogy a pozícióban van-e érvényes mérési adat
    return scanDataValid[scanPos];
}