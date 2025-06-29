#include "MultiButtonDialog.h"

/**
 * @brief MultiButtonDialog konstruktor.
 *
 * @param parentScreen A szülő UIScreen.
 * @param tft TFT_eSPI referencia.
 * @param title A dialógus címe.
 * @param message Az üzenet szövege.
 * @param options Gombok feliratainak tömbje.
 * @param numOptions A gombok száma.
 * @param buttonClickCb Gomb kattintás callback.
 * @param autoClose Automatikusan bezárja-e a dialógust gomb kattintáskor. * @param defaultButtonIndex Az alapértelmezett (kiemelt) gomb indexe (-1 = nincs).
 * @param disableDefaultButton Ha true, az alapértelmezett gomb le van tiltva; ha false, csak vizuálisan kiemelve.
 * @param ctorInputBounds A dialógus határai.
 * @param cs Színséma.
 */
MultiButtonDialog::MultiButtonDialog(UIScreen *parentScreen, const char *title, const char *message, const char *const *options, uint8_t numOptions, ButtonClickCallback buttonClickCb, bool autoClose,
                                     int defaultButtonIndex, bool disableDefaultButton, const Rect &ctorInputBounds, const ColorScheme &cs)
    : UIDialogBase(parentScreen, title, ctorInputBounds, cs), message(message), _userOptions(options), _numUserOptions(numOptions), _buttonClickCallback(buttonClickCb), _autoCloseOnButtonClick(autoClose),
      _defaultButtonIndex(defaultButtonIndex), _disableDefaultButton(disableDefaultButton) {

    // Dialógus tartalmának létrehozása
    createDialogContent();

    // Automatikus magasság számítása, ha szükséges
    if (ctorInputBounds.height == 0) {
        Rect refinedBounds = this->bounds;

        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextSize(1);
        int16_t textHeight = tft.fontHeight() * 2; // Becslés az üzenet területére

        uint16_t buttonAreaHeight = 0;
        if (_numUserOptions > 0) {
            buttonAreaHeight = UIButton::DEFAULT_BUTTON_HEIGHT + UIDialogBase::PADDING;
        }

        uint16_t contentHeight = UIDialogBase::PADDING + textHeight + UIDialogBase::PADDING + buttonAreaHeight;
        uint16_t requiredTotalHeight = getHeaderHeight() + contentHeight + UIDialogBase::PADDING;

        if (refinedBounds.height < requiredTotalHeight) {
            refinedBounds.height = requiredTotalHeight;
            // Ha az eredeti Y középre igazítást kért, újra középre igazítjuk
            if (ctorInputBounds.y == -1) {
                refinedBounds.y = (tft.height() - refinedBounds.height) / 2;
            }
            UIComponent::setBounds(refinedBounds);
        }
        tft.setFreeFont(); // Betűtípus visszaállítása
    }

    // Középre igazítás, ha szükséges
    Rect finalBounds = this->bounds;
    bool boundsChanged = false;

    if (ctorInputBounds.x == -1) {
        finalBounds.x = (tft.width() - finalBounds.width) / 2;
        boundsChanged = true;
    }

    if (boundsChanged) {
        UIComponent::setBounds(finalBounds);
    }

    // Tartalom elrendezése
    layoutDialogContent();
}

/**
 * @brief Létrehozza a dialógus tartalmát (gombokat).
 */
