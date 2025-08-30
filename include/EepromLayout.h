#pragma once

#include "ConfigData.h"  // Config_t struktúra
#include "StationData.h" // FmStationList_t, AmStationList_t struktúrák
#include "StoreEepromBase.h"
#include "defines.h" // BANDTABLE_SIZE konstanshoz

// Forward deklaráció a BandStoreData_t-hez
struct BandStoreData_t;

/**
 * @brief EEPROM memória layout központi kezelése
 *
 * Ez a fájl központilag kezeli az EEPROM területek kiosztását,
 * biztosítva, hogy ne legyenek átfedések és minden komponens
 * tudja, hol található az adott adatblokk.
 *
 * EEPROM Layout struktúra:
 * ┌─────────────────┬──────────┬─────────────────────────────────────┐
 * │ Komponens       │ Cím      │ Méret                               │
 * ├─────────────────┼──────────┼─────────────────────────────────────┤
 * │ Config          │ 0x0000   │ CONFIG_REQUIRED_SIZE bytes          │
 * │ Band Data       │ Config+  │ BAND_STORE_REQUIRED_SIZE bytes      │
 * │ FM Stations     │ Bands+   │ FM_STATIONS_REQUIRED_SIZE bytes     │
 * │ AM Stations     │ FM+      │ AM_STATIONS_REQUIRED_SIZE bytes     │
 * └─────────────────┴──────────┴─────────────────────────────────────┘
 */

// ============================================
// EEPROM TERÜLETEK DEFINÍCIÓI
// ============================================

/** Config terület kezdőcíme (mindig 0) */
constexpr uint16_t EEPROM_CONFIG_START_ADDR = 0;

/** Config terület mérete */
constexpr size_t CONFIG_REQUIRED_SIZE = StoreEepromBase<Config_t>::getRequiredSize();

/** Band adatok kezdőcíme */
constexpr uint16_t EEPROM_BAND_DATA_ADDR = EEPROM_CONFIG_START_ADDR + CONFIG_REQUIRED_SIZE;

/** Band adatok mérete: BANDTABLE_SIZE band × (2+1+1+2) bájt + 2 bájt CRC */
constexpr size_t BAND_STORE_REQUIRED_SIZE = (BANDTABLE_SIZE * (sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t))) + sizeof(uint16_t);

/** FM állomások kezdőcíme */
constexpr uint16_t EEPROM_FM_STATIONS_ADDR = EEPROM_BAND_DATA_ADDR + BAND_STORE_REQUIRED_SIZE;

/** FM állomások mérete */
constexpr size_t FM_STATIONS_REQUIRED_SIZE = StoreEepromBase<FmStationList_t>::getRequiredSize();

/** AM állomások kezdőcíme */
constexpr uint16_t EEPROM_AM_STATIONS_ADDR = EEPROM_FM_STATIONS_ADDR + FM_STATIONS_REQUIRED_SIZE;

/** AM állomások mérete */
constexpr size_t AM_STATIONS_REQUIRED_SIZE = StoreEepromBase<AmStationList_t>::getRequiredSize();

/** Teljes használt EEPROM méret */
constexpr size_t EEPROM_TOTAL_USED = EEPROM_AM_STATIONS_ADDR + AM_STATIONS_REQUIRED_SIZE;

/** Szabad EEPROM terület */
constexpr size_t EEPROM_FREE_SPACE = EEPROM_SIZE - EEPROM_TOTAL_USED;

// ============================================
// VALIDÁCIÓ
// ============================================

/** Fordítási idejű ellenőrzés az EEPROM méretére */
static_assert(EEPROM_TOTAL_USED <= EEPROM_SIZE, "EEPROM layout exceeds available space! Increase EEPROM_SIZE or reduce data structures.");
