#pragma once

#include "UIComponent.h"

/**
 * @brief STEREO/MONO jelző komponens FM rádióhoz
 * @details Egyszerű komponens, ami STEREO (piros háttér) vagy MONO (kék háttér) feliratot jelenít meg
 */
class StereoIndicator : public UIComponent {
  private:
    bool isStereo = false;
    bool needsUpdate = true;

  public:
    /**
     * @brief Konstruktor
     * @param tft TFT display referencia
     * @param bounds Komponens pozíciója és mérete
     * @param colors Színséma (opcionális)
     */
    StereoIndicator(const Rect &bounds, const ColorScheme &colors = ColorScheme::defaultScheme());

    /**
     * @brief Stereo állapot beállítása
     * @param stereo true = STEREO (piros), false = MONO (kék)
     */
    void setStereo(bool stereo);

    /**
     * @brief Jelenlegi stereo állapot lekérdezése
     * @return true ha STEREO, false ha MONO
     */
    bool getStereo() const { return isStereo; } // UIComponent interface implementáció
    virtual void draw() override;
    virtual bool handleTouch(const TouchEvent &event) override { return false; }
    virtual bool handleRotary(const RotaryEvent &event) override { return false; }
};
