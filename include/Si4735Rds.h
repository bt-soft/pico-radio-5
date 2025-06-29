#ifndef __SI4735_RDS_H
#define __SI4735_RDS_H

#include "Band.h"
#include "Si4735Band.h"
#include "utils.h"

/**
 * @brief Si4735Rds osztály - RDS funkcionalitás kezelése
 * @details Ez az osztály tartalmazza az összes RDS-hez kapcsolódó funkcionalitást
 */
class Si4735Rds : public Si4735Band {

  public:
    /**
     * @brief Si4735Rds osztály konstruktora
     */
    Si4735Rds() : Si4735Band() { clearRdsCache(); }

    // ===================================================================
    // RDS Support - RDS funkcionalitás
    // ===================================================================

    // /**
    //  * @brief Lekérdezi az aktuális RDS Program Service (PS) nevet.
    //  * @note Csak a MemmoryDisplay.cpp fájlban használjuk.
    //  * @return String Az állomásnév, vagy üres String, ha nem elérhető.
    //  */
    // String getCurrentRdsProgramService();

    /**
     * @brief Lekérdezi az RDS állomásnevet (Program Service)
     * @return String Az RDS állomásnév, vagy üres string ha nem elérhető
     */
    String getRdsStationName();

    /**
     * @brief Lekérdezi az RDS program típus kódot (PTY)
     * @return uint8_t Az RDS program típus kódja (0-31), vagy 255 ha nincs RDS
     */
    uint8_t getRdsProgramTypeCode();

    /**
     * @brief Lekérdezi az RDS radio text üzenetet
     * @return String Az RDS radio text, vagy üres string ha nem elérhető
     */
    String getRdsRadioText();

    /**
     * @brief Lekérdezi az RDS dátum és idő információt
     * @param year Referencia a év tárolásához
     * @param month Referencia a hónap tárolásához
     * @param day Referencia a nap tárolásához
     * @param hour Referencia az óra tárolásához
     * @param minute Referencia a perc tárolásához
     * @return true ha sikerült lekérdezni a dátum/idő adatokat
     */
    bool getRdsDateTime(uint16_t &year, uint16_t &month, uint16_t &day, uint16_t &hour, uint16_t &minute);

    /**
     * @brief Ellenőrzi, hogy elérhető-e RDS adat
     * @return true ha van érvényes RDS vétel
     */
    bool isRdsAvailable();

    // ===================================================================
    // Adaptív cache és időzítési funkcionalitás
    // ===================================================================

    /**
     * @brief RDS adatok frissítése adaptív időzítéssel és cache-eléssel
     * @return true ha változtak az adatok
     */
    bool updateRdsDataWithCache();

    /**
     * @brief Cache-elt RDS állomásnév lekérdezése
     * @return String A cache-elt állomásnév
     */
    String getCachedStationName() const { return cachedStationName; }

    /**
     * @brief Cache-elt RDS program típus lekérdezése
     * @return String A cache-elt program típus
     */
    String getCachedProgramType() const { return cachedProgramType; }

    /**
     * @brief Cache-elt RDS radio text lekérdezése
     * @return String A cache-elt radio text
     */
    String getCachedRadioText() const { return cachedRadioText; }

    /**
     * @brief Cache-elt RDS dátum lekérdezése
     * @return String A cache-elt dátum
     */
    String getCachedDate() const { return cachedDate; }

    /**
     * @brief Cache-elt RDS idő lekérdezése
     * @return String A cache-elt idő
     */
    String getCachedTime() const { return cachedTime; }

    /**
     * @brief Cache-elt RDS dátum/idő lekérdezése (kompatibilitás)
     * @return String A cache-elt dátum/idő
     */
    String getCachedDateTime() const {
        if (!cachedDate.isEmpty() && !cachedTime.isEmpty()) {
            return cachedDate + " " + cachedTime;
        }
        return cachedDate.isEmpty() ? cachedTime : cachedDate;
    }

    /**
     * @brief Cache törlése (pl. állomásváltáskor)
     */
    void clearRdsCache();

    /**
     * @brief PTY kód szöveges leírássá alakítása
     * @param ptyCode A PTY kód (0-31)
     * @return String A PTY szöveges leírása
     */
    String convertPtyCodeToString(uint8_t ptyCode);

  private: // RDS cache változók
    String cachedStationName;
    String cachedProgramType;
    String cachedRadioText;
    String cachedDate;
    String cachedTime;

    // Időzítés változók
    uint32_t lastRdsUpdate = 0;
    uint32_t lastValidRdsData = 0;

    // Adaptív frissítési intervallumok (milliszekundum)
    static const uint32_t RDS_UPDATE_INTERVAL_FAST = 1000; // 1 másodperc új állomás esetén
    static const uint32_t RDS_UPDATE_INTERVAL_SLOW = 3000; // 3 másodperc stabil állomás esetén

    // Timeout értékek (milliszekundum)
    static const uint32_t RDS_DATA_TIMEOUT = 120000; // 120 másodperc
};

#endif // __SI4735_RDS_H
