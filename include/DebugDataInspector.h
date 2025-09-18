#pragma once

#include <Arduino.h>

struct Config_t;        // Elődeklaráció a Config_t-hez, hogy megszakítsuk az include ciklust
struct BandStoreData_t; // Elődeklaráció a BandStoreData_t-hez a sáv debuggolásához

// StationData include a lista típusokhoz
#include "StationData.h" // FmStationList_t, AmStationList_t, StationData definíciók

// A Config.h-t itt már nem includoljuk, mert az körkörös függőséget okoz.
// A Config.h includolja ezt a fájlt, és mire a printConfigData inline
// definíciójához ér a fordító, addigra a Config_t már definiálva lesz a
// Config.h-ban.

class DebugDataInspector {
  public:
    /**
     * @brief Kiírja a Config struktúra tartalmát a soros portra.
     * @param config A Config objektum.
     */
    static void printConfigData(const Config_t &configData); // Csak a deklaráció marad

    /**
     * @brief Kiírja az FM állomáslista tartalmát a soros portra.
     * @param fmStore Az FM állomáslista objektum.
     */
    static void printFmStationData(const FmStationList_t &fmData); // Csak a deklaráció marad

    /**
     * @brief Kiírja az AM állomáslista tartalmát a soros portra.
     * @param amStore Az AM állomáslista objektum.
     */
    static void printAmStationData(const AmStationList_t &amData); // Csak a deklaráció marad

    /**
     * @brief Kiírja a Band adatok tartalmát a soros portra.
     * @param bandData A Band store adatok.
     */
    static void printBandStoreData(const BandStoreData_t &bandData); // Band adatok debug kiírása
};