void MultiButtonDialog::createDialogContent() {
    _buttonDefs.clear();
    uint8_t buttonIdCounter = 1;
    if (_userOptions && _numUserOptions > 0) {
        for (uint8_t i = 0; i < _numUserOptions; ++i) {
            // Alapértelmezett gomb státusz meghatározása
            UIButton::ButtonState buttonState = UIButton::ButtonState::Off;
            bool isDefaultButton = (_defaultButtonIndex >= 0 && _defaultButtonIndex == static_cast<int>(i));
            if (isDefaultButton) {
                // Ha ez az alapértelmezett gomb és le van tiltva, akkor Disabled állapotot állítunk be
                if (_disableDefaultButton) {
                    buttonState = UIButton::ButtonState::Disabled;
                } else {
                    buttonState = UIButton::ButtonState::CurrentActive;
                }
            }
            _buttonDefs.push_back({static_cast<uint8_t>(buttonIdCounter + i), _userOptions[i], UIButton::ButtonType::Pushable,
                                   [this, index = i, label = _userOptions[i], isDefaultButton](const UIButton::ButtonEvent &event) {
                                       if (event.state == UIButton::EventButtonState::Clicked) {
                                           // Ha ez az alapértelmezett gomb és le van tiltva, ne csináljunk semmit
                                           if (isDefaultButton && _disableDefaultButton) {
                                               return;
                                           }

                                           _clickedUserButtonIndex = index;
                                           _clickedUserButtonLabel = label;

                                           // Callback meghívása, ha van
                                           if (_buttonClickCallback) {
                                               _buttonClickCallback(index, label, this);
                                           }

                                           // Automatikus bezárás, ha engedélyezett
                                           if (_autoCloseOnButtonClick) {
                                               close(DialogResult::Accepted);
                                           }
                                       }
                                   },
                                   buttonState,
                                   0, // auto szélesség
                                   UIButton::DEFAULT_BUTTON_HEIGHT});
        }
    }
}

/**
 * @brief Elrendezi a dialógus gombjait.
 */
void MultiButtonDialog::layoutDialogContent() {
    // Korábbi gombok eltávolítása
    for (const auto &btn : _buttonsList) {
        removeChild(btn);
    }
    _buttonsList.clear();

    if (_buttonDefs.empty()) {
        markForRedraw();
        return;
    }

    uint16_t buttonHeight = UIButton::DEFAULT_BUTTON_HEIGHT;

    // === EGYSÉGES GOMBSZÉLESSÉG SZÁMÍTÁS ===
    // Megkeressük a leghosszabb címke szélességét
    uint16_t maxButtonWidth = UIButton::DEFAULT_BUTTON_WIDTH; // Minimum szélesség

    for (const auto &def : _buttonDefs) {
        uint16_t calculatedWidth = UIButton::calculateWidthForText(def.label, false, buttonHeight);
        if (calculatedWidth > maxButtonWidth) {
            maxButtonWidth = calculatedWidth;
        }
    }

    // Biztonsági margó hozzáadása (10px)
    maxButtonWidth += 10;

    // Gombdefiníciók frissítése - minden gomb ugyanazt a szélességet kapja
    for (auto &def : _buttonDefs) {
        def.height = buttonHeight;
        def.width = maxButtonWidth; // <<<< KULCS: Minden gomb ugyanakkora széles lesz!
    }

    // Margók kiszámítása (képernyő-relatív)
    int16_t manager_marginLeft = bounds.x + UIDialogBase::PADDING;
    int16_t manager_marginRight = tft.width() - (bounds.x + bounds.width - UIDialogBase::PADDING);
    int16_t manager_marginBottom = tft.height() - (bounds.y + bounds.height - (2 * UIDialogBase::PADDING));

    // Gombok elrendezése horizontálisan - most minden gomb egyforma széles
    layoutHorizontalButtonGroup(_buttonDefs, &_buttonsList, manager_marginLeft, manager_marginRight, manager_marginBottom,
                                maxButtonWidth,        // defaultButtonWidthRef - most az egységes szélességet adjuk át
                                buttonHeight,          // defaultButtonHeightRef
                                UIDialogBase::PADDING, // rowGap
                                UIDialogBase::PADDING, // buttonGap
                                true                   // centerHorizontally
    );

    markForRedraw();
}

/**
 * @brief Rajzolja a dialógus hátterét, fejlécét és az üzenetet.
 */
