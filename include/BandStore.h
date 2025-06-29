#ifndef __BAND_STORE_H
#define __BAND_STORE_H

#include "DebugDataInspector.h" // Debug kiíráshoz
#include "EepromLayout.h"       // EEPROM_BAND_DATA_ADDR konstanshoz
#include "StoreBase.h"
#include "defines.h"

// Forward deklaráció a Band osztályhoz (de a BandTable struktúrát kell include-olni)
class Band;
struct BandTable; // A Band.h-ból lesz ismert

// BandTable változó adatainak struktúrája (mentéshez)
struct BandTableData_t {
    uint16_t currFreq; // Aktuális frekvencia
    uint8_t currStep;  // Aktuális lépésköz
    uint8_t currMod;   // Aktuális moduláció
    uint16_t antCap;   // Antenna capacitor
};

// Teljes Band adatok struktúrája (az összes band számára)
struct BandStoreData_t {
    BandTableData_t bands[BANDTABLE_SIZE]; // Band tábla mérete (defines.h-ból)
};

/**
 * Band adatok mentését és betöltését kezelő osztály
 */
class BandStore : public StoreBase<BandStoreData_t> {
  public:
    // A band adatok, alapértelmezett értékek (0) lesznek a konstruktorban
    BandStoreData_t data;

  protected:
    const char *getClassName() const override { return "BandStore"; }

    /**
     * Referencia az adattagra, csak az ős használja
     */
    BandStoreData_t &getData() override { return data; };

    /**
     * Const referencia az adattagra, CRC számításhoz
     */
    const BandStoreData_t &getData() const override { return data; }; // Felülírjuk a mentést/betöltést a megfelelő EEPROM címmel
    uint16_t performSave() override {
        uint16_t result = StoreEepromBase<BandStoreData_t>::save(getData(), EEPROM_BAND_DATA_ADDR, getClassName());
#ifdef __DEBUG
        DebugDataInspector::printBandStoreData(getData());
#endif
        return result;
    }

    uint16_t performLoad() override {
        uint16_t result = StoreEepromBase<BandStoreData_t>::load(getData(), EEPROM_BAND_DATA_ADDR, getClassName());
#ifdef __DEBUG
        DebugDataInspector::printBandStoreData(getData());
#endif
        return result;
    }

  public:
    /**
     * Konstruktor - inicializálja az adatokat nullákkal
     */
    BandStore() {
        // Minden band adatát nullázzuk inicializáláskor
        for (uint8_t i = 0; i < BANDTABLE_SIZE; i++) {
            data.bands[i].currFreq = 0;
            data.bands[i].currStep = 0;
            data.bands[i].currMod = 0;
            data.bands[i].antCap = 0;
        }
    }

    /**
     * Alapértelmezett értékek betöltése - minden adat nullázása
     */
    void loadDefaults() override {
        for (uint8_t i = 0; i < BANDTABLE_SIZE; i++) {
            data.bands[i].currFreq = 0;
            data.bands[i].currStep = 0;
            data.bands[i].currMod = 0;
            data.bands[i].antCap = 0;
        }
        DEBUG("BandStore defaults loaded.\n");
    }

    /**
     * BandTable változó adatainak betöltése a tárolt adatokból
     * @param bandTable A BandTable tömb referenciája
     */
    void loadToBandTable(BandTable *bandTable);

    /**
     * BandTable változó adatainak mentése a store-ba
     * @param bandTable A BandTable tömb referenciája
     */
    void saveFromBandTable(const BandTable *bandTable);
};

#endif // __BAND_STORE_H
