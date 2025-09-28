#include "UITextBox.h"
#include "Config.h"  // For config.data.beeperEnabled
#include "defines.h" // For DEBUG
#include "utils.h"   // For beepTick()

constexpr uint8_t MAX_TEXTBOX_LINES = 10; // Max sorok száma a szövegdobozban

// ==========================================
// UITextBox::UITextBox
// ==========================================
/**
 * @brief Konstruktor, amely inicializálja a szövegdobozt.
 * @details Létrehozza a sprite-ot és beállítja az alapértelmezett értékeket.
 * @param bounds A szövegdoboz méretei.
 * @param initialText A kezdeti szöveg.
 */
UITextBox::UITextBox(const Rect &bounds, const String &initialText)
    : UIComponent(bounds), text(initialText), textColor(TFT_WHITE), bgColor(TFT_BLACK), textSize(2), textDatum(TL_DATUM), _sprite(&tft), _spriteCreated(false), maxCharsPerLine(40), scrollOffset(0),
      longPressHandled(false) {
    // Sprite létrehozása a konstruktorban
    if (bounds.width > 0 && bounds.height > 0) {
        _sprite.setColorDepth(16);
        _spriteCreated = _sprite.createSprite(bounds.width, bounds.height);
        if (!_spriteCreated) {
            DEBUG("UITextBox: Sprite creation failed!\n");
        }
    }
}

// ==========================================
// UITextBox::~UITextBox
// ==========================================
/**
 * @brief Destruktor, amely felszabadítja az erőforrásokat.
 * @details Törli a sprite-ot, ha az létre lett hozva.
 */
UITextBox::~UITextBox() {
    if (_spriteCreated) {
        _sprite.deleteSprite();
    }
}

// ==========================================
// UITextBox::setBounds
// ==========================================
/**
 * @brief Beállítja a szövegdoboz méreteit.
 * @details Újra létrehozza a sprite-ot az új méretek alapján.
 * @param newBounds Az új méretek.
 */
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

// ==========================================
// UITextBox::setText
// ==========================================
/**
 * @brief Beállítja a szövegdoboz szövegét.
 * @details Ha a szöveg változik, újrarajzolást kér és resetálja a scroll pozíciót.
 * @param newText Az új szöveg.
 */
void UITextBox::setText(const String &newText) {
    if (text != newText) {
        text = newText;
        // Hosszú érintés resetelése új szöveg esetén
        if (longPressHandled) {
            scrollOffset = 0; // Scroll reset csak ha új szöveg jött
        }
        markForRedraw(); // Újrarajzolás kérése, ha a szöveg változik
    }
}

// ==========================================
// UITextBox::getText
// ==========================================
/**
 * @brief Visszaadja a szövegdoboz aktuális szövegét.
 * @return A szöveg.
 */
String UITextBox::getText() const { return text; }

// ==========================================
// UITextBox::setTextColor
// ==========================================
/**
 * @brief Beállítja a szöveg és a háttér színét.
 * @param fg A szöveg színe.
 * @param bg A háttér színe.
 */
void UITextBox::setTextColor(uint16_t fg, uint16_t bg) {
    textColor = fg;
    bgColor = bg;
    if (_spriteCreated)
        _sprite.setTextColor(textColor, bgColor);
    markForRedraw();
}

// ==========================================
// UITextBox::setTextSize
// ==========================================
/**
 * @brief Beállítja a szöveg méretét.
 * @param size Az új szövegméret.
 */
void UITextBox::setTextSize(uint8_t size) {
    textSize = size;
    if (_spriteCreated)
        _sprite.setTextSize(textSize);
    markForRedraw();
}

// ==========================================
// UITextBox::setTextDatum
// ==========================================
/**
 * @brief Beállítja a szöveg igazítását.
 * @param datum Az igazítás típusa.
 */
void UITextBox::setTextDatum(uint8_t datum) {
    textDatum = datum;
    if (_spriteCreated)
        _sprite.setTextDatum(datum);
    markForRedraw();
}

// ==========================================
// UITextBox::setMaxCharsPerLine
// ==========================================
/**
 * @brief Beállítja a soronkénti maximális karakterszámot.
 * @param maxChars A maximális karakterszám.
 */
void UITextBox::setMaxCharsPerLine(int maxChars) {
    maxCharsPerLine = maxChars;
    markForRedraw();
}