void MultiButtonDialog::drawSelf() {
    // Alap dialógus keret és fejléc rajzolása
    UIDialogBase::drawSelf();

    if (message) {
        tft.setTextSize(1);
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextColor(colors.foreground, colors.background);

        uint16_t headerH = getHeaderHeight();

        // Szöveg területének kiszámítása
        Rect textArea;
        textArea.x = bounds.x + UIDialogBase::PADDING + 2;
        textArea.y = bounds.y + headerH + UIDialogBase::PADDING;
        textArea.width = bounds.width - (2 * (UIDialogBase::PADDING + 2));
        textArea.height = bounds.height - headerH - UIButton::DEFAULT_BUTTON_HEIGHT - (4 * UIDialogBase::PADDING);

        if (textArea.width > 0 && textArea.height > 0) {
            tft.setTextDatum(TC_DATUM); // Top-Center
            int16_t textDrawY = textArea.y + textArea.height / 2;
            if (tft.fontHeight() > textArea.height) {
                textDrawY = textArea.y + tft.fontHeight() / 2;
            }
            tft.drawString(message, textArea.x + textArea.width / 2, textDrawY);
        }
    }
}

/**
 * @brief Manuálisan bezárja a dialógust a megadott eredménnyel.
 * @param result A dialógus eredménye
 */
void MultiButtonDialog::closeDialog(DialogResult result) { close(result); }

/**
 * @brief Beállítja az alapértelmezett gomb indexét és frissíti a dialógus tartalmát.
 * @param defaultIndex Az új alapértelmezett gomb indexe (-1 = nincs alapértelmezett gomb)
 */
void MultiButtonDialog::setDefaultButtonIndex(int defaultIndex) {
    if (_defaultButtonIndex != defaultIndex) {
        _defaultButtonIndex = defaultIndex;

        // Újra létrehozzuk a dialógus tartalmát az új default button logikával
        createDialogContent();
        layoutDialogContent();
    }
}

/**
 * @brief Beállítja, hogy az alapértelmezett gomb le legyen-e tiltva vagy csak vizuálisan kiemelve.
 * @param disable Ha true, az alapértelmezett gomb le van tiltva; ha false, csak vizuálisan kiemelve
 */
void MultiButtonDialog::setDisableDefaultButton(bool disable) {
    if (_disableDefaultButton != disable) {
        _disableDefaultButton = disable;

        // Újra létrehozzuk a dialógus tartalmát az új disable logikával
        createDialogContent();
        layoutDialogContent();
    }
}

/**
 * @brief MultiButtonDialog konstruktor const char* alapú alapértelmezett gombbal.
 *
 * Ez a konstruktor megkeresi a defaultButtonLabel indexét az options tömbben,
 * majd meghívja az eredeti konstruktort az index értékkel.
 */
MultiButtonDialog::MultiButtonDialog(UIScreen *parentScreen, const char *title, const char *message, const char *const *options, uint8_t numOptions, ButtonClickCallback buttonClickCb, bool autoClose,
                                     const char *defaultButtonLabel, bool disableDefaultButton, const Rect &bounds, const ColorScheme &cs)
    : MultiButtonDialog(parentScreen, title, message, options, numOptions, buttonClickCb, autoClose, findButtonIndex(options, numOptions, defaultButtonLabel), disableDefaultButton, bounds, cs) {
    // Az eredetikonstruktor delegáláson keresztül hívódik meg
    // A findButtonIndex segédfüggvény megkeresi a megfelelő indexet
}

/**
 * @brief Segédfüggvény a gomb index megkereséséhez felirat alapján.
 * @param options Gombok feliratainak tömbje
 * @param numOptions A gombok száma
 * @param buttonLabel A keresett gomb felirata
 * @return A gomb indexe, vagy -1 ha nem található
 */
int MultiButtonDialog::findButtonIndex(const char *const *options, uint8_t numOptions, const char *buttonLabel) {
    if (buttonLabel == nullptr) {
        return -1; // Nincs alapértelmezett gomb
    }

    for (uint8_t i = 0; i < numOptions; i++) {
        if (options[i] != nullptr && strcmp(options[i], buttonLabel) == 0) {
            return static_cast<int>(i);
        }
    }

    return -1; // Nem található
}
