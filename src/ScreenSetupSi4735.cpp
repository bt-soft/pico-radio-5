#include "ScreenSetupSi4735.h"
#include "Config.h"
#include "MultiButtonDialog.h"
#include "ValueChangeDialog.h"

/**
 * @brief ScreenSetupSi4735 konstruktor
 *
 * @param tft TFT_eSPI referencia a kijelző kezeléséhez
 */
ScreenSetupSi4735::ScreenSetupSi4735() : ScreenSetupBase(SCREEN_NAME_SETUP_SI4735) { layoutComponents(); }

/**
 * @brief Képernyő címének visszaadása
 *
 * @return A képernyő címe
 */
const char *ScreenSetupSi4735::getScreenTitle() const { return "Si4735 Settings"; }

/**
 * @brief Menüpontok feltöltése Si4735 specifikus beállításokkal
 *
 * Ez a metódus feltölti a menüpontokat a Si4735 chip aktuális
 * konfigurációs értékeivel.
 */
void ScreenSetupSi4735::populateMenuItems() {
    // Korábbi menüpontok törlése
    settingItems.clear();

    // Si4735 specifikus beállítások hozzáadása
    settingItems.push_back(SettingItem("Squelch Basis", String(config.data.squelchUsesRSSI ? "RSSI" : "SNR"), static_cast<int>(Si4735ItemAction::SQUELCH_BASIS)));

    settingItems.push_back(SettingItem("FFT Config AM", decodeFFTConfig(config.data.miniAudioFftConfigAm), static_cast<int>(Si4735ItemAction::FFT_CONFIG_AM)));

    settingItems.push_back(SettingItem("FFT Config FM", decodeFFTConfig(config.data.miniAudioFftConfigFm), static_cast<int>(Si4735ItemAction::FFT_CONFIG_FM)));

    // Példa további Si4735 beállításokra (ha léteznek a config-ban)
    // settingItems.push_back(SettingItem("Volume Level",
    //     String(config.data.volumeLevel),
    //     static_cast<int>(Si4735ItemAction::VOLUME_LEVEL)));

    // settingItems.push_back(SettingItem("Audio Mute",
    //     String(config.data.audioMute ? "ON" : "OFF"),
    //     static_cast<int>(Si4735ItemAction::AUDIO_MUTE)));

    // Lista komponens újrarajzolásának kérése, ha létezik
    if (menuList) {
        menuList->markForRedraw();
    }
}

/**
 * @brief Menüpont akció kezelése
 *
 * Ez a metódus kezeli a Si4735 specifikus menüpontok kattintásait.
 *
 * @param index A menüpont indexe
 * @param action Az akció azonosító
 */
void ScreenSetupSi4735::handleItemAction(int index, int action) {
    Si4735ItemAction si4735Action = static_cast<Si4735ItemAction>(action);

    switch (si4735Action) {
        case Si4735ItemAction::SQUELCH_BASIS:
            handleSquelchBasisDialog(index);
            break;
        case Si4735ItemAction::FFT_CONFIG_AM:
            handleFFTConfigDialog(index, true);
            break;
        case Si4735ItemAction::FFT_CONFIG_FM:
            handleFFTConfigDialog(index, false);
            break;
        // case Si4735ItemAction::VOLUME_LEVEL:
        //     handleVolumeLevelDialog(index);
        //     break;
        // case Si4735ItemAction::AUDIO_MUTE:
        //     handleToggleItem(index, config.data.audioMute);
        //     break;
        case Si4735ItemAction::NONE:
        default:
            DEBUG("ScreenSetupSi4735: Unknown action: %d\n", action);
            break;
    }
}

/**
 * @brief FFT konfiguráció érték dekódolása olvasható szöveggé
 *
 * @param value Az FFT konfigurációs érték
 * @return Olvasható string reprezentáció
 */
String ScreenSetupSi4735::decodeFFTConfig(float value) {
    if (value == -1.0f)
        return "Disabled";
    else if (value == 0.0f)
        return "Auto Gain";
    else
        return "Manual: " + String(value, 1) + "x";
}

/**
 * @brief Zajzár alapjának kiválasztása dialógussal
 *
 * @param index A menüpont indexe a lista frissítéséhez
 */
