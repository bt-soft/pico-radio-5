#ifndef __BASE_STATION_STORE_H
#define __BASE_STATION_STORE_H

#include "Band.h"
#include "DebugDataInspector.h"
#include "StationData.h"
#include "StoreBase.h"

/**
 * @brief Template alapú állomás tároló ősosztály
 *
 * Közös funkcionalitás FM és AM állomás tárolókhoz.
 * Eliminálja a kód duplikációt.
 *
 * @tparam StationListType FmStationList_t vagy AmStationList_t
 * @tparam MaxStations MAX_FM_STATIONS vagy MAX_AM_STATIONS
 */
template <typename StationListType, uint8_t MaxStations> class BaseStationStore : public StoreBase<StationListType> {
  public:
    StationListType data;

  protected:
    StationListType &getData() override { return data; }
    const StationListType &getData() const override { return data; }

  public:
    /**
     * @brief Új állomás hozzáadása
     */
    bool addStation(const StationData &newStation) {
        if (data.count >= MaxStations) {
            DEBUG("%s Memory full. Cannot add station.\n", this->getClassName());
            return false;
        }

        // Duplikátum ellenőrzés
        if (isStationExists(newStation)) {
            return false;
        }

        data.stations[data.count] = newStation;
        data.count++;

        DEBUG("%s Station added: %s (Freq: %d)\n", this->getClassName(), newStation.name, newStation.frequency);

        this->checkSave();
        return true;
    }

    /**
     * @brief Állomás frissítése
     */
    bool updateStation(uint8_t index, const StationData &updatedStation) {
        if (index >= data.count) {
            DEBUG("Invalid index for %s station update: %d\n", this->getClassName(), index);
            return false;
        }

        data.stations[index] = updatedStation;
        DEBUG("%s Station updated at index %d: %s\n", this->getClassName(), index, updatedStation.name);
        this->checkSave();
        return true;
    }

    /**
     * @brief Állomás törlése
     */
    bool deleteStation(uint8_t index) {
        if (index >= data.count) {
            DEBUG("Invalid index for %s station delete: %d\n", this->getClassName(), index);
            return false;
        }

        // Elemek eltolása
        for (uint8_t i = index; i < data.count - 1; ++i) {
            data.stations[i] = data.stations[i + 1];
        }
        data.count--;

        // Utolsó elem nullázása
        memset(&data.stations[data.count], 0, sizeof(StationData));

        DEBUG("%s Station deleted at index %d.\n", this->getClassName(), index);
        this->checkSave();
        return true;
    }

    /**
     * @brief Állomás keresése
     */
    int findStation(uint16_t frequency, uint8_t bandIndex, int16_t bfoOffset = 0) {
        for (uint8_t i = 0; i < data.count; ++i) {
            if (data.stations[i].frequency == frequency && data.stations[i].bandIndex == bandIndex) {
                return i;
            }
        }
        return -1;
    }

    // Inline helper metódusok
    inline uint8_t getStationCount() const { return data.count; }

    inline const StationData *getStationByIndex(uint8_t index) const { return (index < data.count) ? &data.stations[index] : nullptr; }

  private:
    /**
     * @brief Ellenőrzi, hogy az állomás már létezik-e
     */
    bool isStationExists(const StationData &newStation) {
        for (uint8_t i = 0; i < data.count; ++i) {
            if (data.stations[i].frequency == newStation.frequency && data.stations[i].bandIndex == newStation.bandIndex) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Ellenőrzi, hogy SSB vagy CW moduláció-e
     */
    inline bool isSSBorCW(uint8_t modulation) const { return (modulation == LSB_DEMOD_TYPE || modulation == USB_DEMOD_TYPE || modulation == CW_DEMOD_TYPE); }
};

#endif // __BASE_STATION_STORE_H
