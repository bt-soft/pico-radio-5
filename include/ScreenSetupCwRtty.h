#ifndef __SCREEN_SETUP_CW_RTTY_H
#define __SCREEN_SETUP_CW_RTTY_H

#include "ScreenSetupBase.h"

/**
 * @brief CW és RTTY beállítások képernyő.
 *
 * Ez a képernyő a CW és RTTY modulációs beállításait kezeli:
 * - CW receiver offset beállítása (400Hz - 1900Hz)
 * - RTTY shift beállítása (80Hz - 1000Hz)
 * - RTTY mark frequency beállítása (1200Hz - 2500Hz)
 */
class ScreenSetupCwRtty : public ScreenSetupBase {
  private:
    /**
     * @brief CW/RTTY specifikus menüpont akciók
     */
    enum class CwRttyItemAction {
        NONE = 0,
        CW_RECEIVER_OFFSET = 400,
        RTTY_SHIFT,
        RTTY_MARK_FREQUENCY,
    };

    // CW/RTTY specifikus dialógus kezelő függvények
    void handleCwOffsetDialog(int index);
    void handleRttyShiftDialog(int index);
    void handleRttyMarkFrequencyDialog(int index);

  protected:
    // SetupScreenBase virtuális metódusok implementációja
    virtual void populateMenuItems() override;
    virtual void handleItemAction(int index, int action) override;
    virtual const char *getScreenTitle() const override;

  public:
    /**
     * @brief Konstruktor.
     */
    ScreenSetupCwRtty();
    virtual ~ScreenSetupCwRtty() = default;
};

#endif // __SCREEN_SETUP_CW_RTTY_H
