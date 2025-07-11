#include "ScreenSetup.h"
#include "Config.h"
#include "SystemInfoDialog.h"

/**
 * @brief ScreenSetup konstruktor
 *
 * @param tft TFT_eSPI referencia a kijelző kezeléséhez
 */
ScreenSetup::ScreenSetup() : ScreenSetupBase(SCREEN_NAME_SETUP) { layoutComponents(); }

/**
 * @brief Képernyő címének visszaadása
 *
 * @return A képernyő címe
 */
const char *ScreenSetup::getScreenTitle() const { return "Setup Menu"; }

/**
 * @brief Menüpontok feltöltése fő setup menü elemeivel
 *
 * Ez a metódus feltölti a menüpontokat almenük hivatkozásaival
 * és egyéb fő setup funkciókkal.
 */
void ScreenSetup::populateMenuItems() {
    // Korábbi menüpontok törlése
    settingItems.clear();

    // Fő setup menü elemek hozzáadása
    settingItems.push_back(SettingItem("System Settings", "", static_cast<int>(MainItemAction::DISPLAY_SETTINGS), true, SCREEN_NAME_SETUP_SYSTEM));
    settingItems.push_back(SettingItem("Si4735 Settings", "", static_cast<int>(MainItemAction::SI4735_SETTINGS), true, SCREEN_NAME_SETUP_SI4735));
    settingItems.push_back(SettingItem("CW/RTTY Settings", "", static_cast<int>(MainItemAction::CW_RTTY_SETTINGS), true, SCREEN_NAME_SETUP_CW_RTTY));
    settingItems.push_back(SettingItem("System Information", "", static_cast<int>(MainItemAction::INFO)));
    settingItems.push_back(SettingItem("Factory Reset", "", static_cast<int>(MainItemAction::FACTORY_RESET)));

    // Lista komponens újrarajzolásának kérése, ha létezik
    if (menuList) {
        menuList->markForRedraw();
    }
}

/**
 * @brief Menüpont akció kezelése
 *
 * Ez a metódus kezeli a fő setup menü kattintásait.
 * Almenük esetén a navigáció a ScreenSetupBase-ben történik.
 *
 * @param index A menüpont indexe
 * @param action Az akció azonosító
 */
void ScreenSetup::handleItemAction(int index, int action) {
    MainItemAction mainAction = static_cast<MainItemAction>(action);

    switch (mainAction) {
        case MainItemAction::DISPLAY_SETTINGS:
        case MainItemAction::SI4735_SETTINGS:
        case MainItemAction::DECODER_SETTINGS:
        case MainItemAction::CW_RTTY_SETTINGS:
            // Ezeket a ScreenSetupBase::onItemClicked kezeli (almenü navigáció)
            break;
        case MainItemAction::INFO:
            handleSystemInfoDialog();
            break;
        case MainItemAction::FACTORY_RESET:
            handleFactoryResetDialog();
            break;
        case MainItemAction::NONE:
        default:
            DEBUG("ScreenSetup: Unknown action: %d\n", action);
            break;
    }
}

/**
 * @brief Rendszer információ dialógus megjelenítése
 */
void ScreenSetup::handleSystemInfoDialog() {
    auto systemInfoDialog = std::make_shared<SystemInfoDialog>( //
        this,                                                   //
        Rect(-1, -1, ::SCREEN_W * 3 / 4, ::SCREEN_H * 3 / 4)    //
    );
    this->showDialog(systemInfoDialog);
}

/**
 * @brief Gyári beállítások visszaállítása dialógussal
 */
void ScreenSetup::handleFactoryResetDialog() {
    auto confirmDialog = std::make_shared<MessageDialog>(            //
        this,                                                        //
        "Factory Reset",                                             //
        "Reset all settings to defaults?\n\nThis cannot be undone!", //
        MessageDialog::ButtonsType::YesNo,                           //
        Rect(-1, -1, ::SCREEN_W * 3 / 4, 0));                        //

    confirmDialog->setDialogCallback([this](UIDialogBase *sender, MessageDialog::DialogResult result) {
        if (result == MessageDialog::DialogResult::Accepted) {
            config.loadDefaults();
            config.forceSave();
            populateMenuItems(); // Frissítés nem szükséges itt, mert ez a főmenü
        }
    });

    this->showDialog(confirmDialog);
}
