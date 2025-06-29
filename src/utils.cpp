#include "utils.h"

#include "Config.h" // Szükséges a config objektum eléréséhez
#include "defines.h"
#include "pins.h" // PIN_TFT_BACKGROUND_LED konstansért

namespace Utils {

/**
 * Várakozás a soros port megnyitására
 * @param tft a TFT kijelző példánya
 */
void debugWaitForSerial(TFT_eSPI &tft) {
#ifdef __DEBUG
    beepError();
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Nyisd meg a soros portot!", 0, 0);
    while (!Serial) {
    }
    tft.fillScreen(TFT_BLACK);
    beepTick();
#endif
}

/**
 * TFT érintőképernyő kalibráció
 * @param tft TFT kijelző példánya
 * @param calData kalibrációs adatok
 */
void tftTouchCalibrate(TFT_eSPI &tft, uint16_t (&calData)[5]) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(2);
    const char *txt = "TFT touch kalibrácio kell!\n";
    tft.setCursor((tft.width() - tft.textWidth(txt)) / 2, tft.height() / 2 - 60);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.println(txt);

    tft.setTextSize(1);
    txt = "Erintsd meg a jelzett helyeken a sarkokat!\n";
    tft.setCursor((tft.width() - tft.textWidth(txt)) / 2, tft.height() / 2 + 20);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.println(txt);

    // TFT_eSPI 'bóti' kalibráció indítása
    tft.calibrateTouch(calData, TFT_YELLOW, TFT_BLACK, 15);

    txt = "Kalibracio befejezodott!";
    tft.fillScreen(TFT_BLACK);
    tft.setCursor((tft.width() - tft.textWidth(txt)) / 2, tft.height() / 2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.println(txt);

    DEBUG("// Használd ezt a kalibrációs kódot a setup()-ban:\n");
    DEBUG("  uint16_t calData[5] = { ");
    for (uint8_t i = 0; i < 5; i++) {
        DEBUG("%d", calData[i]);
        if (i < 4) {
            DEBUG(", ");
        }
    }
    DEBUG(" };\n");
    DEBUG("  tft.setTouch(calData);\n");
}

/**
 * Hiba megjelenítése a képrnyőn
 */
void displayException(TFT_eSPI &tft, const char *msg) {
    int16_t screenWidth = tft.width();
    int16_t screenHeight = tft.height();

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, screenWidth, screenHeight,
                 TFT_RED); // 2px széles piros keret
    tft.drawRect(1, 1, screenWidth - 2, screenHeight - 2, TFT_RED);

    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(MC_DATUM); // Középre igazítás
    tft.setTextSize(2);

    tft.drawString("HIBA!", screenWidth / 2, screenHeight / 3);
    tft.setTextSize(1);
    tft.drawString(msg, screenWidth / 2, screenHeight / 2);

    DEBUG(msg);
    // Végtelen ciklusba esünk  és a belső LED villogtatásával jelezzük hogy hiba
    // van
    while (true) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);
    }
}

/**
 *  Pitty hangjelzés
 */
void beepTick() {
    // Csak akkor csipogunk, ha a beeper engedélyezve van
    if (!config.data.beeperEnabled)
        return;
    tone(PIN_BEEPER, 800);
    delay(10);
    noTone(PIN_BEEPER);
}

/**
 * Hiba jelzés
 */
void beepError() {
    // Csak akkor csipogunk, ha a beeper engedélyezve van
    if (!config.data.beeperEnabled)
        return;
    tone(PIN_BEEPER, 500);
    delay(100);
    tone(PIN_BEEPER, 500);
    delay(100);
    tone(PIN_BEEPER, 500);
    delay(100);
    noTone(PIN_BEEPER);
}

/**
 * @brief TFT háttérvilágítás beállítása DC/PWM módban
 * 255-ös értéknél ténylegesen DC-t ad ki (digitalWrite HIGH),
 * más értékeknél PWM-et használ (analogWrite)
 * @param brightness Fényerő érték (0-255)
 */
void setTftBacklight(uint8_t brightness) {
    if (brightness == 255) {
        // Maximum fényerőnél: tiszta DC (digitalWrite HIGH)
        digitalWrite(PIN_TFT_BACKGROUND_LED, HIGH);
    } else if (brightness == 0) {
        // Minimumnál: tiszta DC (digitalWrite LOW)
        digitalWrite(PIN_TFT_BACKGROUND_LED, LOW);
    } else {
        // Köztes értékeknél: PWM használata
        analogWrite(PIN_TFT_BACKGROUND_LED, brightness);
    }
}

/**
 * @brief Ellenőrzi, hogy egy C string egy adott indextől kezdve csak szóközöket
 * tartalmaz-e.
 */
bool isRemainingOnlySpaces(const char *str, uint16_t offset) {
    if (!str)
        return true;

    size_t len = strlen(str);
    if (offset >= len)
        return true;

    // Optimalizált ciklus: rögtön kilépünk ha nem szóközt találunk
    for (size_t i = offset; i < len; ++i) {
        if (str[i] != ' ')
            return false;
    }
    return true;
}

/**
 * @brief Összehasonlít két C stringet max n karakterig, a második string végén
 * lévő szóközöket figyelmen kívül hagyva.
 */
int strncmpIgnoringTrailingSpaces(const char *s1, const char *s2, size_t n) {
    if (n == 0)
        return 0;

    // s2 effektív hossza (szóközök nélkül a végén)
    size_t len2 = strlen(s2);
    while (len2 > 0 && s2[len2 - 1] == ' ')
        len2--;

    for (size_t i = 0; i < n; i++) {
        bool end1 = (s1[i] == '\0');
        bool end2 = (i >= len2);

        if (end1 && end2)
            return 0;
        if (end1 || end2)
            return end1 ? -1 : 1;
        if (s1[i] != s2[i])
            return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
    return 0;
}

/**
 * @brief Eltávolítja a C string végéről a szóközöket (in-place).
 */
void trimTrailingSpaces(char *str) {
    if (!str)
        return;

    int len = strlen(str);
    while (len > 0 && str[len - 1] == ' ') {
        str[--len] = '\0';
    }
}

/**
 * @brief Eltávolítja a C string elejéről a szóközöket (in-place).
 */
void trimLeadingSpaces(char *str) {
    if (!str)
        return;

    int spaces = 0;
    while (str[spaces] == ' ')
        spaces++;

    if (spaces > 0) {
        int len = strlen(str);
        // Hatékony memória másolás
        memmove(str, str + spaces, len - spaces + 1);
    }
}

/**
 * @brief Eltávolítja a C string elejéről és végéről a szóközöket (in-place).
 */
void trimSpaces(char *str) {
    trimLeadingSpaces(str);
    trimTrailingSpaces(str);
}

/**
 * @brief CRC16 számítás (CCITT algoritmus)
 * Használhatnánk a CRC könyvtárat is, de itt saját implementációt adunk
 *
 * @param data Adat pointer
 * @param length Adat hossza bájtokban
 * @return Számított CRC16 érték
 */
uint16_t calcCRC16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    return crc;
}

} // namespace Utils
