
#include "FreqDisplay.h"
#include "DSEG7_Classic_Mini_Regular_34.h"
#include "Si4735Manager.h"
#include "UIColorPalette.h"
#include "defines.h"

// === Globális színkonfigurációk ===
const FreqSegmentColors defaultNormalColors = UIColorPalette::createNormalFreqColors();
const FreqSegmentColors defaultBfoColors = UIColorPalette::createBfoFreqColors();

// === Karakterszélesség konstansok (DSEG7_Classic_Mini_Regular_34 font) ===
// Valós mért értékek a font-ból
constexpr static int CHAR_WIDTH_DIGIT = 25; // '0'-'9' karakterek szélessége
constexpr static int CHAR_WIDTH_DOT = 3;    // '.' karakter szélessége
constexpr static int CHAR_WIDTH_SPACE = 1;  // ' ' karakter szélessége
constexpr static int CHAR_WIDTH_DASH = 23;  // '-' karakter szélessége

/**
 * @brief FreqDisplay konstruktor - inicializálja a frekvencia kijelző komponenst
 */
FreqDisplay::FreqDisplay(const Rect &bounds_param)
    : UIComponent(bounds_param), spr(&::tft), normalColors(defaultNormalColors), bfoColors(defaultBfoColors), customColors(defaultNormalColors), useCustomColors(false), currentDisplayFrequency(0), hideUnderline(false),
      lastUpdateTime(0), needsFullClear(true) {

    // Alapértelmezett háttérszín beállítása
    this->colors.background = TFT_COLOR_BACKGROUND; // Érintési területek inicializálása
    for (int i = 0; i < 3; i++) {
        ssbCwTouchDigitAreas[i][0] = 0;
        ssbCwTouchDigitAreas[i][1] = 0;
    }

    // Explicit újrarajzolás kérése az első megjelenítéshez
    markForRedraw();
}

/**
 * @brief Beállítja a megjelenítendő frekvenciát
 */
void FreqDisplay::setFrequency(uint16_t freq, bool forceRedraw) {

    if (forceRedraw || currentDisplayFrequency != freq) {
        unsigned long currentTime = millis();

        // Villogás csökkentése: csak akkor frissítünk, ha legalább 50ms eltelt az előző frissítés óta
        // KIVÉVE ha forceRedraw = true vagy jelentős változás van (>10 egység)
        if (forceRedraw || (currentTime - lastUpdateTime > 50) || abs((int16_t)freq - (int16_t)currentDisplayFrequency) > 10) {

            currentDisplayFrequency = freq;
            lastUpdateTime = currentTime;
            markForRedraw();
        } else {
            // Csak a frekvencia értéket frissítjük, de nem rajzolunk újra azonnal
            currentDisplayFrequency = freq;
        }
    }
}

/**
 * @brief Beállítja a megjelenítendő frekvenciát teljes újrarajzolással
 */
void FreqDisplay::setFrequencyWithFullDraw(uint16_t freq, bool hideUnderline) {
    currentDisplayFrequency = freq;
    this->hideUnderline = hideUnderline;
    needsFullClear = true; // Teljes háttér törlés szükséges
    markForRedraw();
}

/**
 * @brief Beállítja az egyedi színkonfigurációt (pl. képernyővédő módhoz)
 */
void FreqDisplay::setCustomColors(const FreqSegmentColors &colors) {
    customColors = colors;
    useCustomColors = true;
    needsFullClear = true; // Színváltásnál teljes háttér törlés szükséges
    markForRedraw();
}

/**
 * @brief Visszaállítja az alapértelmezett színkonfigurációt
 */
void FreqDisplay::resetToDefaultColors() {
    useCustomColors = false;
    markForRedraw();
}

/**
 * @brief Beállítja, hogy megjelenjen-e a finomhangolás aláhúzás (képernyővédő mód)
 */
void FreqDisplay::setHideUnderline(bool hide) {
    if (hideUnderline != hide) {
        hideUnderline = hide;
        markForRedraw();
    }
}

/**
 * @brief Visszaadja az aktuális színkonfigurációt
 */
const FreqSegmentColors &FreqDisplay::getSegmentColors() const {
    if (useCustomColors) {
        return customColors;
    }
    return rtv::bfoOn ? bfoColors : normalColors;
}

