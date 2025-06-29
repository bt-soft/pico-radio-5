#ifndef __STATIONSTORE_H
#define __STATIONSTORE_H

// Először a típusdefiníciók kellenek
#include "BaseStationStore.h"
#include "DebugDataInspector.h"
#include "EepromLayout.h" // EEPROM címek konstansokhoz
#include "StationData.h"

// Üres alapértelmezett listák deklarációja (definíció a .cpp fájlban)
extern const FmStationList_t DEFAULT_FM_STATIONS;
extern const AmStationList_t DEFAULT_AM_STATIONS;

// --- FM Station Store ---
class FmStationStore : public BaseStationStore<FmStationList_t, MAX_FM_STATIONS> {
  protected:
    const char *getClassName() const override { return "FmStationStore"; }

    // Felülírjuk a mentést/betöltést a helyes címmel és névvel
    uint16_t performSave() override {
        uint16_t savedCrc = StoreEepromBase<FmStationList_t>::save(getData(), EEPROM_FM_STATIONS_ADDR, getClassName());
#ifdef __DEBUG
        if (savedCrc != 0)
            DebugDataInspector::printFmStationData(getData());
#endif
        return savedCrc;
    }

    uint16_t performLoad() override {
        uint16_t loadedCrc = StoreEepromBase<FmStationList_t>::load(getData(), EEPROM_FM_STATIONS_ADDR, getClassName());
#ifdef __DEBUG
        DebugDataInspector::printFmStationData(getData());
#endif
        // Count ellenőrzés
        if (data.count > MAX_FM_STATIONS) {
            DEBUG("[%s] Warning: FM station count corrected from %d to %d.\n", getClassName(), data.count, MAX_FM_STATIONS);
            data.count = MAX_FM_STATIONS;
        }
        return loadedCrc;
    }

  public:
    FmStationStore() : BaseStationStore<FmStationList_t, MAX_FM_STATIONS>() { data = DEFAULT_FM_STATIONS; }

    void loadDefaults() override {
        memcpy(&data, &DEFAULT_FM_STATIONS, sizeof(FmStationList_t));
        data.count = 0;
        DEBUG("FM Station defaults loaded.\n");
    }
};

// --- AM Station Store ---
class AmStationStore : public BaseStationStore<AmStationList_t, MAX_AM_STATIONS> {
  protected:
    const char *getClassName() const override { return "AmStationStore"; }

    // Felülírjuk a mentést/betöltést a helyes címmel és névvel
    uint16_t performSave() override {
        uint16_t savedCrc = StoreEepromBase<AmStationList_t>::save(getData(), EEPROM_AM_STATIONS_ADDR, getClassName());
#ifdef __DEBUG
        if (savedCrc != 0)
            DebugDataInspector::printAmStationData(getData());
#endif
        return savedCrc;
    }

    uint16_t performLoad() override {
        uint16_t loadedCrc = StoreEepromBase<AmStationList_t>::load(getData(), EEPROM_AM_STATIONS_ADDR, getClassName());
#ifdef __DEBUG
        DebugDataInspector::printAmStationData(getData());
#endif
        // Count ellenőrzés
        if (data.count > MAX_AM_STATIONS) {
            DEBUG("[%s] Warning: AM station count corrected from %d to %d.\n", getClassName(), data.count, MAX_AM_STATIONS);
            data.count = MAX_AM_STATIONS;
        }
        return loadedCrc;
    }

  public:
    AmStationStore() : BaseStationStore<AmStationList_t, MAX_AM_STATIONS>() { data = DEFAULT_AM_STATIONS; }

    void loadDefaults() override {
        memcpy(&data, &DEFAULT_AM_STATIONS, sizeof(AmStationList_t));
        data.count = 0;
        DEBUG("AM Station defaults loaded.\n");
    }
};

// Globális példányok deklarációja (definíció a .cpp fájlban)
extern FmStationStore fmStationStore;
extern AmStationStore amStationStore;

#endif // __STATIONSTORE_H
