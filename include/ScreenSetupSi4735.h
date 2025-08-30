#pragma once

#include "ScreenSetupBase.h"

/**
 * @brief Si4735 rádió chip beállítások képernyő.
 *
 * Ez a képernyő a Si4735 rádió chip specifikus beállításait kezeli:
 * - Zajzár (squelch) alapjának kiválasztása (RSSI/SNR)
 * - FFT konfigurációk AM és FM módokhoz
 * - Egyéb Si4735 specifikus paraméterek
 */
class ScreenSetupSi4735 : public ScreenSetupBase {
  private:
    /**
     * @brief Si4735 specifikus menüpont akciók
     */
    enum class Si4735ItemAction { NONE = 0, SQUELCH_BASIS = 100, FFT_CONFIG_AM = 101, FFT_CONFIG_FM = 102, VOLUME_LEVEL = 103, AUDIO_MUTE = 104, STEREO_THRESHOLD = 105 };

    // Segédfüggvények

    // Si4735 specifikus dialógus kezelő függvények
    void handleSquelchBasisDialog(int index);
    void handleVolumeLevelDialog(int index);
    void handleToggleItem(int index, bool &configValue);
    void handleStereoThresholdDialog(int index);

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
    ScreenSetupSi4735();
    virtual ~ScreenSetupSi4735() = default;
};