/**
 * @brief Meghatározza a frekvencia formátumot és adatokat a mód alapján
 */
FreqDisplay::FrequencyDisplayData FreqDisplay::getFrequencyDisplayData(uint16_t frequency) {
    FrequencyDisplayData data;
    uint8_t demodMode = ::pSi4735Manager->getCurrentBand().currDemod;
    uint8_t bandType = ::pSi4735Manager->getCurrentBandType();

    if (demodMode == FM_DEMOD_TYPE) {
        // FM mód: 100.50 MHz - optimalizált integer számítás
        data.unit = "MHz";

        // FM módban mindig tizedesjeggyel jelenjük meg (normál és képernyővédő módban is)
        data.mask = "188.88";
        int wholePart = frequency / 100;
        int fracPart = frequency % 100;
        char buffer[16];
        sprintf(buffer, "%d.%02d", wholePart, fracPart);
        data.freqStr = String(buffer);

    } else if (demodMode == AM_DEMOD_TYPE) {
        if (bandType == MW_BAND_TYPE || bandType == LW_BAND_TYPE) {
            // MW/LW: 1440 kHz
            data.unit = "kHz";
            data.mask = "8888";
            data.freqStr = String(frequency);
        } else {
            // SW AM: 27.200 MHz (CB) és 30.000 MHz sávok - optimalizált integer számítás
            data.unit = "MHz";
            // Normál mód: tizedesjeggyel "27.200" MHz
            data.mask = "88.888"; // 5 karakteres maszk - max 30 MHz
            int wholePart = frequency / 1000;
            int fracPart = frequency % 1000;
            char buffer[16];
            sprintf(buffer, "%d.%03d", wholePart, fracPart);
            data.freqStr = String(buffer);
        }

    } else if (demodMode == LSB_DEMOD_TYPE || demodMode == USB_DEMOD_TYPE || demodMode == CW_DEMOD_TYPE) {
        // SSB/CW mód: finomhangolással korrigált frekvencia
        if (rtv::bfoOn) {
            // BFO mód: csak BFO értéket mutatunk
            data.unit = "Hz";
            data.mask = "-888";
            data.freqStr = String(rtv::currentBFOmanu);
        } else {
            // Normál SSB/CW: frekvencia formázás
            data.unit = "kHz";
            uint32_t displayFreqHz = (uint32_t)frequency * 1000 - rtv::freqDec;
            long khz_part = displayFreqHz / 1000;

            if (useCustomColors) {
                // Képernyővédő mód: csak egész kHz értékek
                data.mask = "88 888";
                char buffer[16];
                memset(buffer, 0, sizeof(buffer));

                if (khz_part >= 10000) {
                    // 5+ digit: "21074" -> "21 074"
                    long thousands = khz_part / 1000;
                    long remainder = khz_part % 1000;
                    sprintf(buffer, "%ld %03ld", thousands, remainder);
                } else {
                    // 1-4 digit: "475" -> "  475", "3630" -> "3 630"
                    if (khz_part >= 1000) {
                        // 4 digit: "3630" -> " 3 630"
                        long thousands = khz_part / 1000;
                        long remainder = khz_part % 1000;
                        sprintf(buffer, " %ld %03ld", thousands, remainder);
                    } else {
                        // 1-3 digit: "475" -> "   475"
                        sprintf(buffer, "   %ld", khz_part);
                    }
                }
                data.freqStr = String(buffer);
            } else {
                // Normál mód: tizedesjegyekkel
                data.mask = "88 888.88";
                int hz_tens_part = abs(static_cast<int>(displayFreqHz % 1000)) / 10;
                char buffer[32];
                memset(buffer, 0, sizeof(buffer));

                if (khz_part >= 10000) {
                    // 5+ digit: "21074" -> "21 074.50"
                    long thousands = khz_part / 1000;
                    long remainder = khz_part % 1000;
                    sprintf(buffer, "%ld %03ld.%02d", thousands, remainder, hz_tens_part);
                } else {
                    // 1-4 digit: "475" -> "  475.00", "3630" -> "3 630.00"
                    if (khz_part >= 1000) {
                        // 4 digit: "3630" -> " 3 630.00"
                        long thousands = khz_part / 1000;
                        long remainder = khz_part % 1000;
                        sprintf(buffer, " %ld %03ld.%02d", thousands, remainder, hz_tens_part);
                    } else {
                        // 1-3 digit: "475" -> "   475.00"
                        sprintf(buffer, "   %ld.%02d", khz_part, hz_tens_part);
                    }
                }
                data.freqStr = String(buffer);
            }

            // Extra védelem: ellenőrizzük, hogy a string nem korrupt
            if (data.freqStr.length() == 0 || data.freqStr.length() > 15) {
                data.freqStr = "ERROR"; // Fallback érték
            }
        }
    }

    return data;
}

