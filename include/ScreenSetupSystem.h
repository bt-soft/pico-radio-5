#pragma once

#include "ScreenSetupBase.h"

/**
 * @brief Rendszer beállítások képernyő.
 *
 * Ez a képernyő a rendszer és felhasználói felület beállításait kezeli:
 * - TFT háttérvilágítás fényességének beállítása
 * - Képernyővédő időtúllépésének beállítása
 * - Inaktív számjegyek világítása
 * - Hangjelzések engedélyezése
 * - Rotary encoder gyorsítás beállítása
 */
class ScreenSetupSystem : public ScreenSetupBase {
  private:
    /**
     * @brief Rendszer specifikus menüpont akciók
     */
    enum class SystemItemAction {
        NONE = 0,
        BRIGHTNESS = 300,
        SAVER_TIMEOUT,
        INACTIVE_DIGIT_LIGHT,
        BEEPER_ENABLED,
        ROTARY_ACCELERATION, // Rotary gyorsítás beállítása
    };

    // Rendszer specifikus dialógus kezelő függvények
    void handleBrightnessDialog(int index);
    void handleSaverTimeoutDialog(int index);
    void handleToggleItem(int index, bool &configValue);

  protected:
    // SetupScreenBase virtuális metódusok implementációja
    virtual void populateMenuItems() override;
    virtual void handleItemAction(int index, int action) override;
    virtual const char *getScreenTitle() const override;

  public:
    /**
     * @brief Konstruktor.
     * @param tft TFT_eSPI referencia.
     */
    ScreenSetupSystem();
    virtual ~ScreenSetupSystem() = default;
};
