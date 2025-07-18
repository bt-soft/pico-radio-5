#ifndef __UI_TEXT_BOX_H
#define __UI_TEXT_BOX_H

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

  public:
    UITextBox(const Rect &bounds, const String &initialText);
    virtual ~UITextBox();

    void setText(const String &newText);
    String getText() const;

    void setTextColor(uint16_t fg, uint16_t bg);
    void setTextSize(uint8_t size);
    void setTextDatum(uint8_t datum);

    virtual void setBounds(const Rect &newBounds) override;
    virtual void draw() override;
};

#endif // __UI_TEXT_BOX_H