/**
 * @brief Segédmetódus szöveg rajzolásához
 */
void FreqDisplay::drawText(const String &text, int x, int y, int textSize, uint8_t datum, uint16_t color) {
    tft.setFreeFont();
    tft.setTextSize(textSize);
    tft.setTextDatum(datum);
    tft.setTextColor(color, this->colors.background);
    tft.drawString(text, x, y);
}

/**
 * @brief Rajzolja FM/AM/LW stílusú frekvencia kijelzőt (balra igazított frekvencia)
 */
void FreqDisplay::drawFmAmLwStyle(const FrequencyDisplayData &data) {
    const FreqSegmentColors &colors = getSegmentColors(); // 1. Frekvencia sprite pozicionálása: keret bal szélénél
    // Sprite szélesség: rögzített szélesség a konzisztens megjelenéshez
    tft.setFreeFont(&DSEG7_Classic_Mini_Regular_34);

    // Rögzített sprite szélesség a maszk alapján (dinamikus textWidth helyett)
    int freqSpriteWidth = calculateFixedSpriteWidth(data.mask);

    int freqSpriteX = bounds.x; // nincs margin a bal szélétől
    int freqSpriteY = bounds.y; // Frekvencia sprite létrehozása és rajzolása
    spr.createSprite(freqSpriteWidth, FREQ_7SEGMENT_HEIGHT);
    spr.fillSprite(this->colors.background);
    spr.setTextSize(1);
    spr.setTextPadding(0);
    spr.setFreeFont(&DSEG7_Classic_Mini_Regular_34);

    // Inaktív számjegyek rajzolása (ha engedélyezve van) - JOBBRA igazítva a maszkhoz
    if (config.data.tftDigitLigth) {
        spr.setTextColor(colors.inactive);
        spr.setTextDatum(BR_DATUM);                                       // Jobb alsó sarokhoz igazítás
        spr.drawString(data.mask, freqSpriteWidth, FREQ_7SEGMENT_HEIGHT); // Jobb szélre igazítva
    }

    // Aktív frekvencia számok rajzolása - JOBBRA igazítva a maszkhoz
    spr.setTextColor(colors.active);
    spr.setTextDatum(BR_DATUM);                                          // Jobb alsó sarokhoz igazítás
    spr.drawString(data.freqStr, freqSpriteWidth, FREQ_7SEGMENT_HEIGHT); // Jobb szélre igazítva

    // Sprite kirajzolása és memória felszabadítása
    spr.pushSprite(freqSpriteX, freqSpriteY);
    spr.deleteSprite();

    // 2. Mértékegység pozicionálása: frekvencia sprite után jobbra
    int unitX = freqSpriteX + freqSpriteWidth + 8; // 8 pixel gap a frekvencia után
    int unitY = bounds.y + FREQ_7SEGMENT_HEIGHT;   // Digitek alsó vonalával egy magasságban

    // Mértékegység rajzolása
    tft.setFreeFont();
    tft.setTextSize(UNIT_TEXT_SIZE);
    drawText(data.unit, unitX, unitY, UNIT_TEXT_SIZE, BL_DATUM, colors.indicator);
}

/**
 * @brief Rajzolja SSB/CW stílusú frekvencia kijelzőt (balra igazított frekvencia, finomhangolás, mértékegység alul)
 */
