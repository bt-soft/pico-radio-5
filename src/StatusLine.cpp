#include "StatusLine.h"
#include "Config.h"
#include "PicoSensorUtils.h"
#include "Si4735Manager.h"
#include "rtvars.h"

// Állandók a komponens méreteihez
constexpr uint8_t STATUS_LINE_X = 0;
constexpr uint8_t STATUS_LINE_Y = 0;
// Az UIComponent::SCREEN_W-t használjuk a szélesség meghatározásához
constexpr uint8_t STATUS_LINE_HEIGHT = 16; // Állapotsor magassága pixelben

// Négyzetek szélességei (konstansokban)
constexpr uint8_t BOX_WIDTH_BFO = 32;               // BFO négyzet szélessége
constexpr uint8_t BOX_WIDTH_AGC = 32;               // AGC négyzet szélessége
constexpr uint8_t BOX_WIDTH_MODE = 32;              // Mód négyzet szélessége
constexpr uint8_t BOX_WIDTH_BANDWIDTH = 45;         // Sávszélesség négyzet szélessége
constexpr uint8_t BOX_WIDTH_BAND = 32;              // Sáv négyzet szélessége
constexpr uint8_t BOX_WIDTH_STEP = 45;              // Lépésköz négyzet szélessége
constexpr uint8_t BOX_WIDTH_ANTCAP = 40;            // Antenna kapacitás négyzet szélessége
constexpr uint8_t BOX_WIDTH_TEMP = 45;              // Hőmérséklet négyzet szélessége
constexpr uint8_t BOX_WIDTH_VOLTAGE = 45;           // Feszültség négyzet szélessége
constexpr uint8_t BOX_WIDTH_STATION_IN_MEMORY = 35; // Az állomás memóriában négyzet szélessége

// Default színek a négyzetek kereteinek
constexpr uint16_t BfoBoxColor = TFT_ORANGE;
constexpr uint16_t AgcBoxColor = TFT_COLOR(255, 130, 0); // Narancs
constexpr uint16_t ModeBoxColor = TFT_YELLOW;
constexpr uint16_t BandwidthBoxColor = TFT_COLOR(255, 127, 255); // Magenta
constexpr uint16_t BandBoxColor = TFT_CYAN;
constexpr uint16_t StepBoxColor = TFT_SKYBLUE;
constexpr uint16_t AntCapBoxColor = TFT_SILVER;          // Alapértelmezett AntCap szín
constexpr uint16_t TempBoxColor = TFT_YELLOW;            // Hőmérséklet színe
constexpr uint16_t VoltageBoxColor = TFT_GREENYELLOW;    // Feszültség színe
constexpr uint16_t StationInMemoryBoxColor = TFT_SILVER; // Az állomás memóriában színe

/**
 * @brief StatusLine konstruktor
 * @param x A komponens X koordinátája
 * @param y A komponens Y koordinátája
 * @param bandTable Hivatkozás a BandTable objektumra
 * @param colors Színséma (opcionális, alapértelmezett színsémát használ ha nincs megadva)
 * @details A komponens  kis négyzetekben jelenít meg a különböző állapotinformációkat.
 */
StatusLine::StatusLine(int16_t x, int16_t y, const ColorScheme &colors) : UIComponent(Rect(x, y, ::SCREEN_W, STATUS_LINE_HEIGHT), colors) {
    initializeBoxes();
    stationInMemory = false; // Kezdetben nincs állomás a memóriában
}

/**
 * @brief Inicializálja a 9 négyzet pozícióit és színeit
 */
void StatusLine::initializeBoxes() {
    uint16_t currentX = 0;
    const uint8_t GAP = 1; // 1 pixel hézag a négyzetek között

    // 0. BFO négyzet
    statusBoxes[0] = {currentX, BOX_WIDTH_BFO, BfoBoxColor};
    currentX += BOX_WIDTH_BFO + GAP;

    // 1. AGC négyzet
    statusBoxes[1] = {currentX, BOX_WIDTH_AGC, AgcBoxColor};
    currentX += BOX_WIDTH_AGC + GAP;

    // 2. Mód négyzet
    statusBoxes[2] = {currentX, BOX_WIDTH_MODE, ModeBoxColor};
    currentX += BOX_WIDTH_MODE + GAP;

    // 3. Sávszélesség négyzet
    statusBoxes[3] = {currentX, BOX_WIDTH_BANDWIDTH, BandwidthBoxColor};
    currentX += BOX_WIDTH_BANDWIDTH + GAP;

    // 4. Sáv négyzet
    statusBoxes[4] = {currentX, BOX_WIDTH_BAND, BandBoxColor};
    currentX += BOX_WIDTH_BAND + GAP;

    // 5. Lépésköz négyzet
    statusBoxes[5] = {currentX, BOX_WIDTH_STEP, StepBoxColor};
    currentX += BOX_WIDTH_STEP + GAP;

    // 6. Antenna kapacitás négyzet
    statusBoxes[6] = {currentX, BOX_WIDTH_ANTCAP, AntCapBoxColor};
    currentX += BOX_WIDTH_ANTCAP + GAP;

    // 7. Hőmérséklet négyzet
    statusBoxes[7] = {currentX, BOX_WIDTH_TEMP, TempBoxColor};
    currentX += BOX_WIDTH_TEMP + GAP;

    // 8. Feszültség négyzet (utolsó, nincs GAP utána)
    statusBoxes[8] = {currentX, BOX_WIDTH_VOLTAGE, VoltageBoxColor};
    currentX += BOX_WIDTH_VOLTAGE + GAP;

    // 9. a memória
    statusBoxes[9] = {currentX, BOX_WIDTH_STATION_IN_MEMORY, StationInMemoryBoxColor};
}

