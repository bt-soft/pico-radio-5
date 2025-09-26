#include "UITextBox.h"
#include "defines.h" // For DEBUG

UITextBox::UITextBox(const Rect &bounds, const String &initialText)
    : UIComponent(bounds), text(initialText), textColor(TFT_WHITE), bgColor(TFT_BLACK), textSize(2), textDatum(TL_DATUM), _sprite(&tft), _spriteCreated(false), maxCharsPerLine(40) {
    // Sprite létrehozása a konstruktorban
    if (bounds.width > 0 && bounds.height > 0) {
        _sprite.setColorDepth(16);
        _spriteCreated = _sprite.createSprite(bounds.width, bounds.height);
        if (!_spriteCreated) {
            DEBUG("UITextBox: Sprite creation failed!\n");
        }
    }
}

UITextBox::~UITextBox() {
    if (_spriteCreated) {
        _sprite.deleteSprite();
    }
}

void UITextBox::setBounds(const Rect &newBounds) {
    UIComponent::setBounds(newBounds);
    if (_spriteCreated) {
        _sprite.deleteSprite();
        _spriteCreated = false;
    }
    if (bounds.width > 0 && bounds.height > 0) {
        _sprite.setColorDepth(16);
        _spriteCreated = _sprite.createSprite(bounds.width, bounds.height);
        if (!_spriteCreated) {
            DEBUG("UITextBox: Sprite recreation failed!\n");
        }
    }
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
    if (_spriteCreated)
        _sprite.setTextColor(textColor, bgColor);
    markForRedraw();
}

void UITextBox::setTextSize(uint8_t size) {
    textSize = size;
    if (_spriteCreated)
        _sprite.setTextSize(textSize);
    markForRedraw();
}

void UITextBox::setTextDatum(uint8_t datum) {
    textDatum = datum;
    if (_spriteCreated)
        _sprite.setTextDatum(datum);
    markForRedraw();
}

void UITextBox::setMaxCharsPerLine(int maxChars) {
    maxCharsPerLine = maxChars;
    markForRedraw();
}

void UITextBox::draw() {
    if (!needsRedraw || !_spriteCreated) {
        return;
    }

    // 1. Sprite háttér törlése
    _sprite.fillSprite(bgColor);
    _sprite.drawRect(0, 0, bounds.width, bounds.height, TFT_DARKGREY); // Keret a sprite-on belül

    // 2. Szövegbeállítások a sprite-on
    _sprite.setTextColor(textColor, bgColor);
    _sprite.setTextDatum(TL_DATUM); // A szótördeléshez a bal felső igazítás a legkönnyebb
    _sprite.setFreeFont();
    _sprite.setTextSize(textSize);

    // 3. Karakterszám alapú sortörés és rajzolás a sprite-ra
    if (!text.isEmpty()) {
        int16_t cursorX = 5; // Belső padding
        int16_t cursorY = 5;
        int16_t lineHeight = _sprite.fontHeight() + 2; // Kis extra térköz a sorok között
        String currentLine = "";

        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);

            if (c == '\n') {
                // Explicit új sor
                _sprite.drawString(currentLine, cursorX, cursorY);
                cursorY += lineHeight;
                currentLine = "";

                // Ha kilógnánk a sprite-ból, stop
                if (cursorY + lineHeight > bounds.height) {
                    break;
                }
            } else {
                // Karakter hozzáadása a sorhoz
                currentLine += c;

                // Ha elértük a karakterlimitet, új sort kezdünk
                if (currentLine.length() >= maxCharsPerLine) {
                    _sprite.drawString(currentLine, cursorX, cursorY);
                    cursorY += lineHeight;
                    currentLine = "";

                    // Ha kilógnánk a sprite-ból, stop
                    if (cursorY + lineHeight > bounds.height) {
                        break;
                    }
                }
            }
        }

        // Az utolsó sor kirajzolása (ha van és még elfér)
        if (!currentLine.isEmpty() && cursorY < bounds.height) {
            _sprite.drawString(currentLine, cursorX, cursorY);
        }
    }

    // 4. Sprite kirakása a képernyőre
    _sprite.pushSprite(bounds.x, bounds.y);

    needsRedraw = false;
}