void FreqDisplay::drawSsbCwStyle(const FrequencyDisplayData &data) {
    const FreqSegmentColors &colors = getSegmentColors();

    if (rtv::bfoOn) {
        // BFO mód: külön kezelés
        drawBfoStyle(data);
        return;
    }

    // 1. Frekvencia sprite pozicionálása: keret bal szélénél
    int freqSpriteX = bounds.x + 5; // 5 pixel margin a bal szélétől
    int freqSpriteWidth = calculateFixedSpriteWidth(data.mask);
    int freqSpriteY = bounds.y;

    // Frekvencia sprite rajzolása space-ekkel
    drawFrequencySpriteWithSpaces(data, freqSpriteX, freqSpriteY, freqSpriteWidth);

    // 2. Finomhangolás aláhúzás rajzolása (ha nem elrejtett és nem BFO mód)
    if (!hideUnderline && !rtv::bfoOn) {
        drawFineTuningUnderline(freqSpriteX, freqSpriteWidth);

        // Érintési területek kiszámítása az aláhúzáshoz
        calculateSsbCwTouchAreas(freqSpriteX, freqSpriteWidth);
    }

    // 3. Mértékegység pozicionálása
    int unitX, unitY;
    uint8_t textDatum;

    if (hideUnderline) {
        // Képernyővédő mód: mértékegység az utolsó digit után (jobb oldalán)
        // Képernyővédő módban a maszk rövidebb ("88 888" vs "88 888.88")
        // A frekvencia sprite jobb szélétől számoljuk a pozíciót
        unitX = freqSpriteX + freqSpriteWidth + 5; // 5 pixel távolság a sprite jobb szélétől
        unitY = bounds.y + FREQ_7SEGMENT_HEIGHT + UNIT_Y_OFFSET_SSB_CW;
        textDatum = BL_DATUM; // Bal alsó sarokhoz igazítás
    } else {
        // Normál mód: mértékegység az utolsó digit alatt, aláhúzás alatt
        // Az utolsó digit (10Hz) pozíciója: digit10Hz_offset a sprite bal szélétől
        // int digit10Hz_offset = 196;                                // 10Hz digit (8. pozíció a maszkban)
        // int digitWidth = 25;                                       // Ismert DSEG7 digit szélesség
        // unitX = freqSpriteX + digit10Hz_offset + (digitWidth / 2); // Digit közepéhez igazítva
        // unitY = bounds.y + FREQ_7SEGMENT_HEIGHT                    //
        //         + UNDERLINE_Y_OFFSET + UNDERLINE_HEIGHT            //
        //         + 20;                                              // Aláhúzás alatt 20 pixelrel lejjebb

        // Egy alsó vonalba a digitekkel
        unitX = freqSpriteX + 250;
        unitY = bounds.y + FREQ_7SEGMENT_HEIGHT;

        textDatum = BR_DATUM; // Jobb alsó sarokhoz igazítás
    }

    drawText(data.unit, unitX, unitY, UNIT_TEXT_SIZE, textDatum, colors.indicator);
}

/**
 * @brief Kiszámítja a sprite szélességét space karakterekkel együtt
 */
int FreqDisplay::calculateSpriteWidthWithSpaces(const char *mask) {
    // Egyszerűsített számítás konstansokkal
    const int SPACE_GAP_WIDTH = 8; // Vizuális gap space karakterek helyett
    int totalWidth = 0;
    int maskLen = strlen(mask);

    for (int i = 0; i < maskLen; i++) {
        int charWidth = 0;
        if (mask[i] == ' ') {
            charWidth = SPACE_GAP_WIDTH; // Vizuális gap a space helyett
        } else {
            charWidth = getCharacterWidth(mask[i]);
        }
        totalWidth += charWidth;
    }

    return totalWidth;
}

/**
 * @brief Megbízható sprite szélesség számítás konstansokkal (textWidth() helyett)
 */
int FreqDisplay::calculateFixedSpriteWidth(const String &mask) { // Konstans értékek a különböző maszkokhoz - ezek nem változnak futás közben
    if (mask == "188.88") {
        return 130; // FM: "188.88"
    } else if (mask == "8888") {
        return 100; // MW/LW: "8888"
    } else if (mask == "88.888") {
        return 130; // SW AM: "88.888" (CB és 30MHz sávokhoz)
    } else if (mask == "88 888.88") {
        return 208; // SSB/CW normál: "88 888.88"
    } else if (mask == "88 888") {
        return 150; // SSB/CW képernyővédő: "88 888" (5 digit + space, extra margin)
    } else if (mask == "-888") {
        return 100; // BFO: "-888" (-999...+999 tartomány)
    }

    // Fallback: számítás konstansokkal
    return calculateSpriteWidthWithSpaces(mask.c_str());
}

