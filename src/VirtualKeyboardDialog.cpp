#include "VirtualKeyboardDialog.h"

VirtualKeyboardDialog::VirtualKeyboardDialog(UIScreen *parent, const char *title, const String &initialText, uint8_t maxLength, OnTextChangedCallback onChanged)
    : UIDialogBase(parent, title, Rect(-1, -1, 350, 260)), currentText(initialText), maxTextLength(maxLength), textChangedCallback(onChanged), lastCursorBlink(millis()) {

    // Input mező pozíció számítása
    inputRect = Rect(bounds.x + INPUT_MARGIN, bounds.y + getHeaderHeight() + INPUT_MARGIN, bounds.width - (INPUT_MARGIN * 2), INPUT_HEIGHT);

    // Billentyűzet terület pozíció számítása
    keyboardRect = Rect(bounds.x + 5, inputRect.y + inputRect.height + 10, bounds.width - 10, bounds.height - getHeaderHeight() - INPUT_HEIGHT - 60);

    createKeyboard();
}

VirtualKeyboardDialog::~VirtualKeyboardDialog() { keyButtons.clear(); }

void VirtualKeyboardDialog::createKeyboard() {
    keyButtons.clear();
    keyLabelCount = 0;

    uint8_t buttonId = 100; // Kezdő ID

    // Karakteres gombok létrehozása
    uint16_t startY = keyboardRect.y;

    for (uint8_t row = 0; row < KEYBOARD_ROWS; ++row) {
        const char *rowKeys = keyboardLayout[row];
        uint8_t keysInRow = strlen(rowKeys);

        uint16_t rowWidth = keysInRow * KEY_WIDTH + (keysInRow - 1) * KEY_SPACING;
        uint16_t startX = keyboardRect.x + (keyboardRect.width - rowWidth) / 2;

        uint16_t currentX = startX;
        uint16_t currentY = startY + row * (KEY_HEIGHT + KEY_SPACING);
        for (uint8_t col = 0; col < keysInRow; ++col) {
            char keyChar = rowKeys[col]; // Felirat tárolása a char tömbben
            if (keyLabelCount < 50) {
                keyLabelStorage[keyLabelCount][0] = keyChar;
                keyLabelStorage[keyLabelCount][1] = '\0';

                auto keyButton = std::make_shared<UIButton>(         //
                    buttonId++,                                      //
                    Rect(currentX, currentY, KEY_WIDTH, KEY_HEIGHT), //
                    keyLabelStorage[keyLabelCount],                  //
                    UIButton::ButtonType::Pushable,                  //
                    [this, keyChar](const UIButton::ButtonEvent &event) {
                        if (event.state == UIButton::EventButtonState::Clicked) {
                            handleKeyPress(keyChar);
                        }
                    });

                keyButtons.push_back(keyButton);
                addChild(keyButton);
                keyLabelCount++;
            }

            currentX += KEY_WIDTH + KEY_SPACING;
        }
    } // Speciális gombok létrehozása az utolsó sor alatt
    uint16_t specialY = startY + KEYBOARD_ROWS * (KEY_HEIGHT + KEY_SPACING) + 5;

    // Speciális gombok méretei
    uint16_t shiftWidth = 50;
    uint16_t spaceWidth = 80;
    uint16_t backspaceWidth = 40;
    uint16_t clearWidth = 40;
    uint16_t specialSpacing = 5;

    // Teljes sor szélessége
    uint16_t specialRowWidth = shiftWidth + spaceWidth + backspaceWidth + clearWidth + (3 * specialSpacing); // Középre igazítás
    uint16_t specialStartX = keyboardRect.x + (keyboardRect.width - specialRowWidth) / 2;

    // Shift gomb
    shiftButton = std::make_shared<UIButton>(                  //
        buttonId++,                                            //
        Rect(specialStartX, specialY, shiftWidth, KEY_HEIGHT), //
        "Shift",                                               //
        UIButton::ButtonType::Toggleable,                      //
        UIButton::ButtonState::Off,                            //
        [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::On || event.state == UIButton::EventButtonState::Off) {
                shiftActive = !shiftActive;
                // Gomb állapot beállítása
                shiftButton->setButtonState(shiftActive ? UIButton::ButtonState::On : UIButton::ButtonState::Off);
                // A gomb automatikusan frissül a setButtonState() híváskor
                updateButtonLabels();
            }
        });
    addChild(shiftButton);

    // Space gomb
    uint16_t spaceX = specialStartX + shiftWidth + specialSpacing;
    spaceButton = std::make_shared<UIButton>(           //
        buttonId++,                                     //
        Rect(spaceX, specialY, spaceWidth, KEY_HEIGHT), //
        "Space",                                        //
        UIButton::ButtonType::Pushable, [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                handleKeyPress(' ');
            }
        });
    addChild(spaceButton);

    // Backspace gomb
    uint16_t backspaceX = specialStartX + shiftWidth + spaceWidth + (2 * specialSpacing);

    backspaceButton = std::make_shared<UIButton>(               //
        buttonId++,                                             //
        Rect(backspaceX, specialY, backspaceWidth, KEY_HEIGHT), //
        "<--",                                                  //
        UIButton::ButtonType::Pushable,                         //
        [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                handleSpecialKey("backspace");
            }
        });
    addChild(backspaceButton);

    // Clear gomb
    uint16_t clearX = specialStartX + shiftWidth + spaceWidth + backspaceWidth + (3 * specialSpacing);
    clearButton = std::make_shared<UIButton>( //
        buttonId++,                           //
        Rect(clearX, specialY, clearWidth, KEY_HEIGHT),
        "Clr",                          //
        UIButton::ButtonType::Pushable, //
        [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                handleSpecialKey("clear");
            }
        });
    addChild(clearButton); // OK és Cancel gombok a speciális sor alatt
    uint16_t okCancelY = specialY + KEY_HEIGHT + 8;
    uint16_t cancelWidth = 75; // Cancel gomb szélesebb
    uint16_t okWidth = 60;     // OK gomb normál szélesség
    uint16_t buttonSpacing = 10;
    uint16_t totalButtonsWidth = cancelWidth + okWidth + buttonSpacing;
    uint16_t buttonsStartX = keyboardRect.x + (keyboardRect.width - totalButtonsWidth) / 2;

    // Cancel gomb (bal oldalon, szélesebb)
    cancelButton = std::make_shared<UIButton>(                   //
        buttonId++,                                              //
        Rect(buttonsStartX, okCancelY, cancelWidth, KEY_HEIGHT), //
        "Cancel",                                                //
        UIButton::ButtonType::Pushable, [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                close(UIDialogBase::DialogResult::Rejected);
            }
        });
    addChild(cancelButton);

    // OK gomb (jobb oldalon)
    okButton = std::make_shared<UIButton>(                                                 //
        buttonId++,                                                                        //
        Rect(buttonsStartX + cancelWidth + buttonSpacing, okCancelY, okWidth, KEY_HEIGHT), //
        "OK",                                                                              //
        UIButton::ButtonType::Pushable,                                                    //
        [this](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                close(UIDialogBase::DialogResult::Accepted);
            }
        });
    addChild(okButton);

    // OK gomb állapot beállítása a kezdeti szöveg alapján
    if (okButton) {
        bool shouldEnable = currentText.length() >= 3;
        okButton->setEnabled(shouldEnable);
    }
}

