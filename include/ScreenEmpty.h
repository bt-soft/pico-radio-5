#ifndef __SCREEN_EMPTYH
#define __SCREEN_EMPTYH

#include "UIButton.h"
#include "UIScreen.h"

class ScreenEmpty : public UIScreen {

  public:
    /**
     * @brief Konstruktor
     * @param tft TFT display referencia
     */
    ScreenEmpty() : UIScreen(SCREEN_NAME_EMPTY) {
        DEBUG("ScreenEmpty: Constructor called\n");
        layoutComponents();
    }
    virtual ~ScreenEmpty() = default;

    /**
     * @brief Rotary encoder eseménykezelés felülírása
     * @param event Rotary encoder esemény
     * @return true ha kezelte az eseményt, false egyébként
     */
    virtual bool handleRotary(const RotaryEvent &event) override {

        // Ha van aktív dialógus, akkor a szülő implementációnak adjuk át
        if (isDialogActive()) {
            return UIScreen::handleRotary(event);
        }

        return UIScreen::handleRotary(event);
    }

    /**
     * @brief Loop hívás felülírása
     * animációs vagy egyéb saját logika végrehajtására
     * @note Ez a metódus nem hívja meg a gyerek komponensek loop-ját, csak saját logikát tartalmaz.
     */
    virtual void handleOwnLoop() override {}

    /**
     * @brief Kirajzolja a képernyő saját tartalmát
     */
    virtual void drawContent() override {
        // Szöveg középre igazítása
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_COLOR_BACKGROUND);
        tft.setTextSize(3);

        // Képernyő cím kirajzolása
        tft.drawString(SCREEN_NAME_EMPTY, ::SCREEN_W / 2, ::SCREEN_H / 2 - 20);

        // Információs szöveg
        tft.setTextSize(1);
        tft.drawString("ScreenEmpty  for debugging", ::SCREEN_W / 2, ::SCREEN_W / 2 + 20);
    }

  private:
    /**
     * @brief UI komponensek létrehozása és elhelyezése
     */
    void layoutComponents() {}
};

#endif // __SCREEN_EMPTYH