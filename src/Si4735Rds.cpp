#include "Si4735Rds.h"
#include "Config.h"
#include "StationData.h"

#define VALID_STATION_NAME_MIN_LENGHT 3 // Minimális hossz az érvényes állomásnévhez

// /**
//  * @brief Lekérdezi az aktuális RDS Program Service (PS) nevet.
//  * @note Csak a MemmoryDisplay.cpp fájlban használjuk.
//  * @return String Az állomásnév, vagy üres String, ha nem elérhető.
//  */
// String Si4735Rds::getCurrentRdsProgramService() {

//     // Csak FM módban van értelme RDS-t keresni
//     if (!isCurrentBandFM()) {
//         return "";
//     }

//     si4735.getRdsStatus();                                // Frissítsük az RDS állapotát
//     if (si4735.getRdsReceived() && si4735.getRdsSync()) { // Csak ha van érvényes RDS jel
//         char *rdsPsName = si4735.getRdsText0A();          // Program Service Name (állomásnév)
//         if (rdsPsName != nullptr && strlen(rdsPsName) > 0) {
//             char tempRdsName[STATION_NAME_BUFFER_SIZE]; // STATION_NAME_BUFFER_SIZE a StationData.h-ból
//             strncpy(tempRdsName, rdsPsName, STATION_NAME_BUFFER_SIZE - 1);
//             tempRdsName[STATION_NAME_BUFFER_SIZE - 1] = '\0'; // Biztos null-terminálás
//             Utils::trimSpaces(tempRdsName);                   // Esetleges felesleges szóközök eltávolítása mindkét oldalról
//             return String(tempRdsName);
//         }
//     }

//         return ""; // Nincs érvényes RDS PS név
// }

/**
 * @brief Lekérdezi az RDS állomásnevet (Program Service)
 * @return String Az RDS állomásnév, vagy üres string ha nem elérhető
 */
String Si4735Rds::getRdsStationName() {

    // Ellenőrizzük, hogy FM módban vagyunk-e
    if (!isCurrentBandFM()) {
        return "";
    }

    // RDS státusz frissítése
    si4735.getRdsStatus();

    char *rdsStationName = si4735.getRdsText0A();
    if (rdsStationName != nullptr && strlen(rdsStationName) > 0) {
        char tempName[32];
        strncpy(tempName, rdsStationName, sizeof(tempName) - 1);
        tempName[sizeof(tempName) - 1] = '\0';
        Utils::trimSpaces(tempName);
        String result = String(tempName);
        return result;
    }

    return "";
}

/**
 * @brief Lekérdezi az RDS program típus kódot (PTY)
 * @return uint8_t Az RDS program típus kódja (0-31), vagy 255 ha nincs RDS
 */
uint8_t Si4735Rds::getRdsProgramTypeCode() {

    // Ellenőrizzük, hogy FM módban vagyunk-e
    if (!isCurrentBandFM()) {
        return 255; // Nincs RDS
    }

    // RDS státusz frissítése
    si4735.getRdsStatus();

    return si4735.getRdsProgramType();
}

/**
 * @brief Lekérdezi az RDS radio text üzenetet
 * @return String Az RDS radio text, vagy üres string ha nem elérhető
 */
String Si4735Rds::getRdsRadioText() {

    // Ellenőrizzük, hogy FM módban vagyunk-e
    if (!isCurrentBandFM()) {
        return "";
    }

    // RDS státusz frissítése
    si4735.getRdsStatus();

    char *rdsText = si4735.getRdsText2A();
    if (rdsText != nullptr && strlen(rdsText) > 0) {
        char tempText[128];
        strncpy(tempText, rdsText, sizeof(tempText) - 1);
        tempText[sizeof(tempText) - 1] = '\0';
        Utils::trimSpaces(tempText);
        String result = String(tempText);
        return result;
    }

    return "";
}

