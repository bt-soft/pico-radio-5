#include "ScreenSetupCwRtty.h"
#include "Config.h"
#include "ValueChangeDialog.h"

/**
 * @brief ScreenSetupCwRtty konstruktor
 */
ScreenSetupCwRtty::ScreenSetupCwRtty() : ScreenSetupBase(SCREEN_NAME_SETUP_CW_RTTY) { layoutComponents(); }

/**
 * @brief Képernyő címének visszaadása
 *
 * @return A képernyő címe
 */
const char *ScreenSetupCwRtty::getScreenTitle() const { return "CW/RTTY Settings"; }

/**
 * @brief Menüpontok feltöltése CW/RTTY specifikus beállításokkal
 *
 * Ez a metódus feltölti a menüpontokat a CW/RTTY aktuális
 * konfigurációs értékeivel.
 */
void ScreenSetupCwRtty::populateMenuItems() {
    // Korábbi menüpontok törlése
    settingItems.clear();

    settingItems.push_back(SettingItem("CW Receiver Offset", String(config.data.cwReceiverOffsetHz) + " Hz", static_cast<int>(CwRttyItemAction::CW_RECEIVER_OFFSET)));
    settingItems.push_back(SettingItem("RTTY Shift", String(config.data.rttyShiftHz) + " Hz", static_cast<int>(CwRttyItemAction::RTTY_SHIFT)));
    settingItems.push_back(SettingItem("RTTY Mark Frequency", String(config.data.rttyMarkFrequencyHz) + " Hz", static_cast<int>(CwRttyItemAction::RTTY_MARK_FREQUENCY)));

    // Lista komponens újrarajzolásának kérése, ha létezik
    if (menuList) {
        menuList->markForRedraw();
    }
}

/**
 * @brief Menüpont akció kezelése
 *
 * Ez a metódus kezeli a CW/RTTY specifikus menüpontok kattintásait.
 *
 * @param index A menüpont indexe
 * @param action Az akció azonosító
 */
void ScreenSetupCwRtty::handleItemAction(int index, int action) {
    CwRttyItemAction cwRttyAction = static_cast<CwRttyItemAction>(action);

    switch (cwRttyAction) {
        case CwRttyItemAction::CW_RECEIVER_OFFSET:
            handleCwOffsetDialog(index);
            break;
        case CwRttyItemAction::RTTY_SHIFT:
            handleRttyShiftDialog(index);
            break;
        case CwRttyItemAction::RTTY_MARK_FREQUENCY:
            handleRttyMarkFrequencyDialog(index);
            break;
        case CwRttyItemAction::NONE:
        default:
            DEBUG("ScreenSetupCwRtty: Unknown action: %d\n", action);
            break;
    }
}

/**
 * @brief CW receiver offset beállítása dialógussal
 *
 * @param index A menüpont indexe a lista frissítéséhez
 */
void ScreenSetupCwRtty::handleCwOffsetDialog(int index) {
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
                DEBUG("ScreenSetupCwRtty: Live CW offset preview: %u Hz\n", config.data.cwReceiverOffsetHz);
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
void ScreenSetupCwRtty::handleRttyShiftDialog(int index) {
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
                DEBUG("ScreenSetupCwRtty: Live RTTY shift preview: %u Hz\n", config.data.rttyShiftHz);
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
void ScreenSetupCwRtty::handleRttyMarkFrequencyDialog(int index) {
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
                DEBUG("ScreenSetupCwRtty: Live RTTY mark frequency preview: %u Hz\n", config.data.rttyMarkFrequencyHz);
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
