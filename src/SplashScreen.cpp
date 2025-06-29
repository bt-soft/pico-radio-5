#include "SplashScreen.h"

/**
 * @brief SplashScreen konstruktor
 *
 * Inicializálja a splash képernyő objektumot a megadott TFT és SI4735 referenciákkal.
 * A konstruktor csak a referenciákat tárolja el, nem végez grafikai műveletet.
 *
 * @param tft TFT_eSPI objektum referencia a kijelző kezeléséhez
 * @param si4735 SI4735 objektum referencia a rádió chip információk lekérdezéséhez
 */
SplashScreen::SplashScreen(TFT_eSPI &tft) : tft(tft) {}

/**
 * @brief Splash képernyő megjelenítése
 *
 * Megjeleníti a teljes splash képernyőt az összes komponensével:
 * - Töröli a képernyőt fekete háttérrel
 * - Díszes keretet rajzol a képernyő körül
 * - Címet jelenít meg aláhúzással
 * - SI4735 chip információkat mutat be
 * - Build információkat (fordítás dátuma, ideje) jeleníti meg
 * - Program információkat (verzió, szerző) mutatja
 * - Opcionálisan progress bar-t inicializál
 *
 * @param showProgress true: progress bar megjelenítése, false: nincs progress bar
 * @param progressSteps progress lépések száma (jelenleg nem használt)
 */
void SplashScreen::show(bool showProgress, uint8_t progressSteps) {
    // Képernyő törlése
    tft.fillScreen(BACKGROUND_COLOR);

    // Elemek kirajzolása
    drawBorder();
    drawTitle();
    // drawSI4735Info();
    drawBuildInfo();
    drawProgramInfo();

    if (showProgress) {
        drawProgressBar(0);
    }
}

/**
 * @brief Díszes keret rajzolása a képernyő körül
 *
 * Egy dupla vonalú keretet rajzol a képernyő szélére, kék színnel.
 * A keret 2-3 pixel vastag és speciális díszítéssel rendelkezik a sarkoknál:
 * - Külső és belső keret vonalak
 * - Mind a négy sarokban diagonális díszítő pixelek
 * - A sarkok 8x8 pixeles díszítő mintázattal
 *
 * A keret 2 pixel margót hagy a képernyő szélétől.
 */
void SplashScreen::drawBorder() {
    // Díszes keret rajzolása
    int16_t w = tft.width();
    int16_t h = tft.height();

    // Külső keret
    tft.drawRect(2, 2, w - 4, h - 4, BORDER_COLOR);
    tft.drawRect(3, 3, w - 6, h - 6, BORDER_COLOR);

    // Sarkok díszítése
    for (int i = 0; i < 8; i++) {
        tft.drawPixel(5 + i, 5, BORDER_COLOR);
        tft.drawPixel(5, 5 + i, BORDER_COLOR);

        tft.drawPixel(w - 6 - i, 5, BORDER_COLOR);
        tft.drawPixel(w - 6, 5 + i, BORDER_COLOR);

        tft.drawPixel(5 + i, h - 6, BORDER_COLOR);
        tft.drawPixel(5, h - 6 - i, BORDER_COLOR);

        tft.drawPixel(w - 6 - i, h - 6, BORDER_COLOR);
        tft.drawPixel(w - 6, h - 6 - i, BORDER_COLOR);
    }
}

/**
 * @brief Program címének megjelenítése
 *
 * A képernyő tetején középre igazítva megjeleníti a program nevét:
 * - Nagy betűkkel (2x méret), cián színnel
 * - Középre igazított szöveg (TC_DATUM)
 * - 20 pixel magasságban a képernyő tetejétől
 * - Aláhúzás 3 vonallal a cím alatt 2 pixel távolságra
 *
 * Az aláhúzás pontosan a szöveg szélességében készül, és
 * három egymás alatti vízszintes vonalból áll.
 */
void SplashScreen::drawTitle() {
    tft.setFreeFont();
    tft.setTextSize(2);
    tft.setTextColor(TITLE_COLOR, BACKGROUND_COLOR);
    tft.setTextDatum(TC_DATUM); // Felső közép igazítás

    int16_t titleY = 20;
    tft.drawString(PROGRAM_NAME, tft.width() / 2, titleY);

    // Aláhúzás
    int16_t textWidth = tft.textWidth(PROGRAM_NAME);
    int16_t lineY = titleY + tft.fontHeight() + 2;
    int16_t lineStartX = (tft.width() - textWidth) / 2;
    int16_t lineEndX = lineStartX + textWidth;

    for (int i = 0; i < 3; i++) {
        tft.drawLine(lineStartX, lineY + i, lineEndX, lineY + i, TITLE_COLOR);
    }
}

