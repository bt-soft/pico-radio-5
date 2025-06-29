#ifndef __STATIONDATA_H
#define __STATIONDATA_H

#include <Arduino.h>

#include "ConfigData.h"  // Szükséges a Config_t miatt

// Maximális állomások száma FM és AM sávokra
#define MAX_FM_STATIONS 40
#define MAX_AM_STATIONS 40

// Állomásnév konstansok
#define MAX_STATION_NAME_LEN 15
#define STATION_NAME_BUFFER_SIZE \
  16  // MAX_STATION_NAME_LEN + 1 a null terminátornak

/**
 * @brief Egyetlen állomás adatainak struktúrája
 */
struct StationData {
  uint8_t bandIndex;       // A band indexe (pl. FM, MW, SW, stb.)
  uint16_t frequency;      // Állomás frekvenciája (kHz)
  uint8_t modulation;      // Demodulációs mód indexe (FM, AM, LSB, USB, CW)
  uint8_t bandwidthIndex;  // Sávszélesség indexe
  char name[STATION_NAME_BUFFER_SIZE];  // Állomás neve (15 karakter + null
                                        // terminátor)
};

// FM állomások listája
struct FmStationList_t {
  StationData stations[MAX_FM_STATIONS];
  uint8_t count = 0;  // Tárolt állomások száma
};

// AM (és egyéb) állomások listája
struct AmStationList_t {
  StationData stations[MAX_AM_STATIONS];
  uint8_t count = 0;  // Tárolt állomások száma
};

#endif  // __STATIONDATA_H