/**
 * @brief Lekérdezi az RDS dátum és idő információt
 * @param year Referencia a év tárolásához
 * @param month Referencia a hónap tárolásához
 * @param day Referencia a nap tárolásához
 * @param hour Referencia az óra tárolásához
 * @param minute Referencia a perc tárolásához
 * @return true ha sikerült lekérdezni a dátum/idő adatokat
 */
bool Si4735Rds::getRdsDateTime(uint16_t &year, uint16_t &month, uint16_t &day, uint16_t &hour, uint16_t &minute) {

    // Ellenőrizzük, hogy FM módban vagyunk-e
    if (!isCurrentBandFM()) {
        return false;
    }

    // RDS státusz frissítése
    si4735.getRdsStatus();

    return si4735.getRdsDateTime(&year, &month, &day, &hour, &minute);
}

/**
 * @brief Ellenőrzi, hogy elérhető-e RDS adat
 * @return true ha van érvényes RDS vétel
 */
bool Si4735Rds::isRdsAvailable() {

    // Ellenőrizzük, hogy FM módban vagyunk-e
    if (!isCurrentBandFM()) {
        return false;
    }

    // RDS státusz lekérdezése
    si4735.getRdsStatus();

    if (!si4735.getRdsReceived() or !si4735.getRdsSync() or !si4735.getRdsSyncFound()) {
        return false;
    }
    return true;
}

// ===================================================================
// Adaptív cache és időzítési funkcionalitás
// ===================================================================

/**
 * @brief RDS adatok frissítése adaptív időzítéssel és cache-eléssel
 * @return true ha változtak az adatok
 */
bool Si4735Rds::updateRdsDataWithCache() {
    uint32_t currentTime = millis();

    // Adaptív frissítési időköz:
    // - Ha nincs RDS adat -> 1 másodperc (gyors keresés)
    // - Ha van stabil RDS adat -> 3 másodperc (takarékos)
    uint32_t adaptiveInterval = RDS_UPDATE_INTERVAL_SLOW;
    if (cachedStationName.isEmpty() || cachedStationName.length() < VALID_STATION_NAME_MIN_LENGHT) {
        adaptiveInterval = RDS_UPDATE_INTERVAL_FAST;
    }

    // Időzített frissítés
    if (currentTime - lastRdsUpdate < adaptiveInterval || !isRdsAvailable()) {
        return false; // Túl korai a frissítés vagy nincs RDS adat
    }
    lastRdsUpdate = currentTime;

    bool dataChanged = false;
    bool hasValidData = false;

    // --- Állomásnév frissítése -------------------------------------------------------------
    String newStationName = getRdsStationName();
    // Csak akkor frissíti a cache-t, ha tényleg változott az adat
    if (!newStationName.isEmpty() && newStationName.length() >= VALID_STATION_NAME_MIN_LENGHT && newStationName != cachedStationName) {
        cachedStationName = newStationName;
        dataChanged = true;
        hasValidData = true;
    }
    // Akkor is jelzünk az érvényes adatot, ha nem változott, de van adat
    if (!newStationName.isEmpty()) {
        hasValidData = true;
    }

    // --- Program típus frissítése - PTY kód alapján -----------------------------------------
    uint8_t newPtyCode = getRdsProgramTypeCode();
    if (newPtyCode != 255) { // 255 = nincs RDS
        String newProgramType = convertPtyCodeToString(newPtyCode);
        if (!newProgramType.isEmpty() && newProgramType != cachedProgramType) {
            cachedProgramType = newProgramType;
            dataChanged = true;
            hasValidData = true;
        }
        if (!newProgramType.isEmpty()) {
            hasValidData = true;
        }
    }

    // --- Radio text frissítése -------------------------------------------------------------
    String newRadioText = getRdsRadioText();
    if (!newRadioText.isEmpty() && newRadioText != cachedRadioText) {
        cachedRadioText = newRadioText;
        dataChanged = true;
        hasValidData = true;
    }
    if (!newRadioText.isEmpty()) {
        hasValidData = true;
    }

    // -- Dátum/idő frissítése -------------------------------------------------------------
    uint16_t year, month, day, hour, minute;
    if (getRdsDateTime(year, month, day, hour, minute)) {
        // Dátum formázása: "2025.06.14"
        String newDate = String(year) + "." + (month < 10 ? "0" : "") + String(month) + "." + (day < 10 ? "0" : "") + String(day);

        // Dátum ellenőrzése és frissítése
        if (newDate != cachedDate) {
            cachedDate = newDate;
            dataChanged = true;
            hasValidData = true;
        }

        // Idő formázása: "15:30"
        String newTime = (hour < 10 ? "0" : "") + String(hour) + ":" + (minute < 10 ? "0" : "") + String(minute);

        // Idő ellenőrzése és frissítése
        if (newTime != cachedTime) {
            cachedTime = newTime;
            dataChanged = true;
            hasValidData = true;
        }
    }

    // Ha volt valid adat, frissítsük az időzítőt
    if (hasValidData) {
        lastValidRdsData = currentTime;

        if (dataChanged) {
            DEBUG("--- RDS data dataChanged --- \n");
            DEBUG("cachedStationName: '%s'\n", cachedStationName.c_str());
            DEBUG("cachedProgramType: '%s'\n", cachedProgramType.c_str());
            DEBUG("cachedRadioText: '%s'\n", cachedRadioText.c_str());
            DEBUG("cachedDate: '%s'\n", cachedDate.c_str());
            DEBUG("cachedTime: '%s'\n", cachedTime.c_str());
            DEBUG("---------------------------- \n");
        }
    }

    // Timeout ellenőrzés - különböző időzítés a különböző RDS adatokhoz
    if (currentTime - lastValidRdsData > RDS_DATA_TIMEOUT) { // Hosszú timeout után ha nincs érvényes állomásnév, akkor töröljük a cache-t
        if (!cachedStationName.isEmpty()) {
            cachedStationName = "";
            cachedProgramType = "";
            cachedRadioText = "";
            cachedDate = "";
            cachedTime = "";
            dataChanged = true;
        }
    }

    return dataChanged;
}

