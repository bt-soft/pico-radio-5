#include "Band.h"
#include "BandStore.h"

#include "rtVars.h"

// Egyszerűsített BandTable tömb
BandTable bandTable[] = {
    {"FM", FM_BAND_TYPE, FM_DEMOD_TYPE, 8750, 10800, 9390, 10, false, 0, 0, 0, 0},   //  FM          0   // 93.9MHz, a 64MHz-es sávot nem használjuk
    {"LW", LW_BAND_TYPE, AM_DEMOD_TYPE, 100, 514, 198, 9, false, 0, 0, 0, 0},        //  LW          1
    {"MW", MW_BAND_TYPE, AM_DEMOD_TYPE, 514, 1800, 540, 9, false, 0, 0, 0, 0},       //  MW          2   // 540kHz Kossuth
    {"800m", SW_BAND_TYPE, AM_DEMOD_TYPE, 280, 470, 284, 1, true, 0, 0, 0, 0},       // Ham  800M    3
    {"630m", SW_BAND_TYPE, LSB_DEMOD_TYPE, 470, 480, 475, 1, true, 0, 0, 0, 0},      // Ham  630M    4
    {"160m", SW_BAND_TYPE, LSB_DEMOD_TYPE, 1800, 2000, 1850, 1, true, 0, 0, 0, 0},   // Ham  160M    5
    {"120m", SW_BAND_TYPE, AM_DEMOD_TYPE, 2000, 3200, 2400, 5, false, 0, 0, 0, 0},   //      120M    6
    {"90m", SW_BAND_TYPE, AM_DEMOD_TYPE, 3200, 3500, 3300, 5, false, 0, 0, 0, 0},    //       90M    7
    {"80m", SW_BAND_TYPE, LSB_DEMOD_TYPE, 3500, 3900, 3630, 1, true, 0, 0, 0, 0},    // Ham   80M    8
    {"75m", SW_BAND_TYPE, AM_DEMOD_TYPE, 3900, 5300, 3950, 5, false, 0, 0, 0, 0},    //       75M    9
    {"60m", SW_BAND_TYPE, USB_DEMOD_TYPE, 5300, 5900, 5375, 1, true, 0, 0, 0, 0},    // Ham   60M   10
    {"49m", SW_BAND_TYPE, AM_DEMOD_TYPE, 5900, 7000, 6000, 5, false, 0, 0, 0, 0},    //       49M   11
    {"40m", SW_BAND_TYPE, LSB_DEMOD_TYPE, 7000, 7500, 7070, 1, true, 0, 0, 0, 0},    // Ham   40M   12
    {"41m", SW_BAND_TYPE, AM_DEMOD_TYPE, 7200, 9000, 7210, 5, false, 0, 0, 0, 0},    //       41M   13
    {"31m", SW_BAND_TYPE, AM_DEMOD_TYPE, 9000, 10000, 9600, 5, false, 0, 0, 0, 0},   //       31M   14
    {"30m", SW_BAND_TYPE, USB_DEMOD_TYPE, 10000, 10100, 10100, 1, true, 0, 0, 0, 0}, // Ham   30M   15
    {"25m", SW_BAND_TYPE, AM_DEMOD_TYPE, 10200, 13500, 11700, 5, false, 0, 0, 0, 0}, //       25M   16
    {"22m", SW_BAND_TYPE, AM_DEMOD_TYPE, 13500, 14000, 13700, 5, false, 0, 0, 0, 0}, //       22M   17
    {"20m", SW_BAND_TYPE, USB_DEMOD_TYPE, 14000, 14500, 14074, 1, true, 0, 0, 0, 0}, // Ham   20M   18
    {"19m", SW_BAND_TYPE, AM_DEMOD_TYPE, 14500, 17500, 15700, 5, false, 0, 0, 0, 0}, //       19M   19
    {"17m", SW_BAND_TYPE, AM_DEMOD_TYPE, 17500, 18000, 17600, 5, false, 0, 0, 0, 0}, //       17M   20
    {"16m", SW_BAND_TYPE, USB_DEMOD_TYPE, 18000, 18500, 18100, 1, true, 0, 0, 0, 0}, // Ham   16M   21
    {"15m", SW_BAND_TYPE, AM_DEMOD_TYPE, 18500, 21000, 18950, 5, false, 0, 0, 0, 0}, //       15M   22
    {"14m", SW_BAND_TYPE, USB_DEMOD_TYPE, 21000, 21500, 21074, 1, true, 0, 0, 0, 0}, // Ham   14M   23
    {"13m", SW_BAND_TYPE, AM_DEMOD_TYPE, 21500, 24000, 21500, 5, false, 0, 0, 0, 0}, //       13M   24
    {"12m", SW_BAND_TYPE, USB_DEMOD_TYPE, 24000, 25500, 24940, 1, true, 0, 0, 0, 0}, // Ham   12M   25
    {"11m", SW_BAND_TYPE, AM_DEMOD_TYPE, 25500, 26100, 25800, 5, false, 0, 0, 0, 0}, //       11M   26
    {"CB", SW_BAND_TYPE, AM_DEMOD_TYPE, 26100, 28000, 27200, 1, false, 0, 0, 0, 0},  // CB band     27
    {"10m", SW_BAND_TYPE, USB_DEMOD_TYPE, 28000, 30000, 28500, 1, true, 0, 0, 0, 0}, // Ham   10M   28
    {"SW", SW_BAND_TYPE, AM_DEMOD_TYPE, 100, 30000, 15500, 5, false, 0, 0, 0, 0}     // Whole SW    29
};

