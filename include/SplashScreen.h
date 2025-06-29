#ifndef __SPLASHSCREEN_H
#define __SPLASHSCREEN_H

#include <Arduino.h>
#include <SI4735.h>
#include <TFT_eSPI.h>

#include "defines.h"

/**
 * @brief Splash képernyő a program indulásakor
 */
class SplashScreen {
  private:
    TFT_eSPI &tft;

    // Színek
    static const uint16_t BACKGROUND_COLOR = TFT_BLACK;
    static const uint16_t TITLE_COLOR = TFT_CYAN;
    static const uint16_t INFO_LABEL_COLOR = TFT_YELLOW;
    static const uint16_t INFO_VALUE_COLOR = TFT_WHITE;
    static const uint16_t VERSION_COLOR = TFT_GREEN;
    static const uint16_t AUTHOR_COLOR = TFT_MAGENTA;
    static const uint16_t BORDER_COLOR = TFT_BLUE;

    void drawBorder();
    void drawTitle();
    void drawBuildInfo();
    void drawProgramInfo();
    void drawProgressBar(uint8_t progress);

  public:
    /**
     * @brief Konstruktor
     * @param tft TFT_eSPI objektum referencia
     */
    SplashScreen(TFT_eSPI &tft);

    /**
     * @brief Splash képernyő megjelenítése
     * @param showProgress Progress bar megjelenítése
     * @param progressSteps Progress lépések száma (ha showProgress = true)
     */
    void show(bool showProgress = true, uint8_t progressSteps = 5);

    /**
     * @brief SI4735 információk kirajzolása
     */
    void drawSI4735Info(SI4735 &si4735);

    /**
     * @brief Progress frissítése
     * @param step Aktuális lépés
     * @param totalSteps Összes lépés
     * @param message Üzenet (opcionális)
     */
    void updateProgress(uint8_t step, uint8_t totalSteps, const char *message = nullptr);

    /**
     * @brief Splash képernyő eltűntetése
     */
    void hide();
};

#endif // __SPLASHSCREEN_H
