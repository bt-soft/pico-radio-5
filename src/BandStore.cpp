#include "BandStore.h"
#include "Band.h" // BandTable struktúra definíciójához

/**
 * BandTable változó adatainak betöltése a tárolt adatokból
 * @param bandTable A BandTable tömb referenciája
 */
void BandStore::loadToBandTable(BandTable *bandTable) {
    for (uint8_t i = 0; i < BANDTABLE_SIZE; i++) {
        if (data.bands[i].currFreq != 0) {
            bandTable[i].currFreq = data.bands[i].currFreq;
            bandTable[i].currStep = data.bands[i].currStep;
            bandTable[i].currDemod = data.bands[i].currMod;
            bandTable[i].antCap = data.bands[i].antCap;
        }
    }
}

/**
 * BandTable változó adatainak mentése a store-ba
 * @param bandTable A BandTable tömb referenciája
 */
void BandStore::saveFromBandTable(const BandTable *bandTable) {
    for (uint8_t i = 0; i < BANDTABLE_SIZE; i++) {
        data.bands[i].currFreq = bandTable[i].currFreq;
        data.bands[i].currStep = bandTable[i].currStep;
        data.bands[i].currMod = bandTable[i].currDemod;
        data.bands[i].antCap = bandTable[i].antCap;
    }
    // Az adatok megváltoztak, a checkSave() fogja észlelni
}
