#ifndef __STATUSLINE_H
#define __STATUSLINE_H

#include "UIComponent.h"

/**
 * @file StatusLine.h
 * @brief StatusLine komponens - állapotsor megjelenítése a képernyő tetején
 * @details Ez a komponens 9 kis négyzetet jelenít meg vízszintesen, mindegyikben különböző állapotinformációkkal.
 * A komponens nem reagál touch és rotary eseményekre, csak információ megjelenítésére szolgál.
 */
class StatusLine : public UIComponent {
  private:
    // Négyzetek pozíciói és méretei
    struct StatusBox {
        uint16_t x;
        uint16_t width;
        uint16_t color;
    };
    bool stationInMemory; // Memória állomás jelző

#define STATUS_LINE_BOXES 10
    StatusBox statusBoxes[STATUS_LINE_BOXES];

    // Privát rajzoló metódusok
    void initializeBoxes();
    void drawBoxFrames();
    void clearBoxContent(uint8_t boxIndex);
    void drawTextInBox(uint8_t boxIndex, const char *text, uint16_t textColor);

    /**
     * @brief Reseteli a betűtípust és a szöveg méretét
     * @details Ez a metódus visszaállítja a TFT betűtípust az alapértelmezett értékre és a szöveg méretét 1-re.
     */
    void resetFont() {
        tft.setFreeFont();
        tft.setTextSize(1);
    }

  public:
    /**
     * @brief StatusLine konstruktor
     * @param x A komponens X koordinátája
     * @param y A komponens Y koordinátája
     * @param colors Színséma (opcionális, alapértelmezett színsémát használ ha nincs megadva)
     */
    StatusLine(int16_t x, int16_t y, const ColorScheme &colors = ColorScheme::defaultScheme());

    /**
     * @brief Virtuális destruktor
     */
    virtual ~StatusLine() = default;

    /**
     * @brief Touch esemény kezelése
     * @param event Touch esemény
     * @return false - ez a komponens nem kezeli a touch eseményeket
     */
    virtual bool handleTouch(const TouchEvent &event) override { return false; };

    /**
     * @brief Rotary encoder esemény kezelése
     * @param event Rotary esemény
     * @return false - ez a komponens nem kezeli a rotary eseményeket
     */
    virtual bool handleRotary(const RotaryEvent &event) override { return false; };

    /**
     * @brief A komponens kirajzolása
     * @details Megrajzolja az állapotsor kereteit és inicializálja a négyzeteket
     */
    virtual void draw() override;

    // Értékfrissítő metódusok - mindegyik a saját négyzetét frissíti
    void updateBfo();
    void updateAgc();
    void updateMode();
    void updateBandwidth();
    void updateBand();
    void updateStep();
    void updateAntCap();
    void updateTemperature();
    void updateVoltage();
    void updateStationInMemory(bool isInMemo);
};

#endif // __STATUSLINE_H