/**
 * @brief A komponens kirajzolása - kereteket rajzolja meg
 */
void StatusLine::draw() {
    if (!needsRedraw) {
        return;
    }

    // Csak a ténylegesen használt terület törlése, nem a teljes bounds
    // Kiszámoljuk az utolsó négyzet végének pozícióját
    uint16_t actualWidth = statusBoxes[9].x + statusBoxes[9].width;
    ::tft.fillRect(bounds.x, bounds.y, actualWidth, bounds.height, colors.screenBackground);

    // Négyzetek kereteinek kirajzolása
    drawBoxFrames();
    updateBfo();
    updateAgc();
    updateMode();
    updateBand();
    updateStep();
    updateBandwidth();
    updateAntCap();
    updateTemperature();
    updateVoltage();
    updateStationInMemory(stationInMemory);

    needsRedraw = false;
}

/**
 * @brief Kirajzolja az összes négyzet keretét
 */
void StatusLine::drawBoxFrames() {
    for (uint8_t i = 0; i < STATUS_LINE_BOXES; i++) {
        ::tft.drawRect(statusBoxes[i].x, bounds.y, statusBoxes[i].width, bounds.height, statusBoxes[i].color);
    }
}

/**
 * @brief Törli egy négyzet belső tartalmát (keret marad)
 */
void StatusLine::clearBoxContent(uint8_t boxIndex) {
    if (boxIndex >= STATUS_LINE_BOXES)
        return;

    resetFont();

    ::tft.fillRect(statusBoxes[boxIndex].x + 1, bounds.y + 1, statusBoxes[boxIndex].width - 2, bounds.height - 2, colors.screenBackground);
}

/**
 * @brief Szöveget ír egy négyzetbe
 */
void StatusLine::drawTextInBox(uint8_t boxIndex, const char *text, uint16_t textColor) {
    if (boxIndex >= STATUS_LINE_BOXES)
        return;

    ::tft.setTextColor(textColor, colors.screenBackground);
    ::tft.setTextSize(1);

    // Text datum beállítása középre igazításhoz
    ::tft.setTextDatum(MC_DATUM); // Middle Center datum

    // Négyzet közepének kiszámítása
    uint16_t centerX = statusBoxes[boxIndex].x + statusBoxes[boxIndex].width / 2;
    uint16_t centerY = bounds.y + bounds.height / 2;

    ::tft.drawString(text, centerX, centerY);

    // Datum visszaállítása alapértelmezettre
    ::tft.setTextDatum(TL_DATUM); // Top Left datum (alapértelmezett)
}

// ========== Értékfrissítő metódusok ==========

/**
 * @brief BFO értékének frissítése (0. négyzet)
 */
void StatusLine::updateBfo() {

    // BFO státusz szöveg előállítás
    char bfoText[10]; // Buffer a szövegnek (pl. "25Hz" vagy " BFO ")
    if (rtv::bfoOn) {
        // Formázzuk a lépésközt és a "Hz"-t a bufferbe
        snprintf(bfoText, sizeof(bfoText), "%dHz", rtv::currentBFOStep);
    } else {
        strcpy(bfoText, " BFO ");
    }

    // Töröljük a régi tartalmat és rajzoljuk ki az új szöveget
    clearBoxContent(0);
    drawTextInBox(0, bfoText, statusBoxes[0].color);
}

/**
 * @brief AGC értékének frissítése (1. négyzet)
 */
void StatusLine::updateAgc() {
    Si4735Runtime::AgcGainMode currentMode = static_cast<Si4735Runtime::AgcGainMode>(config.data.agcGain);

    uint16_t agcColor = statusBoxes[1].color; // Alapértelmezett AGC szín
    String agcText;

    if (currentMode == Si4735Runtime::AgcGainMode::Manual) {
        // Manual mode (ATT)
        agcText = "ATT" + String(config.data.currentAGCgain < 10 ? " " : "") + String(config.data.currentAGCgain);
    } else {
        // Automatic vagy Off mode (AGC)
        if (currentMode != Si4735Runtime::AgcGainMode::Automatic) {
            agcColor = TFT_SILVER; // AgcColor ha Auto, Silver ha Off
        }
        agcText = " AGC ";
    }

    clearBoxContent(1);
    drawTextInBox(1, agcText.c_str(), agcColor);
}

