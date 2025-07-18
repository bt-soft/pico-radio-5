#include "UITextBox.h"

UITextBox::UITextBox(const Rect &bounds, const String &initialText) : UIComponent(bounds), text(initialText), textColor(TFT_WHITE), bgColor(TFT_BLACK), textSize(2), textDatum(TL_DATUM) {
    // Kezdeti állapot
}

void UITextBox::setText(const String &newText) {
    if (text != newText) {
        text = newText;
        markForRedraw(); // Újrarajzolás kérése, ha a szöveg változik
    }
}

String UITextBox::getText() const { return text; }

void UITextBox::setTextColor(uint16_t fg, uint16_t bg) {
    textColor = fg;
    bgColor = bg;
    markForRedraw();
}

void UITextBox::setTextSize(uint8_t size) {
    textSize = size;
    markForRedraw();
}

void UITextBox::setTextDatum(uint8_t datum) {
    textDatum = datum;
    markForRedraw();
}

void UITextBox::draw() {
    if (!needsRedraw) {
        return;
    }

    tft.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, bgColor);
    tft.drawRect(bounds.x, bounds.y, bounds.width, bounds.height, TFT_DARKGREY); // Keret

    tft.setTextColor(textColor, bgColor);
    tft.setTextDatum(textDatum);
    tft.setTextSize(textSize);
    tft.drawString(text, bounds.x + 5, bounds.y + 5); // 5px padding

    needsRedraw = false;
}