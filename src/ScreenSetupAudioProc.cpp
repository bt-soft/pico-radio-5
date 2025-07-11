#include "ScreenSetupAudioProc.h"
#include "Config.h"
#include "MultiButtonDialog.h"
#include "ValueChangeDialog.h"

/**
 * @brief ScreenSetupAudioProc konstruktor
 */
ScreenSetupAudioProc::ScreenSetupAudioProc() : ScreenSetupBase(SCREEN_NAME_SETUP_AUDIO_PROC) { layoutComponents(); }

/**
 * @brief Képernyő címének visszaadása
 *
 * @return A képernyő címe
 */
const char *ScreenSetupAudioProc::getScreenTitle() const { return "Audio Processing"; }

/**
 * @brief Menüpontok feltöltése audió feldolgozás specifikus beállításokkal
 *
 * Ez a metódus feltölti a menüpontokat az audió feldolgozás aktuális
 * konfigurációs értékeivel.
 */
void ScreenSetupAudioProc::populateMenuItems() {
    // Korábbi menüpontok törlése
    settingItems.clear();

    settingItems.push_back(SettingItem("CW Receiver Offset", String(config.data.cwReceiverOffsetHz) + " Hz", static_cast<int>(AudioProcItemAction::CW_RECEIVER_OFFSET)));
    settingItems.push_back(SettingItem("RTTY Shift", String(config.data.rttyShiftHz) + " Hz", static_cast<int>(AudioProcItemAction::RTTY_SHIFT)));
    settingItems.push_back(SettingItem("RTTY Mark Frequency", String(config.data.rttyMarkFrequencyHz) + " Hz", static_cast<int>(AudioProcItemAction::RTTY_MARK_FREQUENCY)));

    settingItems.push_back(SettingItem("FFT Gain AM", decodeFFTGain(config.data.miniAudioFftConfigAm), static_cast<int>(AudioProcItemAction::FFT_GAIN_AM)));
    settingItems.push_back(SettingItem("FFT Gain FM", decodeFFTGain(config.data.miniAudioFftConfigFm), static_cast<int>(AudioProcItemAction::FFT_GAIN_FM)));

    // Lista komponens újrarajzolásának kérése, ha létezik
    if (menuList) {
        menuList->markForRedraw();
    }
}

/**
 * @brief Menüpont akció kezelése
 *
 * Ez a metódus kezeli az audió feldolgozás specifikus menüpontok kattintásait.
 *
 * @param index A menüpont indexe
 * @param action Az akció azonosító
 */
void ScreenSetupAudioProc::handleItemAction(int index, int action) {
    AudioProcItemAction audioProcAction = static_cast<AudioProcItemAction>(action);

    switch (audioProcAction) {
        case AudioProcItemAction::CW_RECEIVER_OFFSET:
            handleCwOffsetDialog(index);
            break;
        case AudioProcItemAction::RTTY_SHIFT:
            handleRttyShiftDialog(index);
            break;
        case AudioProcItemAction::RTTY_MARK_FREQUENCY:
            handleRttyMarkFrequencyDialog(index);
            break;
        case AudioProcItemAction::FFT_GAIN_AM:
            handleFFTGainDialog(index, true);
            break;
        case AudioProcItemAction::FFT_GAIN_FM:
            handleFFTGainDialog(index, false);
            break;
        case AudioProcItemAction::NONE:
        default:
            DEBUG("ScreenSetupAudioProc: Unknown action: %d\n", action);
            break;
    }
}

/**
 * @brief CW receiver offset beállítása dialógussal
 *
 * @param index A menüpont indexe a lista frissítéséhez
 */
void ScreenSetupAudioProc::handleCwOffsetDialog(int index) {
    auto tempValuePtr = std::make_shared<int>(static_cast<int>(config.data.cwReceiverOffsetHz));

    auto cwOffsetDialog = std::make_shared<ValueChangeDialog>(
        this, "CW Offset", "CW Receiver Offset (Hz):", tempValuePtr.get(),
        static_cast<int>(400),  // Min: 400Hz
        static_cast<int>(1900), // Max: 1900Hz
        static_cast<int>(10),   // Step: 10Hz
        [this, index](const std::variant<int, float, bool> &liveNewValue) {
            if (std::holds_alternative<int>(liveNewValue)) {
                int currentDialogVal = std::get<int>(liveNewValue);
                config.data.cwReceiverOffsetHz = static_cast<uint16_t>(currentDialogVal);
                DEBUG("ScreenSetupAudioProc: Live CW offset preview: %u Hz\n", config.data.cwReceiverOffsetHz);
            }
        },
        [this, index, tempValuePtr](UIDialogBase *sender, MessageDialog::DialogResult dialogResult) {
            if (dialogResult == MessageDialog::DialogResult::Accepted) {
                config.data.cwReceiverOffsetHz = static_cast<uint16_t>(*tempValuePtr);
                settingItems[index].value = String(config.data.cwReceiverOffsetHz) + " Hz";
                updateListItem(index);
            }
        },
        Rect(-1, -1, 280, 0));
    this->showDialog(cwOffsetDialog);
}