/**
 * @brief Rajzolja a frekvencia sprite-ot space karakterekkel
 */
void FreqDisplay::drawFrequencySpriteWithSpaces(const FrequencyDisplayData &data, int x, int y, int width) {
    const FreqSegmentColors &colors = getSegmentColors(); // Sprite létrehozása
    spr.createSprite(width, FREQ_7SEGMENT_HEIGHT);
    spr.fillSprite(this->colors.background);
    spr.setTextSize(1);
    spr.setTextPadding(0);
    spr.setFreeFont(&DSEG7_Classic_Mini_Regular_34);

    // Inaktív számjegyek rajzolása (ha engedélyezve van) - JOBBRA igazítva a maszkhoz
    if (config.data.tftDigitLigth) {
        spr.setTextColor(colors.inactive);
        spr.setTextDatum(BR_DATUM);                             // Jobb alsó sarokhoz igazítás
        spr.drawString(data.mask, width, FREQ_7SEGMENT_HEIGHT); // Jobb szélre igazítva
    }

    // Aktív frekvencia számok rajzolása - JOBBRA igazítva a maszkhoz
    spr.setTextColor(colors.active);
    spr.setTextDatum(BR_DATUM);                                // Jobb alsó sarokhoz igazítás
    spr.drawString(data.freqStr, width, FREQ_7SEGMENT_HEIGHT); // Jobb szélre igazítva

    // Sprite kirajzolása és memória felszabadítása
    spr.pushSprite(x, y);
    spr.deleteSprite();
}

/**
 * @brief Rajzolja a finomhangolás aláhúzást SSB/CW módban
 */
void FreqDisplay::drawFineTuningUnderline(int freqSpriteX, int freqSpriteWidth) {
    const FreqSegmentColors &colors = getSegmentColors();

    // Az aláhúzás digit pozíciói relatívan a sprite bal szélétől (balra igazított sprite miatt):
    // A maszk "88 888.88" esetén az utolsó 3 digit pozíciói a bal széltől számítva
    int digit1kHz_offset = 138;  // 1kHz digit (5. pozíció a maszkban)
    int digit100Hz_offset = 170; // 100Hz digit (7. pozíció a maszkban)
    int digit10Hz_offset = 196;  // 10Hz digit (8. pozíció a maszkban)
    int digitPositions[3] = {freqSpriteX + digit1kHz_offset, freqSpriteX + digit100Hz_offset, freqSpriteX + digit10Hz_offset};

    int digitWidth = 25; // Ismert DSEG7 digit szélesség

    // Aláhúzás rajzolása a kiválasztott digit alatt
    if (rtv::freqstepnr >= 0 && rtv::freqstepnr < 3) {
        int digitCenter = digitPositions[rtv::freqstepnr];
        int underlineY = bounds.y + FREQ_7SEGMENT_HEIGHT + UNDERLINE_Y_OFFSET;

        // Aláhúzás középre igazítva a digit alatt
        int underlineX = digitCenter - (digitWidth / 2);

        // Előbb töröljük az egész aláhúzási területet (mind a 3 digit területe)
        int totalUnderlineWidth = digitPositions[2] - digitPositions[0] + digitWidth;
        int clearStartX = digitPositions[0] - (digitWidth / 2);

        tft.fillRect(clearStartX, underlineY, totalUnderlineWidth, UNDERLINE_HEIGHT, this->colors.background);

        // Aztán rajzoljuk az aktív aláhúzást
        tft.fillRect(underlineX, underlineY, digitWidth, UNDERLINE_HEIGHT, colors.indicator);
    }
}

/**
 * @brief Kiszámítja az SSB/CW frekvencia érintési területeket
 */
