#include "UIHorizontalButtonBar.h"
#include "defines.h"

/**
 * @brief Konstruktor - vízszintes gombsor létrehozása többsoros támogatással
 */
UIHorizontalButtonBar::UIHorizontalButtonBar(const Rect &bounds, const std::vector<ButtonConfig> &buttonConfigs, uint16_t buttonWidth, uint16_t buttonHeight, uint16_t buttonGap, uint16_t rowGap)
    : UIContainerComponent(bounds), buttonWidth(buttonWidth), buttonHeight(buttonHeight), buttonGap(buttonGap), rowGap(rowGap) {

    createButtons(buttonConfigs);
}

/**
 * @brief Gombok létrehozása és többsoros elhelyezése
 */
void UIHorizontalButtonBar::createButtons(const std::vector<ButtonConfig> &buttonConfigs) {
    if (buttonConfigs.empty()) {
        return;
    }

    // Számítsuk ki, hogy hány gomb fér el egy sorban
    uint16_t buttonsPerRow = calculateButtonsPerRow();
    if (buttonsPerRow == 0) {
        return;
    }

    // Számítsuk ki a szükséges sorok számát
    uint16_t totalButtons = buttonConfigs.size();
    uint16_t requiredRows = calculateRequiredRows(totalButtons);

    // Az utolsó sor a képernyő aljához igazítva
    uint16_t lastRowY = ::SCREEN_H - buttonHeight;

    uint16_t currentRow = 0;
    uint16_t buttonInRow = 0;

    for (size_t i = 0; i < buttonConfigs.size(); ++i) {
        const auto &config = buttonConfigs[i];

        // Ha elértük a sor végét, új sort kezdünk
        if (buttonInRow >= buttonsPerRow) {
            currentRow++;
            buttonInRow = 0;
        }

        // Számítsuk ki, melyik sor ez az utolsóhoz képest (fordított sorrendben)
        uint16_t rowFromBottom = (requiredRows - 1) - (currentRow);

        // Aktuális gomb pozíciójának számítása
        // Az utolsó sortól felfelé számoljuk a sorokat
        uint16_t buttonX = bounds.x + buttonInRow * (buttonWidth + buttonGap);
        uint16_t buttonY = lastRowY - rowFromBottom * (buttonHeight + rowGap);

        // Biztonsági ellenőrzés: nem mehetünk túl a képernyő szélein
        // A felső határ a képernyő teteje legyen, nem a bounds.y
        if (buttonY < 0) {
            break;
        }

        if (buttonX + buttonWidth > ::SCREEN_W) {
            break;
        }

        // Gomb létrehozása
        auto button = std::make_shared<UIButton>(config.id, Rect(buttonX, buttonY, buttonWidth, buttonHeight), config.label, config.type, config.initialState, config.callback);

        // Hozzáadás a konténerhez és a belső listához
        addChild(button);
        buttons.push_back(button);

        buttonInRow++;
    }
}

/**
 * @brief Kiszámítja, hogy hány gomb fér el egy sorban
 */
uint16_t UIHorizontalButtonBar::calculateButtonsPerRow() const {
    // A képernyő szélességéből levonjuk a jobb oldali függőleges gombsor helyét
    // Függőleges gombsor: 60px széles + 0px margó = 60px
    const uint16_t VERTICAL_BUTTON_WIDTH = 60;
    const uint16_t VERTICAL_BUTTON_MARGIN = 0;
    const uint16_t RESERVED_FOR_VERTICAL_BUTTONS = VERTICAL_BUTTON_WIDTH + VERTICAL_BUTTON_MARGIN;

    uint16_t availableWidth = ::SCREEN_W - RESERVED_FOR_VERTICAL_BUTTONS;

    if (buttonWidth > availableWidth) {
        return 0; // Egy gomb sem fér el
    }

    // Első gomb mindig elfér, utána minden gombhoz hozzáadódik a gap is
    uint16_t usedWidth = buttonWidth; // Első gomb
    uint16_t buttonCount = 1;

    while (usedWidth + buttonGap + buttonWidth <= availableWidth) {
        usedWidth += buttonGap + buttonWidth;
        buttonCount++;
    }

    return buttonCount;
}

/**
 * @brief Számítja ki a szükséges sorok számát
 */
uint16_t UIHorizontalButtonBar::calculateRequiredRows(uint16_t totalButtons) const {
    uint16_t buttonsPerRow = calculateButtonsPerRow();
    if (buttonsPerRow == 0) {
        return 0;
    }

    return (totalButtons + buttonsPerRow - 1) / buttonsPerRow; // Felfelé kerekítés
}

/**
 * @brief Gomb állapotának beállítása ID alapján
 */
void UIHorizontalButtonBar::setButtonState(uint8_t buttonId, UIButton::ButtonState state) {
    for (auto &button : buttons) {
        if (button->getId() == buttonId) {
            button->setButtonState(state);
            return;
        }
    }
}

/**
 * @brief Gomb állapotának lekérdezése ID alapján
 */
UIButton::ButtonState UIHorizontalButtonBar::getButtonState(uint8_t buttonId) const {
    for (const auto &button : buttons) {
        if (button->getId() == buttonId) {
            return button->getButtonState();
        }
    }
    return UIButton::ButtonState::Disabled;
}

/**
 * @brief Gomb referenciájának megszerzése ID alapján
 */
std::shared_ptr<UIButton> UIHorizontalButtonBar::getButton(uint8_t buttonId) const {
    for (const auto &button : buttons) {
        if (button->getId() == buttonId) {
            return button;
        }
    }
    return nullptr;
}
