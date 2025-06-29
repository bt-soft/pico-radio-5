#ifndef __SI4735_RUNTIME_H
#define __SI4735_RUNTIME_H

#include "Si4735Base.h"

/**
 * @brief Struktúra a rádió jel minőségi adatainak tárolására
 */
struct SignalQualityData {
    uint8_t rssi;       // RSSI érték (0-127)
    uint8_t snr;        // SNR érték (0-127)
    uint32_t timestamp; // Utolsó frissítés időbélyege (millis())
    bool isValid;       // Adatok érvényességi flag

    SignalQualityData() : rssi(0), snr(0), timestamp(0), isValid(false) {}
};

class Si4735Runtime : public Si4735Base {
  public:
    // AGC beállítási lehetőségek
    enum class AgcGainMode : uint8_t {
        Off = 0,       // AGC kikapcsolva (de technikailag aktív marad, csak a csillapítás 0)
        Automatic = 1, // AGC engedélyezve (teljesen automatikus működés)
        Manual = 2     // AGC manuális beállítással (a config.data.currentAGCgain értékével)
    };

  private:
    uint32_t hardwareAudioMuteElapsed; // SI4735 hardware audio mute állapot start ideje
    bool isSquelchMuted = false;       // Kezdetben nincs némítva a squelch miatt
    bool hardwareAudioMuteState;       // SI4735 hardware audio mute állapot

    // Signal quality cache
    SignalQualityData signalCache;

  protected:
    void manageHardwareAudioMute();
    void manageSquelch();

    /**
     * @brief Frissíti a signal quality cache-t
     */
    void updateSignalCache();

  public:
    Si4735Runtime() : Si4735Base(), hardwareAudioMuteState(false), hardwareAudioMuteElapsed(millis()) {};

    void hardwareAudioMuteOn();
    void checkAGC();

    /**
     * @brief Frissíti a signal quality cache-t, ha szükséges
     */
    void updateSignalCacheIfNeeded();

    /**
     * @brief Invalidálja a signal quality cache-t (következő lekérdezéskor frissül)
     */
    void invalidateSignalCache();

    /**
     * @brief Lekéri a signal quality adatokat cache-elt módon (max 1mp késleltetés)
     * @return SignalQualityData A cache-elt signal quality adatok
     */
    SignalQualityData getSignalQuality();

    /**
     * @brief Lekéri a signal quality adatokat valós időben (közvetlen chip lekérdezés)
     * @return SignalQualityData A friss signal quality adatok
     */
    SignalQualityData getSignalQualityRealtime();

    /**
     * @brief Lekéri csak az RSSI értéket cache-elt módon
     * @return uint8_t RSSI érték
     */
    uint8_t getRSSI();

    /**
     * @brief Lekéri csak az SNR értéket cache-elt módon
     * @return uint8_t SNR érték
     */
    uint8_t getSNR();
};

#endif // __SI4735_RUNTIME_H