void FreqDisplay::calculateSsbCwTouchAreas(int freqSpriteX, int freqSpriteWidth) {
    // EGYSZERŰSÍTETT MEGOLDÁS: Ugyanazok a hard-coded relatív pozíciók, mint a finomhangolásban

    int digit1kHz_offset = 135;  // 1kHz digit (5. pozíció a maszkban)
    int digit100Hz_offset = 170; // 100Hz digit (7. pozíció a maszkban)
    int digit10Hz_offset = 193;  // 10Hz digit (8. pozíció a maszkban)

    int digitPositions[3] = {freqSpriteX + digit1kHz_offset, freqSpriteX + digit100Hz_offset, freqSpriteX + digit10Hz_offset};

    int digitWidth = 25; // Ismert DSEG7 digit szélesség

    // Touch területek beállítása
    ssbCwTouchDigitAreas[0][0] = digitPositions[0] - digitWidth / 2; // 1kHz digit bal széle
    ssbCwTouchDigitAreas[0][1] = digitPositions[0] + digitWidth / 2; // 1kHz digit jobb széle

    ssbCwTouchDigitAreas[1][0] = digitPositions[1] - digitWidth / 2; // 100Hz digit bal széle
    ssbCwTouchDigitAreas[1][1] = digitPositions[1] + digitWidth / 2; // 100Hz digit jobb széle

    ssbCwTouchDigitAreas[2][0] = digitPositions[2] - digitWidth / 2; // 10Hz digit bal széle
    ssbCwTouchDigitAreas[2][1] = digitPositions[2] + digitWidth / 2; // 10Hz digit jobb széle
}

/**
 * @brief Rajzolja a frekvencia kijelzőt a megadott mód szerint
 */
void FreqDisplay::drawFrequencyDisplay(const FrequencyDisplayData &data) {
    if (::pSi4735Manager->isCurrentDemodSSBorCW()) {
        drawSsbCwStyle(data);
    } else {
        drawFmAmLwStyle(data);
    }
}

/**
 * @brief Fő rajzolási metódus
 */
void FreqDisplay::draw() {
    if (!needsRedraw) {
        return;
    }

    // BFO animáció kezelése külön, mielőtt bármit rajzolnánk
    if (rtv::bfoTr) {
        handleBfoAnimation();
        rtv::bfoTr = false;
        needsFullClear = true; // Animáció után teljes újrarajzolás szükséges
    }

    // Csak akkor töröljük a hátteret, ha szükséges (pl. első rajzolás, mód váltás)
    if (needsFullClear) {
        tft.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, this->colors.background);
        needsFullClear = false; // Reset a flag
    }

    // Frekvencia adatok meghatározása
    FrequencyDisplayData data = getFrequencyDisplayData(currentDisplayFrequency);

    // Frekvencia rajzolása
    drawFrequencyDisplay(data);

    // Debug keret - segít az optimalizálásban és pozíciók ellenőrzésében
    // tft.drawRect(bounds.x, bounds.y, bounds.width, bounds.height, TFT_RED);

    needsRedraw = false;
}

/**
 * @brief Érintési esemény kezelése
 */
bool FreqDisplay::handleTouch(const TouchEvent &event) {
    // Csak SSB/CW módban és ha nincs elrejtve az aláhúzás
    if (!::pSi4735Manager->isCurrentDemodSSBorCW() || hideUnderline || rtv::bfoOn) {
        return false;
    }

    // Pozíció ellenőrzése
    if (!bounds.contains(event.x, event.y)) {
        return false;
    }

    // Digit érintés ellenőrzése
    for (int i = 0; i < 3; i++) {
        if (event.x >= ssbCwTouchDigitAreas[i][0] && event.x < ssbCwTouchDigitAreas[i][1]) {
            // Digit kiválasztása
            if (rtv::freqstepnr != i) {
                rtv::freqstepnr = i; // Frekvencia lépés beállítása
                // i=0: 1kHz digit, i=1: 100Hz digit, i=2: 10Hz digit
                switch (i) {
                    case 0:
                        rtv::freqstep = 1000;
                        break; // 1kHz
                    case 1:
                        rtv::freqstep = 100;
                        break; // 100Hz
                    case 2:
                        rtv::freqstep = 10;
                        break; // 10Hz
                    default:
                        break;
                }

                markForRedraw();
            }
            return true;
        }
    }

    return false;
}

/**
 * @brief Visszaadja egy karakter szélességét konstansok alapján
 */
int FreqDisplay::getCharacterWidth(char c) {
    if (c >= '0' && c <= '9') {
        return CHAR_WIDTH_DIGIT;
    }
    switch (c) {
        case '.':
            return CHAR_WIDTH_DOT;
        case ' ':
            return CHAR_WIDTH_SPACE;
        case '-':
            return CHAR_WIDTH_DASH;
        default:
            return CHAR_WIDTH_DIGIT; // Biztonsági alapértelmezett
    }
}

