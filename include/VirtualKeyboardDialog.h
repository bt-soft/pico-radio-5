#pragma once

#include "UIButton.h"
#include "UIDialogBase.h"

/**
 * @brief Virtuális billentyűzet dialógus állomás nevek szerkesztéséhez
 * @details Érintőképernyős billentyűzet dialógus szöveg bevitelhez
 */
class VirtualKeyboardDialog : public UIDialogBase {
  public:
    using OnTextChangedCallback = std::function<void(const String &newText)>;

    /**
     * @brief Konstruktor
     * @param parent Szülő UIScreen (dialógus megjelenítéséhez)
     * @param title Dialógus címe
     * @param initialText Kezdeti szöveg
     * @param maxLength Maximális szöveg hossz
     * @param onChanged Callback, amikor a szöveg megváltozik
     */
    VirtualKeyboardDialog(UIScreen *parent, const char *title, const String &initialText = "", uint8_t maxLength = 15, OnTextChangedCallback onChanged = nullptr);
    virtual ~VirtualKeyboardDialog(); // UIDialogBase interface
    virtual void drawSelf() override;
    virtual void handleOwnLoop() override;

    /**
     * @brief Aktuális szöveg lekérdezése
     */
    String getCurrentText() const { return currentText; }

    /**
     * @brief Szöveg beállítása
     */
    void setText(const String &text);

  private:
    // Billentyűzet konstansok
    static constexpr uint8_t KEYBOARD_ROWS = 4;
    static constexpr uint8_t MAX_KEYS_PER_ROW = 10;
    static constexpr uint16_t KEY_WIDTH = 32;
    static constexpr uint16_t KEY_HEIGHT = 27;
    static constexpr uint16_t KEY_SPACING = 2;
    static constexpr uint16_t INPUT_HEIGHT = 30;
    static constexpr uint16_t INPUT_MARGIN = 5;
    static constexpr unsigned long CURSOR_BLINK_INTERVAL = 500;                                         // Billentyűzet layout
    const char *keyboardLayout[KEYBOARD_ROWS] = {"1234567890", "qwertzuiop", "asdfghjkl", "yxcvbnm-."}; // UI elemek
    std::vector<std::shared_ptr<UIButton>> keyButtons;
    char keyLabelStorage[50][2]; // Max 50 gomb, 1 karakter + null terminator
    uint8_t keyLabelCount = 0;
    std::shared_ptr<UIButton> shiftButton;
    std::shared_ptr<UIButton> spaceButton;
    std::shared_ptr<UIButton> backspaceButton;
    std::shared_ptr<UIButton> clearButton;
    std::shared_ptr<UIButton> okButton;
    std::shared_ptr<UIButton> cancelButton; // Szöveg kezelés
    String currentText;
    uint8_t maxTextLength;
    OnTextChangedCallback textChangedCallback;

    // Kurzor villogás
    bool cursorVisible = true;
    unsigned long lastCursorBlink = 0;

    // Shift állapot
    bool shiftActive = false;

    // Pozíciók
    Rect inputRect;
    Rect keyboardRect; // Metódusok
    void createKeyboard();
    void drawInputField();
    void drawCursor();
    void redrawCursorArea();
    void handleKeyPress(char key);
    void handleSpecialKey(const String &keyType);
    void updateButtonLabels();
    void updateOkButtonState();
    char getKeyChar(char baseChar, bool shifted);
    void notifyTextChanged();
};