/**
 * @brief Demodulációs mód frissítése (2. négyzet)
 */
void StatusLine::updateMode() {
    // Ha nincs Si4735Manager, akkor N/A-t írunk ki
    const char *modeText = ::pSi4735Manager ? (rtv::CWShift ? "CW" : ::pSi4735Manager->getCurrentBandDemodModDesc()) : "N/A";
    clearBoxContent(2);
    drawTextInBox(2, modeText, statusBoxes[2].color);
}

/**
 * @brief HF sávszélesség frissítése (3. négyzet)
 */
void StatusLine::updateBandwidth() {
    // Ha nincs Si4735Manager, akkor N/A-t írunk ki
    String bandwidthText = ::pSi4735Manager ? ::pSi4735Manager->getCurrentBandWidthLabel() : "N/A";
    if (bandwidthText != "AUTO") {
        bandwidthText += "kHz"; // kHz egység hozzáadása
    }

    clearBoxContent(3);
    drawTextInBox(3, bandwidthText.c_str(), statusBoxes[3].color);
}

/**
 * @brief Sáv frissítése (4. négyzet)
 */
void StatusLine::updateBand() {

    // Ha nincs Si4735Manager, akkor N/A-t írunk ki
    const char *bandText = ::pSi4735Manager ? ::pSi4735Manager->getCurrentBandName() : "N/A";

    clearBoxContent(4);
    drawTextInBox(4, bandText, statusBoxes[4].color);
}

/**
 * @brief Hangolási frekvencia lépésköz frissítése (5. négyzet)
 */
void StatusLine::updateStep() {
    clearBoxContent(5);
    // Ha nincs Si4735Manager, akkor N/A-t írunk ki
    drawTextInBox(5, ::pSi4735Manager ? ::pSi4735Manager->currentStepSizeStr() : "N/A", statusBoxes[5].color);
}

/**
 * @brief Antenna kapacitás értékének frissítése (6. négyzet)
 */
void StatusLine::updateAntCap() {

    // Kiírjuk az értéket
    String value;
    uint16_t antCapColor = statusBoxes[6].color;
    if (::pSi4735Manager) {
        uint16_t currentAntCap = ::pSi4735Manager->getCurrentBand().antCap;
        bool isDefault = (currentAntCap == ::pSi4735Manager->getDefaultAntCapValue());
        antCapColor = isDefault ? statusBoxes[6].color : TFT_GREEN;

        if (isDefault) {
            value = F("AntC"); // A String konstruktor tudja kezelni a F() makrót
        } else {
            // Explicit String konverzió a számhoz, majd hozzáfűzés
            value = String(currentAntCap);
            value += "pF";
        }
    } else {
        value = "N/A"; // Ha nincs Si4735Manager, akkor N/A-t írunk ki
    }

    clearBoxContent(6);
    drawTextInBox(6, value.c_str(), antCapColor);
}

/**
 * @brief CPU mag hőmérsékletének frissítése (7. négyzet)
 */
void StatusLine::updateTemperature() {

    float temp = PicoSensorUtils::readCoreTemperature();
    String tempText = isnan(temp) ? "---" : String(temp, 1); // 1 tizedesjegy

    clearBoxContent(7);
    drawTextInBox(7, (tempText + "C").c_str(), statusBoxes[7].color);
}

/**
 * @brief Akkumulátor feszültségének frissítése (8. négyzet)
 */
void StatusLine::updateVoltage() {

    float voltage = PicoSensorUtils::readVBusExternal();              // Cache-olt érték
    String voltageText = isnan(voltage) ? "---" : String(voltage, 2); // 2 tizedesjegy

    clearBoxContent(8);
    drawTextInBox(8, (voltageText + "V").c_str(), statusBoxes[8].color);
}

/**
 * @brief "Memo" indikátor kirajzolása.
 * Ha az állomás a memóriában van, zölden jelenik meg kerettel.
 * Ha nincs a memóriában, halványan (ezüst) jelenik meg
 * @param isInMemo Igaz, ha az aktuális állomás a memóriában van.
 * @param initFont Ha true, akkor a betűtípus inicializálása történik.
 */
void StatusLine::updateStationInMemory(bool isInMemo) {
    stationInMemory = isInMemo;
    uint16_t color = StatusLine::stationInMemory ? TFT_GREEN : statusBoxes[9].color;
    clearBoxContent(9);
    drawTextInBox(9, "Memo", color);
}