void ScreenSetupSi4735::handleSquelchBasisDialog(int index) {
    const char *options[] = {"RSSI", "SNR"};
    int currentSelection = config.data.squelchUsesRSSI ? 0 : 1;

    auto basisDialog = std::make_shared<MultiButtonDialog>(
        this, "Squelch Basis", "Select squelch basis:", options, ARRAY_ITEM_COUNT(options),
        [this, index](int buttonIndex, const char *buttonLabel, MultiButtonDialog *dialog) {
            bool newSquelchUsesRSSI = (buttonIndex == 0);
            if (config.data.squelchUsesRSSI != newSquelchUsesRSSI) {
                config.data.squelchUsesRSSI = newSquelchUsesRSSI;
                config.checkSave();
            }
            settingItems[index].value = String(config.data.squelchUsesRSSI ? "RSSI" : "SNR");
            updateListItem(index);
        },
        true, currentSelection, true, Rect(-1, -1, 250, 120));
    this->showDialog(basisDialog);
}

/**
 * @brief FFT konfigurációs dialógus kezelése AM vagy FM módhoz
 *
 * @param index A menüpont indexe a lista frissítéséhez
 * @param isAM true = AM mód, false = FM mód
 */
void ScreenSetupSi4735::handleFFTConfigDialog(int index, bool isAM) {
    float &currentConfig = isAM ? config.data.miniAudioFftConfigAm : config.data.miniAudioFftConfigFm;
    const char *title = isAM ? "FFT Config AM" : "FFT Config FM";

    int defaultSelection = 0; // Disabled
    if (currentConfig == 0.0f) {
        defaultSelection = 1; // Auto Gain
    } else if (currentConfig > 0.0f) {
        defaultSelection = 2; // Manual Gain
    }

    const char *options[] = {"Disabled", "Auto G", "Manual G"};

    auto fftDialog = std::make_shared<MultiButtonDialog>(
        this, title, "Select FFT gain mode:", options, ARRAY_ITEM_COUNT(options),
        [this, index, isAM, &currentConfig, title](int buttonIndex, const char *buttonLabel, MultiButtonDialog *dialog) {
            switch (buttonIndex) {
                case 0: // Disabled
                    currentConfig = -1.0f;
                    config.checkSave();
                    settingItems[index].value = "Disabled";
                    updateListItem(index);
                    dialog->close(UIDialogBase::DialogResult::Accepted);
                    break;

                case 1: // Auto Gain
                    currentConfig = 0.0f;
                    config.checkSave();
                    settingItems[index].value = "Auto Gain";
                    updateListItem(index);
                    dialog->close(UIDialogBase::DialogResult::Accepted);
                    break;

                case 2: // Manual Gain
                {
                    dialog->close(UIDialogBase::DialogResult::Accepted);

                    auto tempGainValuePtr = std::make_shared<float>((currentConfig > 0.0f) ? currentConfig : 1.0f);

                    auto gainDialog = std::make_shared<ValueChangeDialog>(
                        this, (String(title) + " - Manual Gain").c_str(), "Set gain factor (0.1 - 10.0):", tempGainValuePtr.get(), 0.1f, 10.0f, 0.1f, nullptr,
                        [this, index, &currentConfig, tempGainValuePtr](UIDialogBase *sender, MessageDialog::DialogResult result) {
                            if (result == MessageDialog::DialogResult::Accepted) {
                                currentConfig = *tempGainValuePtr;
                                config.checkSave();
                                populateMenuItems(); // Teljes frissítés a helyes érték megjelenítéséhez
                            }
                        },
                        Rect(-1, -1, 300, 0));
                    this->showDialog(gainDialog);
                } break;
            }
        },
        false, defaultSelection, false, Rect(-1, -1, 340, 120));
    this->showDialog(fftDialog);
}

/**
 * @brief Boolean beállítások váltása
 *
 * @param index A menüpont indexe
 * @param configValue Referencia a módosítandó boolean értékre
 */
void ScreenSetupSi4735::handleToggleItem(int index, bool &configValue) {
    configValue = !configValue;
    config.checkSave();

    if (index >= 0 && index < settingItems.size()) {
        settingItems[index].value = String(configValue ? "ON" : "OFF");
        updateListItem(index);
    }
}