// ==========================================
// UITextBox::draw
// ==========================================
/**
 * @brief Kirajzolja a szövegdobozt a képernyőre.
 * @details A szöveget sortöréssel jeleníti meg a sprite-on belül, automatikus scrolling-gal.
 */
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
        int16_t lineHeight = _sprite.fontHeight() + 2;           // Kis extra térköz a sorok között
        int maxVisibleLines = (bounds.height - 10) / lineHeight; // Hány sor fér el

        // Szöveg sorokra törése
        String lines[MAX_TEXTBOX_LINES]; // Max 10 sor tárolása
        int lineCount = 0;
        String currentLine = "";

        // Először fel kell bontani a teljes szöveget sorokra
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);

            if (c == '\n') {
                // Explicit új sor
                if (lineCount < MAX_TEXTBOX_LINES) {
                    lines[lineCount++] = currentLine;
                }
                currentLine = "";
            } else {
                // Karakter hozzáadása a sorhoz
                currentLine += c;

                // Ha elértük a karakterlimitet, új sort kezdünk
                if (currentLine.length() >= maxCharsPerLine) {
                    if (lineCount < MAX_TEXTBOX_LINES) {
                        lines[lineCount++] = currentLine;
                    }
                    currentLine = "";
                }
            }
        }

        // Az utolsó sor hozzáadása (ha van)
        if (!currentLine.isEmpty() && lineCount < MAX_TEXTBOX_LINES) {
            lines[lineCount++] = currentLine;
        }

        // Automatikus scroll: ha túl sok sor van, scrolloljunk le
        if (lineCount > maxVisibleLines) {
            scrollOffset = lineCount - maxVisibleLines;
        } else {
            scrollOffset = 0;
        }

        // Sorok kirajzolása a scroll offset-tel
        int startLine = scrollOffset;
        int endLine = min(startLine + maxVisibleLines, lineCount);

        for (int i = startLine; i < endLine; i++) {
            _sprite.drawString(lines[i], cursorX, cursorY + (i - startLine) * lineHeight);
        }
    }

    // 4. Sprite kirakása a képernyőre
    _sprite.pushSprite(bounds.x, bounds.y);

    needsRedraw = false;
}

// ==========================================
// UITextBox::loop
// ==========================================
/**
 * @brief Ellenőrzi a hosszú érintést és végrehajtja a megfelelő műveleteket.
 * @details Ha a felhasználó 2 másodpercig nyomva tartja a szövegdobozt, a tartalom törlődik,
 * és egy hangjelzés hallható, ha a konfiguráció engedélyezi.
 */
void UITextBox::loop() {

    // Alap osztály loop-ja (ha van)
    UIComponent::loop();

    // Hosszú érintés ellenőrzése
    if (pressed && !longPressHandled) {
        uint32_t pressDuration = millis() - touchDownTime;
        if (pressDuration >= 2000) { // 2 másodperc
            // Hosszú érintés történt - szöveg törlése és hangjelzés
            if (!text.isEmpty()) {
                setText(""); // Szöveg törlése
            }
            // Hangjelzés, ha engedélyezve van
            if (config.data.beeperEnabled) {
                Utils::beepTick();
            }

            longPressHandled = true; // Csak egyszer hajtjuk végre
        }

        DEBUG("UITextBox: Long press detected, duration: %lu ms\n", pressDuration);
    }
}

// ==========================================
// UITextBox::onTouchDown
// ==========================================
/**
 * @brief Kezeli az érintés lenyomását.
 * @details Reseteli a hosszú érintés flag-et, hogy újra lehessen érzékelni.
 * @param event Az érintés esemény adatai.
 */
void UITextBox::onTouchDown(const TouchEvent &event) {
    // Alapértelmezett touchDown kezelés
    UIComponent::onTouchDown(event);

    // Reset a hosszú érintés kezeléshez
    longPressHandled = false;

    // Hangjelzés, ha engedélyezve van
    if (config.data.beeperEnabled) {
        Utils::beepTick();
    }
}

// ==========================================
// UITextBox::onTouchUp
// ==========================================
/**
 * @brief Kezeli az érintés felengedését.
 * @details Reseteli a hosszú érintés flag-et, hogy újra lehessen érzékelni.
 * @param event Az érintés esemény adatai.
 */
void UITextBox::onTouchUp(const TouchEvent &event) {
    // Alapértelmezett touchUp kezelés
    UIComponent::onTouchUp(event);

    // Reset a hosszú érintés kezeléshez
    longPressHandled = false;
}

// ==========================================
// UITextBox::onTouchCancel
// ==========================================
/**
 * @brief Kezeli az érintés megszakítását.
 * @details Reseteli a hosszú érintés flag-et, hogy újra lehessen érzékelni.
 * @param event Az érintés esemény adatai.
 */
void UITextBox::onTouchCancel(const TouchEvent &event) {
    // Alapértelmezett touchCancel kezelés
    UIComponent::onTouchCancel(event);

    // Reset a hosszú érintés kezeléshez
    longPressHandled = false;
}