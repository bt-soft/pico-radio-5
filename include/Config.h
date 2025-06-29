#ifndef __CONFIG_H
#define __CONFIG_H

#include "ConfigData.h"
#include "DebugDataInspector.h" // Szükséges a debug kiíratáshoz
#include "StoreBase.h"
#include "utils.h" // Utils::setTftBacklight függvényhez

// Alapértelmezett konfigurációs adatok (readonly, const)
extern const Config_t DEFAULT_CONFIG;

/**
 * Konfigurációs adatok kezelése
 */
class Config : public StoreBase<Config_t> {
  public:
    // A 'config' változó, alapértelmezett értékeket veszi fel a konstruktorban
    // Szándékosan public, nem kell a sok getter egy embedded rendszerben
    Config_t data;

  protected:
    const char *getClassName() const override { return "Config"; }

    /**
     * Referencia az adattagra, csak az ős használja
     */
    Config_t &getData() override { return data; };

    /**
     * Const referencia az adattagra, CRC számításhoz
     */
    const Config_t &getData() const override { return data; };

    // Felülírjuk a mentést/betöltést a debug kiíratás hozzáadásához
    uint16_t performSave() override {
        uint16_t savedCrc = StoreEepromBase<Config_t>::save(getData(), 0, getClassName());
#ifdef __DEBUG
        if (savedCrc != 0) {
            DebugDataInspector::printConfigData(getData());
        }
#endif
        return savedCrc;
    }
    uint16_t performLoad() override {
        uint16_t loadedCrc = StoreEepromBase<Config_t>::load(getData(), 0, getClassName());
#ifdef __DEBUG
        DebugDataInspector::printConfigData(getData()); // Akkor is kiírjuk, ha defaultot töltött
#endif
        uint8_t currentTimeout = data.screenSaverTimeoutMinutes;
        if (currentTimeout < SCREEN_SAVER_TIMEOUT_MIN || currentTimeout > SCREEN_SAVER_TIMEOUT_MAX) {
            data.screenSaverTimeoutMinutes = SCREEN_SAVER_TIMEOUT;
            // A 'data' módosítása miatt a checkSave() később észlelni fogja az
            // eltérést a lastCRC-hez képest (amit a loadedCrc alapján állít be a
            // StoreBase), és menteni fogja a javított adatot.
        }

        // Háttérvilágítás beállítása a betöltött konfiguráció alapján
        Utils::setTftBacklight(data.tftBackgroundBrightness);

        return loadedCrc;
    }

  public:
    /**
     * Konstruktor
     * @param pData Pointer a konfigurációs adatokhoz
     */
    Config() : StoreBase<Config_t>(), data(DEFAULT_CONFIG) {}

    /**
     * Alapértelmezett adatok betöltése
     */
    void loadDefaults() override {
        memcpy(&data, &DEFAULT_CONFIG, sizeof(Config_t));
        Utils::setTftBacklight(data.tftBackgroundBrightness); // Háttérvilágítás beállítása DC/PWM módban
    }
};

// Globális config példány deklaráció
extern Config config;

#endif // __CONFIG_H