// BandMode description
const char *Band::bandModeDesc[5] = {"FM", "LSB", "USB", "AM", "CW"};

// Sávszélesség struktúrák tömbjei - ez indexre állítódik az si4735-ben!
const BandWidth Band::bandWidthFM[] = {{"AUTO", 0}, {"110", 1}, {"84", 2}, {"60", 3}, {"40", 4}};
const BandWidth Band::bandWidthAM[] = {{"1.0", 4}, {"1.8", 5}, {"2.0", 3}, {"2.5", 6}, {"3.0", 2}, {"4.0", 1}, {"6.0", 0}};
const BandWidth Band::bandWidthSSB[] = {{"0.5", 4}, {"1.0", 5}, {"1.2", 0}, {"2.2", 1}, {"3.0", 2}, {"4.0", 3}};

// AM Lépésköz
const FrequencyStep Band::stepSizeAM[] = {
    {"1kHz", 1},  // "1kHz" -> 1
    {"5kHz", 5},  // "5kHz" -> 5
    {"9kHz", 9},  // "9kHz" -> 9
    {"10kHz", 10} // "10kHz" -> 10
};
// FM Lépésköz
const FrequencyStep Band::stepSizeFM[] = {
    {"50kHz", 5},   // "50kHz" -> 5
    {"100kHz", 10}, // "100kHz" -> 10
    {"1MHz", 100}   // "1MHz" -> 100
};

// BFO Lépésköz
const FrequencyStep Band::stepSizeBFO[] = {
    {"1Hz", 1},
    {"5Hz", 5},
    {"10Hz", 10},
    {"25Hz", 25},
};

/**
 * Konstruktor
 */
Band::Band() : bandStore(nullptr) {}

/**
 * @brief Band tábla dinamikus adatainak egyszeri inicializálása
 * @details Ezt a metódust csak egyszer kell meghívni az alkalmazás indításakor!
 * @param forceReinit Ha true, akkor újrainicializálja a dinamikus adatokat, függetlenül a jelenlegi állapotuktól
 */
