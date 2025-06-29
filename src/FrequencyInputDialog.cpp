#include "FrequencyInputDialog.h"

/**
 * @brief Konstruktor
 */
FrequencyInputDialog::FrequencyInputDialog(UIScreen *parentScreen, const char *title, const char *message, const Rect &bounds, FrequencyChangeCallback callback, const ColorScheme &cs)
    : UIDialogBase(parentScreen, title, bounds, cs), _frequencyCallback(callback), _isValid(false), _firstInput(true) {

    // Sáv paraméterek inicializálása
    initializeBandParameters();

    // Aktuális frekvencia betöltése
    // Az SI4735 osztály cache-ból olvassuk az aktuális frekvenciát, nem használunk chip olvasást
    uint16_t currentFreq = ::pSi4735Manager->getSi4735().getCurrentFrequency();
    setCurrentFrequency(currentFreq);

    // Dialógus méret beállítása ha automatikus
    if (this->bounds.width <= 0 || this->bounds.height <= 0) {
        this->bounds.width = ::SCREEN_W;
        this->bounds.height = 350; // Növelt magasság a gomboknak
    }

    // Dialógus tartalom létrehozása
    createDialogContent();

    // Layout alkalmazása
    layoutDialogContent();
}

/**
 * @brief Sáv paraméterek inicializálása
 */
void FrequencyInputDialog::initializeBandParameters() {
    _currentBandType = ::pSi4735Manager->getCurrentBandType();
    BandTable &currentBand = ::pSi4735Manager->getCurrentBand();

    _minFreq = currentBand.minimumFreq;
    _maxFreq = currentBand.maximumFreq;

    // Sáv típus alapú formátum beállítás
    switch (_currentBandType) {
        case FM_BAND_TYPE:
            _unitString = "MHz";
            _maskString = FM_MASK;
            break;

        case MW_BAND_TYPE:
        case LW_BAND_TYPE:
            _unitString = "kHz";
            _maskString = MW_LW_MASK;
            break;

        case SW_BAND_TYPE:
            _unitString = "kHz"; // SW is kHz-ben!
            _maskString = SW_MASK;
            break;

        default:
            _unitString = "MHz";
            _maskString = FM_MASK;
            break;
    }

    DEBUG("FrequencyInputDialog: Band type %d, range %d-%d %s\n", _currentBandType, _minFreq, _maxFreq, _unitString.c_str());
}

/**
 * @brief Aktuális frekvencia beállítása
 */
void FrequencyInputDialog::setCurrentFrequency(uint16_t rawFrequency) {
    // Frekvencia konvertálása string formátumba
    switch (_currentBandType) {
        case FM_BAND_TYPE:
            // FM: 10800 -> "108.00"
            _inputString = String(rawFrequency / 100.0f, 2);
            break;

        case MW_BAND_TYPE:
        case LW_BAND_TYPE:
            // MW/LW: 1440 -> "1440"
            _inputString = String(rawFrequency);
            break;

        case SW_BAND_TYPE:
            // SW: 15230 -> "15230" (kHz-ben!)
            _inputString = String(rawFrequency);
            break;

        default:
            _inputString = "0";
            break;
    }

    // Maszk generálása
    generateMaskPattern(); // Validálás
    _isValid = validateAndParseFrequency();

    // OK gomb állapot frissítése (ha már létezik)
    updateOkButtonState();
}

/**
 * @brief Maszk pattern generálása
 */
void FrequencyInputDialog::generateMaskPattern() {
    switch (_currentBandType) {
        case FM_BAND_TYPE:
            _displayString = applyInputMask(_inputString);
            break;
        case MW_BAND_TYPE:
        case LW_BAND_TYPE:
            _displayString = _inputString + " " + _unitString;
            break;
        case SW_BAND_TYPE:
            _displayString = applyInputMask(_inputString);
            break;
        default:
            _displayString = _inputString + " " + _unitString;
            break;
    }
}

/**
 * @brief Beviteli maszk alkalmazása
 */