/**
 * @brief Rajzolja a BFO módot (BFO érték nagyban, fő frekvencia kicsiben)
 */
void FreqDisplay::drawBfoStyle(const FrequencyDisplayData &data) {
    const FreqSegmentColors &colors = getSegmentColors(); // BFO konstansok - új elrendezés: [-123] [Hz] [BFO] a felső sorban, [-123] [7.074.50] [kHz] az alsó sorban
    constexpr uint16_t BfoSpriteRightMargin = 115;        // BFO sprite jobb széle

    // Hz és BFO feliratok a 7-szegmenses sprite jobb oldalán

    constexpr uint16_t BfoHzLabelXOffset = BfoSpriteRightMargin + 10; // Hz felirat a sprite után 10 pixel
    constexpr uint16_t BfoHzLabelYOffset = 20;                        // Hz felirat a 7-szegmenses felső részénél

    constexpr uint16_t BfoLabelRectXOffset = BfoSpriteRightMargin + 40; // BFO felirat a Hz után
    constexpr uint16_t BfoLabelRectYOffset = 0;                         // BFO felirat a 7-szegmenses felső részénél
    constexpr uint16_t BfoLabelRectW = 42;
    constexpr uint16_t BfoLabelRectH = 20;

    // Mini frekvencia és kHz a 7-szegmenses alsó részével egy vonalban
    constexpr uint16_t BfoMiniFreqX = BfoSpriteRightMargin + 105; // Mini frekvencia a sprite után
    constexpr uint16_t BfoMiniUnitXOffset = 20;                   // kHz a mini frekvencia után

    // Mini frekvencia Y pozíciója - alja egy vonalban a 7-szegmenses aljával
    uint16_t BfoMiniFreqY = bounds.y + FREQ_7SEGMENT_HEIGHT; // 1. BFO érték kirajzolása a 7 szegmensesre (balra pozicionálva)
    int bfoSpriteWidth = calculateFixedSpriteWidth(data.mask);

    // BFO sprite pozíciója: jobb széle BfoSpriteRightMargin-nál legyen
    int bfoSpriteX = bounds.x + BfoSpriteRightMargin - bfoSpriteWidth;
    int bfoSpriteY = bounds.y;

    // BFO frekvencia sprite létrehozása
    spr.createSprite(bfoSpriteWidth, FREQ_7SEGMENT_HEIGHT);
    spr.fillSprite(this->colors.background);
    spr.setTextSize(1);
    spr.setTextPadding(0);
    spr.setFreeFont(&DSEG7_Classic_Mini_Regular_34);
    spr.setTextDatum(BR_DATUM);

    // Inaktív számjegyek rajzolása (ha engedélyezve van)
    if (config.data.tftDigitLigth) {
        spr.setTextColor(colors.inactive);
        spr.drawString(data.mask, bfoSpriteWidth, FREQ_7SEGMENT_HEIGHT);
    }

    // Aktív BFO érték rajzolása
    spr.setTextColor(colors.active);
    spr.drawString(data.freqStr, bfoSpriteWidth, FREQ_7SEGMENT_HEIGHT);

    // Sprite kirajzolása és törlése
    spr.pushSprite(bfoSpriteX, bfoSpriteY);
    spr.deleteSprite();

    // 2. BFO "Hz" felirat rajzolása
    drawText("Hz", bounds.x + BfoHzLabelXOffset, bounds.y + BfoHzLabelYOffset, UNIT_TEXT_SIZE, BL_DATUM, colors.indicator);

    // 3. BFO felirat háttérrel
    tft.fillRect(bounds.x + BfoLabelRectXOffset, bounds.y + BfoLabelRectYOffset, BfoLabelRectW, BfoLabelRectH, colors.active);

    tft.setFreeFont();
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLACK, colors.active);
    tft.drawString("BFO", bounds.x + BfoLabelRectXOffset + BfoLabelRectW / 2,
                   bounds.y + BfoLabelRectYOffset + BfoLabelRectH / 2); // 4. Fő frekvencia kisebb méretben (jobb oldalon, alja egy vonalban a 7-szegmenses aljával)
    // Optimalizált frekvencia számítás
    char freqBuffer[16];
    calculateBfoFrequency(freqBuffer, sizeof(freqBuffer));

    drawText(String(freqBuffer), bounds.x + BfoMiniFreqX, BfoMiniFreqY, UNIT_TEXT_SIZE, BR_DATUM, colors.indicator);

    // 5. Fő frekvencia "kHz" felirata még kisebb méretben (ugyanazon a vonalon)
    drawText("kHz", bounds.x + BfoMiniFreqX + BfoMiniUnitXOffset, BfoMiniFreqY, 1, BR_DATUM, colors.indicator);
}