void VirtualKeyboardDialog::drawSelf() {
    // Szülő osztály rajzolása (keret, cím, háttér, 'X' gomb)
    UIDialogBase::drawSelf();

    // Input mező rajzolása
    drawInputField();
}

void VirtualKeyboardDialog::drawInputField() {

    // Input mező háttér
    tft.fillRect(inputRect.x, inputRect.y, inputRect.width, inputRect.height, TFT_BLACK);
    tft.drawRect(inputRect.x, inputRect.y, inputRect.width, inputRect.height, TFT_WHITE);

    // Szöveg kirajzolása
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextSize(1);

    String displayText = currentText;
    if (displayText.length() > 18) { // Ha túl hosszú, csak a végét mutatjuk
        displayText = "..." + displayText.substring(displayText.length() - 15);
    }

    tft.drawString(displayText, inputRect.x + 5, inputRect.y + inputRect.height / 2);

    // Kurzor rajzolása
    if (cursorVisible) {
        drawCursor();
    }
}

void VirtualKeyboardDialog::drawCursor() {
    // Kurzor pozíció számítása
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextSize(1);

    String displayText = currentText;
    if (displayText.length() > 18) {
        displayText = "..." + displayText.substring(displayText.length() - 15);
    }

    uint16_t textWidth = tft.textWidth(displayText);
    uint16_t cursorX = inputRect.x + 5 + textWidth;
    uint16_t cursorY = inputRect.y + 3;
    uint16_t cursorHeight = inputRect.height - 6;

    if (cursorX < inputRect.x + inputRect.width - 3) {
        tft.drawFastVLine(cursorX, cursorY, cursorHeight, TFT_WHITE);
    }
}

void VirtualKeyboardDialog::handleKeyPress(char key) {

    if (currentText.length() >= maxTextLength) {
        return; // Max hossz elérve
    }
    char actualKey = getKeyChar(key, shiftActive);
    currentText += actualKey;

    // Shift állapot megmarad - csak manuális kikapcsolással kapcsol ki
    notifyTextChanged();
    updateOkButtonState();
    // NE hívjuk a markForRedraw()-t, csak az input mezőt rajzoljuk újra
    // markForRedraw(); // Ez okozza a gombok eltűnését

    // Azonnali újrarajzolás az input mezőnek
    drawInputField();
}