String FrequencyInputDialog::applyInputMask(const String &inputString) const {
    if (inputString.isEmpty()) {
        return _maskString; // Üres maszk megjelenítése
    }

    String result;

    switch (_currentBandType) {
        case FM_BAND_TYPE: {
            // FM: "108.50" -> "108.50 MHz"
            result = inputString + " " + _unitString;
            break;
        }
        case SW_BAND_TYPE: {
            // SW: "15230" -> "15 230.00 kHz"
            if (inputString.length() >= 5) {
                String intPart = inputString.substring(0, inputString.length() - 3);
                String decPart = inputString.substring(inputString.length() - 3);

                // Szóköz beszúrása az egész részbe (pl. "15" -> "15")
                if (intPart.length() > 2) {
                    result = intPart.substring(0, intPart.length() - 2) + " " + intPart.substring(intPart.length() - 2);
                } else {
                    result = intPart;
                }
                result += " " + decPart + " " + _unitString;
            } else {
                result = inputString + " " + _unitString;
            }
            break;
        }
        default:
            result = inputString + " " + _unitString;
            break;
    }

    return result;
}

/**
 * @brief Dialógus tartalom létrehozása
 */
void FrequencyInputDialog::createDialogContent() {
    // OK és Cancel gombok létrehozása
    createOkCancelButtons();

    // Numerikus gombok létrehozása
    createNumericButtons();

    // Funkció gombok létrehozása
    createFunctionButtons();
}

/**
 * @brief OK és Cancel gombok létrehozása
 */
void FrequencyInputDialog::createOkCancelButtons() {

    _okButton = std::make_shared<UIButton>(100, Rect(0, 0, 60, 30), "OK", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off);
    // Disabled színek beállítása az OK gomb számára
    ButtonColorScheme okButtonScheme = UIColorPalette::createDefaultButtonScheme();
    _okButton->setButtonColorScheme(okButtonScheme);
    _okButton->setEventCallback([this](const UIButton::ButtonEvent &event) {
        if (event.state == UIButton::EventButtonState::Clicked) {
            onOkClicked();
        }
    });
    addChild(_okButton);

    // Cancel gomb
    _cancelButton = std::make_shared<UIButton>(101, Rect(0, 0, 60, 30), "Cancel", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off);
    _cancelButton->setEventCallback([this](const UIButton::ButtonEvent &event) {
        if (event.state == UIButton::EventButtonState::Clicked) {
            onCancelClicked();
        }
    });
    addChild(_cancelButton);
}

/**
 * @brief Numerikus gombok létrehozása
 */