/**
 * @brief SI4735 chip információk megjelenítése
 *
 * A képernyő középső részén táblázatos formában jeleníti meg
 * a SI4735 rádió chip részletes firmware információit:
 *
 * Megjelenített adatok:
 * - Part Number: A chip alkatrész azonosítója (hex formában)
 * - Firmware: Nagy és kis verziócode (major.minor formátum)
 * - Patch ID: Firmware patch azonosító (hex, 4 karakter)
 * - Component: Komponens verzió (major.minor formátum)
 * - Chip Rev: Chip revízió száma
 *
 * Formátum:
 * - Címsor: "SI4735 Firmware:" sárga színnel
 * - Címkék: bal oldal, fehér színnel
 * - Értékek: jobb oldal (180px), fehér színnel
 * - 20 pixel magasság soronként, 70px-től indul
 */
void SplashScreen::drawSI4735Info(SI4735 &si4735) {
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM); // Bal felső igazítás

    uint16_t startY = 70;
    uint16_t leftX = 15;
    uint16_t rightX = 180;
    uint8_t lineHeight = 20;
    uint16_t currentY = startY;

    // SI4735 címsor
    tft.setTextColor(INFO_LABEL_COLOR, BACKGROUND_COLOR);
    tft.drawString("SI4735 Firmware:", leftX, currentY);
    currentY += lineHeight + 5;

    // SI4735 adatok
    tft.setTextColor(INFO_VALUE_COLOR, BACKGROUND_COLOR);

    // Part Number
    tft.drawString("Part Number:", leftX, currentY);
    tft.drawString("0x" + String(si4735.getFirmwarePN(), HEX), rightX, currentY);
    currentY += lineHeight;

    // Firmware verzió
    tft.drawString("Firmware:", leftX, currentY);
    tft.drawString(String(si4735.getFirmwareFWMAJOR()) + "." + String(si4735.getFirmwareFWMINOR()), rightX, currentY);
    currentY += lineHeight;

    // Patch ID
    tft.drawString("Patch ID:", leftX, currentY);
    String patchHex = String(si4735.getFirmwarePATCHH(), HEX);
    if (patchHex.length() == 1)
        patchHex = "0" + patchHex;
    String patchLowHex = String(si4735.getFirmwarePATCHL(), HEX);
    if (patchLowHex.length() == 1)
        patchLowHex = "0" + patchLowHex;
    tft.drawString("0x" + patchHex + patchLowHex, rightX, currentY);
    currentY += lineHeight;

    // Component verzió
    tft.drawString("Component:", leftX, currentY);
    tft.drawString(String(si4735.getFirmwareCMPMAJOR()) + "." + String(si4735.getFirmwareCMPMINOR()), rightX, currentY);
    currentY += lineHeight;

    // Chip Rev
    tft.drawString("Chip Rev:", leftX, currentY);
    tft.drawString(String(si4735.getFirmwareCHIPREV()), rightX, currentY);
}

/**
 * @brief Build információk megjelenítése
 *
 * A program fordítási információit jeleníti meg a képernyő alsó részén:
 * - Fordítás dátuma (__DATE__ makró)
 * - Fordítás időpontja (__TIME__ makró)
 *
 * Formátum:
 * - Címsor: "Build Information:" sárga színnel
 * - Címkék: bal oldal (15px), fehér színnel
 * - Értékek: jobb oldal (120px), fehér színnel
 * - 16 pixel magasság soronként, 200px magasságtól indul
 * - Kompakt elrendezés 3 pixel extra térközzel a címsor alatt
 *
 * Ez a funkció a fordítás idejének nyomonkövetésére szolgál.
 */
void SplashScreen::drawBuildInfo() {
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);

    uint16_t startY = 200;
    uint16_t leftX = 15;
    uint16_t rightX = 120;
    uint8_t lineHeight = 16;
    uint16_t currentY = startY;

    // Build címsor
    tft.setTextColor(INFO_LABEL_COLOR, BACKGROUND_COLOR);
    tft.drawString("Build Information:", leftX, currentY);
    currentY += lineHeight + 3;

    tft.setTextColor(INFO_VALUE_COLOR, BACKGROUND_COLOR);

    // Dátum
    tft.drawString("Date:", leftX, currentY);
    tft.drawString(__DATE__, rightX, currentY);
    currentY += lineHeight;

    // Idő
    tft.drawString("Time:", leftX, currentY);
    tft.drawString(__TIME__, rightX, currentY);
}

