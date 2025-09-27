#pragma once

#include "UIComponent.h"
#include <Arduino.h>

/**
 * @brief Egyszerű szövegdoboz komponens a dekódolt szöveg megjelenítésére.
 */
class UITextBox : public UIComponent {
  private:
    String text;
    uint16_t textColor;
    uint16_t bgColor;
    uint8_t textSize;
    uint8_t textDatum; // A szöveg igazítása (word-wrap esetén korlátozottan használható)
    TFT_eSprite _sprite;
    bool _spriteCreated;
    int maxCharsPerLine; // Karakterek száma soronként
    int scrollOffset;    // Scrolling offset sorokban

    // Hosszú érintés kezelése
    static constexpr uint32_t LONG_PRESS_DURATION = 2000; // 2 másodperc
    bool longPressHandled = false;                        // Biztosítja, hogy csak egyszer történjen meg a törlés

  public:
    UITextBox(const Rect &bounds, const String &initialText);
    virtual ~UITextBox();

    void setText(const String &newText);
    String getText() const;

    void setTextColor(uint16_t fg, uint16_t bg);
    void setTextSize(uint8_t size);
    void setTextDatum(uint8_t datum);
    void setMaxCharsPerLine(int maxChars); // Karakterszám alapú sortörés beállítása

    virtual void setBounds(const Rect &newBounds) override;
    virtual void draw() override;
    virtual void loop() override; // Hosszú érintés ellenőrzéséhez
    virtual void onTouchDown(const TouchEvent &event) override;
    virtual void onTouchUp(const TouchEvent &event) override;
    virtual void onTouchCancel(const TouchEvent &event) override;
};