/**
 * @brief Cache törlése (pl. állomásváltáskor)
 */
void Si4735Rds::clearRdsCache() {
    cachedStationName = "";
    cachedProgramType = "";
    cachedRadioText = "";
    cachedDate = "";
    cachedTime = "";
    lastRdsUpdate = 0; // Azonnal frissítsen
    lastValidRdsData = 0;
}

/**
 * @brief PTY kód szöveges leírássá alakítása
 * @param ptyCode A PTY kód (0-31)
 * @return String A PTY szöveges leírása
 */
String Si4735Rds::convertPtyCodeToString(uint8_t ptyCode) {
    // PTY kódok RDS szabvány szerint (0-31)
    static const char *ptyTable[] = {
        "No programme",          // 0
        "News",                  // 1
        "Current Affairs",       // 2
        "Information",           // 3
        "Sport",                 // 4
        "Education",             // 5
        "Drama",                 // 6
        "Culture",               // 7
        "Science",               // 8
        "Varied",                // 9
        "Pop Music",             // 10
        "Rock Music",            // 11
        "Easy Listening",        // 12
        "Light Classical",       // 13
        "Serious Classical",     // 14
        "Other Music",           // 15
        "Weather",               // 16
        "Finance",               // 17
        "Children's programmes", // 18
        "Social Affairs",        // 19
        "Religion",              // 20
        "Phone In",              // 21
        "Travel",                // 22
        "Leisure",               // 23
        "Jazz Music",            // 24
        "Country Music",         // 25
        "National Music",        // 26
        "Oldies Music",          // 27
        "Folk Music",            // 28
        "Documentary",           // 29
        "Alarm Test",            // 30
        "Alarm"                  // 31
    };

    if (ptyCode <= 31) {
        return String(ptyTable[ptyCode]);
    }
    return "Unknown";
}
