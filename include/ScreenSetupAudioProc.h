#ifndef __SCREEN_SETUP_AUDIO_PROC_H
#define __SCREEN_SETUP_AUDIO_PROC_H

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
        CW_RECEIVER_OFFSET = 400,
        RTTY_SHIFT,
        RTTY_MARK_FREQUENCY,
        FFT_CONFIG_AM,
        FFT_CONFIG_FM,
    };

    // Segédfüggvények
    String decodeFFTConfig(float value);

    // Audió feldolgozás specifikus dialógus kezelő függvények
    void handleCwOffsetDialog(int index);
    void handleRttyShiftDialog(int index);
    void handleRttyMarkFrequencyDialog(int index);
    void handleFFTConfigDialog(int index, bool isAM);

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

#endif // __SCREEN_SETUP_AUDIO_PROC_H