/**
 * @brief Program információk megjelenítése
 *
 * A képernyő alján középre igazítva megjeleníti a program
 * alapvető azonosító információit:
 *
 * - Program verzió: "Version X.Y.Z" formátumban, zöld színnel
 * - Szerző neve: magenta/lila színnel
 *
 * Formátum:
 * - Alsó középre igazított szöveg (BC_DATUM)
 * - A képernyő aljától 50 pixel magasságban kezdődik
 * - 15 pixel távolság a verzió és szerző között
 * - A defines.h fájlból származó konstansokat használja
 */
void SplashScreen::drawProgramInfo() {
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(BC_DATUM); // Alsó közép igazítás

    uint16_t bottomY = tft.height() - 50;

    // Program verzió
    tft.setTextColor(VERSION_COLOR, BACKGROUND_COLOR);
    tft.drawString("Version " + String(PROGRAM_VERSION), tft.width() / 2, bottomY);

    // Szerző
    tft.setTextColor(AUTHOR_COLOR, BACKGROUND_COLOR);
    tft.drawString(PROGRAM_AUTHOR, tft.width() / 2, bottomY + 15);
}

/**
 * @brief Progress bar megjelenítése
 *
 * A képernyő alján egy vízszintes progress bar-t jelenít meg
 * a betöltési folyamat vizualizálására:
 *
 * Jellemzők:
 * - 200 pixel széles, 8 pixel magas
 * - Középre igazítva
 * - Képernyő aljától 25 pixel magasságban
 * - Fehér keret a progress bar körül
 * - Fekete háttér
 * - Zöld kitöltés a progress szerint
 *
 * A progress érték 0-100% között lehet, és ennek megfelelően
 * töltődik fel zöld színnel a bar.
 *
 * @param progress Jelenlegi haladás százalékban (0-100)
 */
void SplashScreen::drawProgressBar(uint8_t progress) {
    int16_t barWidth = 200;
    int16_t barHeight = 8;
    int16_t barX = (tft.width() - barWidth) / 2;
    int16_t barY = tft.height() - 25;

    // Progress bar háttér
    tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, TFT_WHITE);
    tft.fillRect(barX, barY, barWidth, barHeight, TFT_BLACK);

    // Progress kitöltés
    if (progress > 0) {
        int16_t fillWidth = (barWidth * progress) / 100;
        tft.fillRect(barX, barY, fillWidth, barHeight, TFT_GREEN);
    }
}

/**
 * @brief Progress frissítése üzenettel
 *
 * Frissíti a progress bar állását és opcionálisan megjeleníti
 * az aktuális művelet leírását:
 *
 * Funkciók:
 * - Kiszámítja a progress százalékot a lépések alapján
 * - Újrarajzolja a progress bar-t az új értékkel
 * - Ha üzenet van megadva, törli az előző üzenetet
 * - Az új üzenetet középre igazítva, sárga színnel jeleníti meg
 *
 * Üzenet pozíció:
 * - A képernyő aljától 45 pixel magasságban
 * - Középre igazítva (TC_DATUM)
 * - 10px margóval bal és jobb oldalon
 * - Sárga szöveg fekete háttéren
 *
 * @param step Aktuális lépés száma (1-től számozva)
 * @param totalSteps Összes lépés száma
 * @param message Megjelenítendő üzenet (ha nullptr, nincs üzenet)
 */
void SplashScreen::updateProgress(uint8_t step, uint8_t totalSteps, const char *message) {
    uint8_t progress = (step * 100) / totalSteps;
    drawProgressBar(progress);

    if (message != nullptr) {
        // Előző üzenet törlése
        tft.fillRect(10, tft.height() - 45, tft.width() - 20, 15, BACKGROUND_COLOR);

        // Új üzenet kiírása
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextColor(TFT_YELLOW, BACKGROUND_COLOR);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(message, tft.width() / 2, tft.height() - 45);
    }
}

/**
 * @brief Splash képernyő eltüntetése
 *
 * Egyszerűen fekete színnel kitölti a teljes képernyőt,
 * ezzel eltüntetve az összes splash képernyő elemet.
 *
 * Ez a legegyszerűbb módja a splash képernyő bezárásának,
 * mivel fekete háttérrel töröl mindent.
 */
void SplashScreen::hide() { tft.fillScreen(TFT_BLACK); }