void FrequencyInputDialog::createNumericButtons() {

    // Statikus felirat tömb a gombok számára
    static const char *digitLabels[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

    _digitButtons.clear();
    _digitButtons.reserve(10);

    // 0-9 gombok létrehozása
    for (uint8_t i = 0; i <= 9; i++) {
        auto button = std::make_shared<UIButton>(i, Rect(0, 0, NUMERIC_BUTTON_SIZE, NUMERIC_BUTTON_SIZE), digitLabels[i], UIButton::ButtonType::Pushable, UIButton::ButtonState::Off);

        // Gomb esemény kezelő beállítása
        button->setEventCallback([this, i](const UIButton::ButtonEvent &event) {
            if (event.state == UIButton::EventButtonState::Clicked) {
                handleDigitInput(i);
            }
        });

        _digitButtons.push_back(button);
        addChild(button);
    }
}

/**
 * @brief Funkció gombok létrehozása
 */
void FrequencyInputDialog::createFunctionButtons() {
    // Statikus feliratok

    // Tizedes pont gomb (mindig létrehozzuk, de csak FM-nél látható)
    _dotButton = std::make_shared<UIButton>(10, Rect(0, 0, NUMERIC_BUTTON_SIZE, NUMERIC_BUTTON_SIZE), ".", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off);
    _dotButton->setEventCallback([this](const UIButton::ButtonEvent &event) {
        if (event.state == UIButton::EventButtonState::Clicked) {
            handleDotInput();
        }
    });

    addChild(_dotButton);

    // Clear All gomb (C)
    _clearAllButton = std::make_shared<UIButton>(12, Rect(0, 0, NUMERIC_BUTTON_SIZE, NUMERIC_BUTTON_SIZE), "C", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off);
    _clearAllButton->setEventCallback([this](const UIButton::ButtonEvent &event) {
        if (event.state == UIButton::EventButtonState::Clicked) {
            handleClearAll();
        }
    });
    addChild(_clearAllButton);
}

/**
 * @brief Dialógus tartalom elrendezése
 */
void FrequencyInputDialog::layoutDialogContent() {
    // Frekvencia kijelző terület a cím alatt
    uint16_t displayY = bounds.y + HEADER_HEIGHT + PADDING;

    // Numerikus billentyűzet elrendezése - 5 oszlopos layout
    uint16_t buttonSpacing = NUMERIC_BUTTON_SIZE + BUTTON_SPACING;
    uint16_t wideButtonWidth = NUMERIC_BUTTON_SIZE * 2 + BUTTON_SPACING; // Dupla szélesség + spacing

    // Teljes keypad szélességének számítása (4 normál gomb + 1 dupla gomb)
    uint16_t keypadWidth = 4 * buttonSpacing + wideButtonWidth;
    uint16_t keypadStartX = bounds.x + (bounds.width - keypadWidth) / 2;   // Középre igazítás
    uint16_t keypadStartY = displayY + DISPLAY_AREA_HEIGHT + PADDING - 10; // 10px feljebb// Sor 1: [1] [2] [3] [4] [Cancel]
    for (int i = 1; i <= 4; i++) {
        auto &button = _digitButtons[i];
        button->setBounds(Rect(keypadStartX + (i - 1) * buttonSpacing, keypadStartY, NUMERIC_BUTTON_SIZE, NUMERIC_BUTTON_SIZE));
    }
    // Cancel gomb az első sor végén - dupla szélesség
    _cancelButton->setBounds(Rect(keypadStartX + 4 * buttonSpacing, keypadStartY, wideButtonWidth, NUMERIC_BUTTON_SIZE));

    // Sor 2: [5] [6] [7] [8]
    for (int i = 5; i <= 8; i++) {
        auto &button = _digitButtons[i];
        button->setBounds(Rect(keypadStartX + (i - 5) * buttonSpacing, keypadStartY + buttonSpacing, NUMERIC_BUTTON_SIZE, NUMERIC_BUTTON_SIZE));
    }

    // Sor 3: [C] [9] [0] [.] [OK]
    // Clear All gomb (bal oldal)
    _clearAllButton->setBounds(Rect(keypadStartX, keypadStartY + 2 * buttonSpacing, NUMERIC_BUTTON_SIZE, NUMERIC_BUTTON_SIZE));

    // 9 gomb (második pozíció)
    _digitButtons[9]->setBounds(Rect(keypadStartX + buttonSpacing, keypadStartY + 2 * buttonSpacing, NUMERIC_BUTTON_SIZE, NUMERIC_BUTTON_SIZE));

    // 0 gomb (harmadik pozíció)
    _digitButtons[0]->setBounds(Rect(keypadStartX + 2 * buttonSpacing, keypadStartY + 2 * buttonSpacing, NUMERIC_BUTTON_SIZE, NUMERIC_BUTTON_SIZE));

    // Tizedes pont gomb (negyedik pozíció)
    if (_dotButton) {
        _dotButton->setBounds(Rect(keypadStartX + 3 * buttonSpacing, keypadStartY + 2 * buttonSpacing, NUMERIC_BUTTON_SIZE, NUMERIC_BUTTON_SIZE));
        // FM sávnál engedélyezett, egyébként letiltott
        _dotButton->setEnabled(_currentBandType == FM_BAND_TYPE);
    }

    // OK gomb a harmadik sor végén - dupla szélesség
    _okButton->setBounds(Rect(keypadStartX + 4 * buttonSpacing, keypadStartY + 2 * buttonSpacing, wideButtonWidth, NUMERIC_BUTTON_SIZE));
}

/**
 * @brief Saját tartalom rajzolása
 */
void FrequencyInputDialog::drawSelf() {
    // Szülő rajzolás (háttér, cím)
    UIDialogBase::drawSelf();

    // Frekvencia kijelző rajzolása
    drawFrequencyDisplay();
}

/**
 * @brief Frekvencia kijelző rajzolása
 */
void FrequencyInputDialog::drawFrequencyDisplay() {
    // Kijelző terület
    uint16_t displayY = bounds.y + HEADER_HEIGHT + PADDING;
    uint16_t displayCenterX = bounds.x + bounds.width / 2;

    // Frekvencia szám előkészítése (csak számok, pont, szóköz)
    String freqNumbers = _inputString.isEmpty() ? "0" : _inputString;

    // SW sáv speciális formázása
    if (_currentBandType == SW_BAND_TYPE && freqNumbers.length() >= 5) {
        String intPart = freqNumbers.substring(0, freqNumbers.length() - 3);
        String decPart = freqNumbers.substring(freqNumbers.length() - 3);

        if (intPart.length() > 2) {
            freqNumbers = intPart.substring(0, intPart.length() - 2) + " " + intPart.substring(intPart.length() - 2) + "." + decPart;
        } else {
            freqNumbers = intPart + "." + decPart;
        }
    }

    // 7-szegmenses font beállítása CSAK a számokhoz
    tft.setFreeFont(&DSEG7_Classic_Mini_Regular_34);
    tft.setTextSize(1);
    tft.setTextDatum(BC_DATUM);

    // Frekvencia számok megjelenítése
    uint16_t textColor = _isValid ? colors.foreground : TFT_RED;
    tft.setTextColor(textColor, colors.background);
    tft.drawString(freqNumbers, displayCenterX - 20, displayY + 40); // Kicsit balra tolva

    // Mértékegység kirajzolása a számok mellé
    tft.setFreeFont();
    tft.setTextSize(2);
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(colors.foreground, colors.background);
    tft.drawString(_unitString, displayCenterX + 70, displayY + 40);

    // Font visszaállítása
    tft.setTextSize(1);
}

/**
 * @brief Numerikus gomb megnyomás kezelése
 */
void FrequencyInputDialog::handleDigitInput(uint8_t digit) {
    // Ha ez az első bevitel, töröljük az előzetes értéket
    if (_firstInput) {
        _inputString = "";
        _firstInput = false;
    }

    // Maximum hossz ellenőrzése sáv alapján
    uint8_t maxLength;
    switch (_currentBandType) {
        case FM_BAND_TYPE:
            maxLength = 6; // "108.00"
            break;
        case MW_BAND_TYPE:
        case LW_BAND_TYPE:
            maxLength = 4; // "1440"
            break;
        case SW_BAND_TYPE:
            maxLength = 5; // "15230"
            break;
        default:
            maxLength = 6;
            break;
    }

    if (_inputString.length() >= maxLength) {
        Utils::beepError();
        return;
    }

    // Digit hozzáadása
    _inputString += String(digit);

    // Validálás és kijelző frissítése
    _isValid = validateAndParseFrequency();
    updateOkButtonState();
    updateFrequencyDisplay();
}

/**
 * @brief Tizedes pont bevitel kezelése
 */
void FrequencyInputDialog::handleDotInput() {
    // Ha ez az első bevitel, töröljük az előzetes értéket
    if (_firstInput) {
        _inputString = "";
        _firstInput = false;
    }

    // Ellenőrizzük, hogy már van-e pont
    if (_inputString.indexOf('.') != -1) {
        Utils::beepError();
        return;
    }

    // Ha üres, akkor "0." -ot kezdünk
    if (_inputString.isEmpty()) {
        _inputString = "0.";
    } else {
        _inputString += ".";
    }

    updateFrequencyDisplay();
}

/**
 * @brief Egy digit törlése
 */
void FrequencyInputDialog::handleClearDigit() {
    if (_inputString.length() > 0) {
        _inputString = _inputString.substring(0, _inputString.length() - 1);
        _isValid = validateAndParseFrequency();
        updateOkButtonState();
        updateFrequencyDisplay();
    }

    // Ha teljesen üres lett, akkor újra first input állapot
    if (_inputString.isEmpty()) {
        _firstInput = true;
    }
}

/**
 * @brief Minden digit törlése
 */
void FrequencyInputDialog::handleClearAll() {
    _inputString = "";
    _isValid = false;
    _firstInput = true; // Vissza az eredeti állapotba
    updateOkButtonState();
    updateFrequencyDisplay();
}

/**
 * @brief Frekvencia string validálása
 */
bool FrequencyInputDialog::validateAndParseFrequency() {
    if (_inputString.isEmpty()) {
        return false;
    }

    // String-ből float konvertálás
    float freqValue = _inputString.toFloat();

    // Alapvető tartomány ellenőrzés
    switch (_currentBandType) {
        case FM_BAND_TYPE:
            // FM: 64.0 - 108.0 MHz
            if (freqValue < 64.0f || freqValue > 108.0f)
                return false;
            break;

        case MW_BAND_TYPE:
            // MW: 520 - 1710 kHz
            if (freqValue < 520.0f || freqValue > 1710.0f)
                return false;
            break;

        case LW_BAND_TYPE:
            // LW: 150 - 450 kHz
            if (freqValue < 150.0f || freqValue > 450.0f)
                return false;
            break;

        case SW_BAND_TYPE:
            // SW: 1800 - 30000 kHz
            if (freqValue < 1800.0f || freqValue > 30000.0f)
                return false;
            break;

        default:
            return false;
    }

    // Sávhatárokon belüli ellenőrzés
    uint16_t rawFreq = calculateRawFrequency();
    return isFrequencyInBounds(rawFreq);
}

/**
 * @brief Nyers frekvencia kiszámítása
 */
uint16_t FrequencyInputDialog::calculateRawFrequency() const {
    float freqValue = _inputString.toFloat();

    switch (_currentBandType) {
        case FM_BAND_TYPE:
            // FM: MHz -> x100 (pl. 100.5 -> 10050)
            return static_cast<uint16_t>(freqValue * 100);

        case MW_BAND_TYPE:
        case LW_BAND_TYPE:
        case SW_BAND_TYPE:
            // MW/LW/SW: kHz -> x1 (pl. 1440 -> 1440, 15230 -> 15230)
            return static_cast<uint16_t>(freqValue);

        default:
            return 0;
    }
}

/**
 * @brief Sáv határokon belüli ellenőrzés
 */
bool FrequencyInputDialog::isFrequencyInBounds(uint16_t rawFreq) const { return (rawFreq >= _minFreq && rawFreq <= _maxFreq); }

/**
 * @brief OK gomb állapot frissítése
 */
void FrequencyInputDialog::updateOkButtonState() {
    if (_okButton) {
        // Ez beállítja a gomb needsRedraw flag-jét és automatikusan frissül.
        _okButton->setButtonState(_isValid ? UIButton::ButtonState::Off : UIButton::ButtonState::Disabled);
    }
}

/**
 * @brief Kijelző frissítése
 */
void FrequencyInputDialog::updateFrequencyDisplay() {
    // Csak a frekvencia kijelző területet frissítjük
    uint16_t displayY = bounds.y + HEADER_HEIGHT + PADDING;

    // Teljes szélességben törlés kis margóval, de kisebb magasságban (50px)
    uint16_t clearHeight = 50; // Kisebb magasság, hogy ne nyúljon bele a gombokba
    uint16_t clearMargin = 5;  // Kis margó a szélek mellett
    tft.fillRect(bounds.x + clearMargin, displayY, bounds.width - 2 * clearMargin, clearHeight, colors.background);

    // Frekvencia újrarajzolása
    drawFrequencyDisplay();
}

/**
 * @brief Rotary encoder kezelés
 */
bool FrequencyInputDialog::handleRotary(const RotaryEvent &event) { return UIDialogBase::handleRotary(event); }

/**
 * @brief OK gomb kezelése
 */
void FrequencyInputDialog::onOkClicked() {
    if (_isValid && _frequencyCallback) {
        uint16_t rawFreq = calculateRawFrequency();
        _frequencyCallback(rawFreq);
    }

    close(DialogResult::Accepted);
}

/**
 * @brief Cancel gomb kezelése
 */
void FrequencyInputDialog::onCancelClicked() { close(DialogResult::Rejected); }

// Hiányzó metódusok implementálása (ha szükséges)
int FrequencyInputDialog::findNextEditablePosition() const {
    return -1; // Egyelőre nem implementált
}

int FrequencyInputDialog::findPreviousEditablePosition() const {
    return -1; // Egyelőre nem implementált
}

bool FrequencyInputDialog::insertCharacterToMask(char ch) {
    return false; // Egyelőre nem implementált
}
