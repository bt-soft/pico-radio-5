#include "StereoIndicator.h"

/**
 * @brief StereoIndicator konstruktor
 */
StereoIndicator::StereoIndicator(const Rect &bounds, const ColorScheme &colors) : UIComponent(bounds, colors) {
    isStereo = false;
    needsUpdate = true;
}

/**
 * @brief Stereo állapot beállítása
 * @param stereo true = STEREO (piros), false = MONO (kék)
 */
void StereoIndicator::setStereo(bool stereo) {
    if (isStereo != stereo) {
        isStereo = stereo;
        needsUpdate = true;
        markForRedraw();
    }
}

/**
 * @brief Komponens kirajzolása
 */
void StereoIndicator::draw() {
    if (!needsUpdate && !needsRedraw) {
        return;
    }

    // Háttér színek
    uint16_t bgColor = isStereo ? TFT_RED : TFT_BLUE;
    uint16_t textColor = TFT_WHITE;

    // Háttér kitöltése
    tft.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, bgColor);

    // Keret rajzolása
    tft.drawRect(bounds.x, bounds.y, bounds.width, bounds.height, TFT_WHITE);

    // Szöveg beállítása
    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(1);
    tft.setFreeFont();          // Alapértelmezett font
    tft.setTextDatum(MC_DATUM); // Middle Center

    // Szöveg kirajzolása
    const char *text = isStereo ? "STEREO" : "MONO";
    int16_t centerX = bounds.x + bounds.width / 2;
    int16_t centerY = bounds.y + bounds.height / 2;

    tft.drawString(text, centerX, centerY);

    needsUpdate = false;
    needsRedraw = false;
}