void Band::initializeBandTableData(bool forceReinit) {

    DEBUG("Band::initializeBandTableData() called. forceReinit: %s\n", forceReinit ? "true" : "false"); // Ha forceReinit true, akkor betöltjük a mentett adatokat
    if (forceReinit && bandStore != nullptr) {

        // Ellenőrizzük, hogy a bandTable tömb mérete megegyezik-e a BANDTABLE_SIZE konstanssal
        size_t actualBandTableSize = ARRAY_ITEM_COUNT(bandTable);
        if (actualBandTableSize != BANDTABLE_SIZE) {
            DEBUG("ERROR: A bandTable size eltér! Tényleges: %d, előre definiált: %d\n", actualBandTableSize, BANDTABLE_SIZE);
            DEBUG("Javítsd ki a defines.h BANDTABLE_SIZE értékét %d-re!\n", actualBandTableSize);
            // Itt esetleg lehetne hibakezelés, de most csak figyelmeztetünk
        }

        DEBUG("Band::initializeBandTableData() -> Loading band data from store...\n");
        loadBandData();
    }

    // A BandTable dinamikus adatainak inicializálása - változó adatok beállítása, ha még nincsenek inicializálva
    for (uint8_t i = 0; i < BANDTABLE_SIZE; i++) {

        // Ha még nem volt az EEPROM mentésből visszaállítás, akkor most bemásoljuk a default értékeket a változó értékekbe
        if (bandTable[i].currFreq == 0 || forceReinit) {
            // Ha a mentett érték 0, akkor a default értéket használjuk
            if (bandTable[i].currFreq == 0) {
                bandTable[i].currFreq = bandTable[i].defFreq; // Frekvencia
            }
            if (bandTable[i].currStep == 0) {
                bandTable[i].currStep = bandTable[i].defStep; // Lépés
            }
            if (bandTable[i].currDemod == 0) {
                bandTable[i].currDemod = bandTable[i].prefDemod; // Moduláció
            }

            // Antenna tunning capacitor - csak ha még 0
            if (bandTable[i].antCap == 0) {
                if (bandTable[i].bandType != FM_BAND_TYPE && bandTable[i].bandType != MW_BAND_TYPE && bandTable[i].bandType != LW_BAND_TYPE) {
                    bandTable[i].antCap = 1; // SW esetén antenna tunning capacitor szükséges
                } else {
                    bandTable[i].antCap = 0; // FM/MW/LW esetén nem (de ez már 0, szóval...)
                }
            }
        }
    }
}

/**
 * A Band egy rekordjának elkérése az index alapján
 *
 * @param bandIdx A keresett sáv indexe
 * @return A BandTable rekord referenciája, vagy egy üres rekord, ha nem található
 */
BandTable &Band::getBandByIdx(uint8_t bandIdx) {

    static BandTable emptyBand = {"", 0, 0, 0, 0, 0, 0, false, 0, 0, 0, 0}; // Üres rekord
    if (bandIdx >= BANDTABLE_SIZE) {
        return emptyBand; // Érvénytelen index esetén üres rekordot adunk vissza
    }

    return bandTable[bandIdx]; // Egyébként visszaadjuk a megfelelő rekordot
}

/**
 * A Band indexének elkérése a bandName alapján
 *
 * @param bandName A keresett sáv neve
 * @return A BandTable rekord indexe, vagy -1, ha nem található
 */
int8_t Band::getBandIdxByBandName(const char *bandName) {

    for (size_t i = 0; i < BANDTABLE_SIZE; i++) {
        if (strcmp(bandName, bandTable[i].bandName) == 0) {
            return i; // Megtaláltuk az indexet
        }
    }
    return -1; // Ha nem található
}

/**
 * Band tábla méretének lekérdezése
 */
uint8_t Band::getBandTableSize() { return BANDTABLE_SIZE; }

/**
 * Szűrt band nevek számának lekérdezése
 *
 * @param isHamFilter HAM szűrő
 */
uint8_t Band::getFilteredBandCount(bool isHamFilter) {
    uint8_t count = 0;
    for (size_t i = 0; i < BANDTABLE_SIZE; i++) {
        if (bandTable[i].isHam == isHamFilter) {
            count++;
        }
    }
    return count;
}

/**
 * Sávok neveinek betöltése a hívó által megadott tömbbe
 *
 * @param names A hívó által allokált tömb, amelybe a neveket betöltjük (legalább getFilteredBandCount() méretű kell legyen)
 * @param count Talált elemek száma (kimeneti paraméter)
 * @param isHamFilter HAM szűrő
 */
void Band::getBandNames(const char **names, uint8_t &count, bool isHamFilter) {

    count = 0;                                           // Kezdőérték
    uint8_t maxSize = getFilteredBandCount(isHamFilter); // Maximális méret kiszámítása
    for (size_t i = 0; i < BANDTABLE_SIZE && count < maxSize; i++) {
        if (bandTable[i].isHam == isHamFilter) {    // HAM sáv szűrés
            names[count++] = bandTable[i].bandName; // Közvetlen pointer hozzáadás
        }
    }
}

/**
 * @brief Band adatok mentése a BandStore-ba
 */
void Band::saveBandData() {
    if (bandStore != nullptr) {
        bandStore->saveFromBandTable(bandTable);
    }
}

/**
 * @brief Band adatok betöltése a BandStore-ból
 */
void Band::loadBandData() {
    if (bandStore != nullptr) {
        DEBUG("Band::loadBandData() -> Loading band data from store...\n");
        bandStore->loadToBandTable(bandTable);
    }
}