void VirtualKeyboardDialog::handleSpecialKey(const String &keyType) {
    if (keyType == "backspace") {
        if (currentText.length() > 0) {
            currentText.remove(currentText.length() - 1);
            notifyTextChanged();
            updateOkButtonState();
            // NE hívjuk a markForRedraw()-t, csak az input mezőt rajzoljuk újra
            // markForRedraw(); // Ez okozza a gombok eltűnését

            // Azonnali újrarajzolás az input mezőnek
            drawInputField();
        }
    } else if (keyType == "clear") {
        if (currentText.length() > 0) {
            currentText = "";
            notifyTextChanged();
            updateOkButtonState();
            // NE hívjuk a markForRedraw()-t, csak az input mezőt rajzoljuk újra
            // markForRedraw(); // Ez okozza a gombok eltűnését

            // Azonnali újrarajzolás az input mezőnek
            drawInputField();
        }
    }
}

void VirtualKeyboardDialog::updateButtonLabels() {
    // Az eredeti billentyűzet layout alapján frissítjük a gombokat
    size_t buttonIndex = 0;

    for (uint8_t row = 0; row < KEYBOARD_ROWS && buttonIndex < keyButtons.size(); ++row) {
        const char *rowKeys = keyboardLayout[row];
        uint8_t keysInRow = strlen(rowKeys);

        for (uint8_t col = 0; col < keysInRow && buttonIndex < keyButtons.size(); ++col) {
            char baseChar = rowKeys[col];
            char newChar = getKeyChar(baseChar, shiftActive);
            keyLabelStorage[buttonIndex][0] = newChar;
            keyLabelStorage[buttonIndex][1] = '\0';
            keyButtons[buttonIndex]->setLabel(keyLabelStorage[buttonIndex]); // A setLabel() automatikusan hívja a markForRedraw-t, ha a címke változik
            buttonIndex++;
        }
    }
}

char VirtualKeyboardDialog::getKeyChar(char baseChar, bool shifted) {

    if (isalpha(baseChar)) {
        char result = shifted ? toupper(baseChar) : tolower(baseChar);
        return result;
    }

    // Speciális karakterek shift módban
    if (shifted) {
        switch (baseChar) {
            case '1':
                return '!';
            case '2':
                return '@';
            case '3':
                return '#';
            case '4':
                return '$';
            case '5':
                return '%';
            case '6':
                return '^';
            case '7':
                return '&';
            case '8':
                return '*';
            case '9':
                return '(';
            case '0':
                return ')';
            case '-':
                return '_';
            case '.':
                return ':';
            default:
                return baseChar;
        }
    }

    return baseChar;
}

void VirtualKeyboardDialog::setText(const String &text) {
    currentText = text;
    if (currentText.length() > maxTextLength) {
        currentText = currentText.substring(0, maxTextLength);
    }
    notifyTextChanged();
    updateOkButtonState();
    markForRedraw();

    // Azonnali újrarajzolás az input mezőnek
    drawInputField();
}

void VirtualKeyboardDialog::notifyTextChanged() {
    if (textChangedCallback) {
        textChangedCallback(currentText);
    }
}

void VirtualKeyboardDialog::handleOwnLoop() {
    // Kurzor villogtatás
    unsigned long now = millis();
    if (now - lastCursorBlink >= CURSOR_BLINK_INTERVAL) {
        cursorVisible = !cursorVisible;
        lastCursorBlink = now;

        // Csak a kurzor területét rajzoljuk újra
        redrawCursorArea();
    }
}

void VirtualKeyboardDialog::redrawCursorArea() {
    // Kurzor pozíció számítása
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextSize(1);

    String displayText = currentText;
    if (displayText.length() > 18) {
        displayText = "..." + displayText.substring(displayText.length() - 15);
    }

    uint16_t textWidth = tft.textWidth(displayText);
    uint16_t cursorX = inputRect.x + 5 + textWidth;
    uint16_t cursorY = inputRect.y + 3;
    uint16_t cursorHeight = inputRect.height - 6;

    if (cursorX < inputRect.x + inputRect.width - 3) {
        // Kurzor területének törlése (fekete háttér)
        tft.fillRect(cursorX, cursorY, 2, cursorHeight, TFT_BLACK);

        // Kurzor rajzolása csak ha látható
        if (cursorVisible) {
            tft.drawFastVLine(cursorX, cursorY, cursorHeight, TFT_WHITE);
        }
    }
}

void VirtualKeyboardDialog::updateOkButtonState() {
    if (okButton) {
        bool shouldEnable = currentText.length() >= 3;
        okButton->setEnabled(shouldEnable);
        // A gomb automatikusan frissül a setEnabled() híváskor
    }
}