/**
 * @brief RTTY shift beállítása dialógussal
 *
 * @param index A menüpont indexe a lista frissítéséhez
 */
void ScreenSetupAudioProc::handleRttyShiftDialog(int index) {
    auto tempValuePtr = std::make_shared<int>(static_cast<int>(config.data.rttyShiftHz));

    auto rttyShiftDialog = std::make_shared<ValueChangeDialog>(
        this, "RTTY Shift", "RTTY Shift (Hz):", tempValuePtr.get(),
        static_cast<int>(80),   // Min: 80Hz
        static_cast<int>(1000), // Max: 1000Hz
        static_cast<int>(10),   // Step: 10Hz
        [this, index](const std::variant<int, float, bool> &liveNewValue) {
            if (std::holds_alternative<int>(liveNewValue)) {
                int currentDialogVal = std::get<int>(liveNewValue);
                config.data.rttyShiftHz = static_cast<uint16_t>(currentDialogVal);
                DEBUG("ScreenSetupAudioProc: Live RTTY shift preview: %u Hz\n", config.data.rttyShiftHz);
            }
        },
        [this, index, tempValuePtr](UIDialogBase *sender, MessageDialog::DialogResult dialogResult) {
            if (dialogResult == MessageDialog::DialogResult::Accepted) {
                config.data.rttyShiftHz = static_cast<uint16_t>(*tempValuePtr);
                settingItems[index].value = String(config.data.rttyShiftHz) + " Hz";
                updateListItem(index);
            }
        },
        Rect(-1, -1, 280, 0));
    this->showDialog(rttyShiftDialog);
}

/**
 * @brief RTTY mark frequency beállítása dialógussal
 *
 * @param index A menüpont indexe a lista frissítéséhez
 */
void ScreenSetupAudioProc::handleRttyMarkFrequencyDialog(int index) {
    auto tempValuePtr = std::make_shared<int>(static_cast<int>(config.data.rttyMarkFrequencyHz));

    auto rttyMarkDialog = std::make_shared<ValueChangeDialog>(
        this, "RTTY Mark Freq", "RTTY Mark Frequency (Hz):", tempValuePtr.get(),
        static_cast<int>(1200), // Min: 1200Hz
        static_cast<int>(2500), // Max: 2500Hz
        static_cast<int>(25),   // Step: 25Hz
        [this, index](const std::variant<int, float, bool> &liveNewValue) {
            if (std::holds_alternative<int>(liveNewValue)) {
                int currentDialogVal = std::get<int>(liveNewValue);
                config.data.rttyMarkFrequencyHz = static_cast<uint16_t>(currentDialogVal);
                DEBUG("ScreenSetupAudioProc: Live RTTY mark frequency preview: %u Hz\n", config.data.rttyMarkFrequencyHz);
            }
        },
        [this, index, tempValuePtr](UIDialogBase *sender, MessageDialog::DialogResult dialogResult) {
            if (dialogResult == MessageDialog::DialogResult::Accepted) {
                config.data.rttyMarkFrequencyHz = static_cast<uint16_t>(*tempValuePtr);
                settingItems[index].value = String(config.data.rttyMarkFrequencyHz) + " Hz";
                updateListItem(index);
            }
        },
        Rect(-1, -1, 280, 0));
    this->showDialog(rttyMarkDialog);
}

/**
 * @brief FFT gain érték dekódolása olvasható szöveggé
 *
 * @param value Az FFT gain érték
 * @return Olvasható string reprezentáció
 */
String ScreenSetupAudioProc::decodeFFTGain(float value) {
    if (value == -1.0f)
        return "Disabled";
    else if (value == 0.0f)
        return "Auto Gain";
    else
        return "Manual: " + String(value, 1) + "x";
}

/**
 * @brief FFT gain dialógus kezelése AM vagy FM módhoz
 *
 * @param index A menüpont indexe a lista frissítéséhez
 * @param isAM true = AM mód, false = FM mód
 */
void ScreenSetupAudioProc::handleFFTGainDialog(int index, bool isAM) {
    float &currentConfig = isAM ? config.data.miniAudioFftConfigAm : config.data.miniAudioFftConfigFm;
    const char *title = isAM ? "FFT Gain AM" : "FFT Gain FM";

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