/**
 * @brief Kezeli a BFO be/kikapcsolási animációt
 */
void FreqDisplay::handleBfoAnimation() {
    const FreqSegmentColors &colors = getSegmentColors(); // Optimalizált frekvencia számítás
    char freqBuffer[16];
    calculateBfoFrequency(freqBuffer, sizeof(freqBuffer)); // Pozíció számítás optimalizálása - eredeti logika helyreállítása
    constexpr uint16_t BfoSpriteRightMargin = 115;
    constexpr uint16_t BfoMiniFreqX = BfoSpriteRightMargin + 105;

    const int baseStartX = bounds.x + 5;
    const int baseEndX = bounds.x + 5 + (BfoMiniFreqX - 5) * 3 / 4; // Eredeti 3/4-es számítás

    int startX, endX, startSize, endSize;
    if (rtv::bfoOn) {
        // BFO bekapcsolás: nagy frekvencia → mini frekvencia
        startX = baseStartX;
        endX = baseEndX;
        startSize = 4; // Nagy méret
        endSize = 1;   // Kis méret
    } else {
        // BFO kikapcsolás: mini frekvencia → nagy frekvencia
        startX = baseEndX;
        endX = baseStartX;
        startSize = 1; // Kis méret
        endSize = 4;   // Nagy méret
    }

    // Animáció: 4 lépésben interpolálunk pozíció és méret között
    for (uint8_t i = 0; i < 4; i++) {
        // Teljes képernyő törlése minden lépésben
        tft.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, this->colors.background);

        // Interpoláció számítása (0.0 - 1.0 között)
        float progress = (float)i / 3.0f;

        // Pozíció interpoláció
        int animX = startX + static_cast<int>((endX - startX) * progress);

        // Méret interpoláció
        int textSize = startSize + static_cast<int>((endSize - startSize) * progress);
        if (textSize < 1)
            textSize = 1;

        int animY = bounds.y + FREQ_7SEGMENT_HEIGHT;

        // Animált szöveg rajzolása
        tft.setFreeFont();
        tft.setTextSize(textSize);
        tft.setTextDatum(BL_DATUM); // Bal alsó sarokhoz igazítás
        tft.setTextColor(colors.indicator, this->colors.background);
        tft.drawString(String(freqBuffer), animX, animY);
        delay(100); // 100ms késleltetés lépésenként
    }

    // Animáció után teljes terület törlése a maradványok eltávolítására
    tft.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, this->colors.background);
}

/**
 * @brief Kényszeríti a teljes újrarajzolást (BFO módváltáskor)
 */
void FreqDisplay::forceFullRedraw() {
    needsFullClear = true;
    markForRedraw();
}

/**
 * @brief Optimalizált segédmetódus a BFO frekvencia számításához
 */
void FreqDisplay::calculateBfoFrequency(char *buffer, size_t bufferSize) {
    uint32_t bfoOffset = rtv::lastBFO;
    uint32_t displayFreqHz = (uint32_t)currentDisplayFrequency * 1000 - bfoOffset;
    long khz_part = displayFreqHz / 1000;
    int hz_tens_part = abs((static_cast<int>(displayFreqHz) % 1000)) / 10;

    snprintf(buffer, bufferSize, "%ld.%02d", khz_part, hz_tens_part);
}

/**
 * @brief Beállítja a komponens szélességét dinamikusan
 */
void FreqDisplay::setWidth(uint16_t newWidth) {
    if (bounds.width != newWidth) {
        bounds.width = newWidth;
        needsFullClear = true; // Teljes háttér törlés szükséges
        markForRedraw();
    }
}
