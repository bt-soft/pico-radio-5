#pragma once

#include "ScreenSetupBase.h"

/**
 * @brief Audió feldolgozás beállítások képernyő.
 *
 * Ez a képernyő az audió feldolgozás specifikus beállításait kezeli:
 * - CW receiver offset beállítása (400Hz - 1900Hz)
 * - RTTY shift beállítása (80Hz - 1000Hz)
 * - RTTY mark frequency beállítása (1200Hz - 2500Hz)
 * - FFT konfigurációk AM és FM módokhoz
 */
class ScreenSetupAudioProc : public ScreenSetupBase {
  private:
    /**
     * @brief Audió feldolgozás specifikus menüpont akciók
     */
    enum class AudioProcItemAction {
        NONE = 0,
        CW_TONE_FREQUENCY = 400,
        CW_RTTY_LED_DEBUG, // CW/RTTY LED debug jelzés engedélyezése
        RTTY_SHIFT,
        RTTY_MARK_FREQUENCY,
        FFT_GAIN_AM,
        FFT_GAIN_FM,
    };

    // Segédfüggvények
    String decodeFFTGain(float value);

    // Audió feldolgozás specifikus dialógus kezelő függvények
    void handleCwToneFrequencyDialog(int index);
    void handleRttyShiftDialog(int index);
    void handleRttyMarkFrequencyDialog(int index);
    void handleFFTGainDialog(int index, bool isAM);
    void handleToggleItem(int index, bool &configValue);

  protected:
    // SetupScreenBase virtuális metódusok implementációja
    virtual void populateMenuItems() override;
    virtual void handleItemAction(int index, int action) override;
    virtual const char *getScreenTitle() const override;

  public:
    /**
     * @brief Konstruktor.
     */
    ScreenSetupAudioProc();
    virtual ~ScreenSetupAudioProc() = default;
};
