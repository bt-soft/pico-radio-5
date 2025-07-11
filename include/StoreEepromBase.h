#ifndef STORE_EEPROM_BASE_H
#define STORE_EEPROM_BASE_H

#include <EEPROM.h>

#include "AudioProcessor.h"
#include "EepromSafeWrite.h"
#include "defines.h"
#include "utils.h"

#ifndef EEPROM_SIZE
#define EEPROM_SIZE 2048 // Alapértelmezett 2K méret (512-4096 között módosítható)
#endif

/**
 * @brief Generikus EEPROM kezelő osztály struktúrák tárolásához
 *
 * Automatikus CRC16 ellenőrzéssel rendelkezik az adatok integritásának
 * biztosítására.
 *
 * @tparam T A tárolandó struktúra típusa
 */
template <typename T> class StoreEepromBase {
  public:
    /**
     * @brief EEPROM inicializálása
     *
     * Ezt egyszer kell meghívni a setup() függvényben.
     */
    static void init() {
        EEPROM.begin(EEPROM_SIZE);
        DEBUG("EEPROM inicializálva, méret: %d bájt\n", EEPROM_SIZE);
    }

    /**
     * @brief Adatok betöltése az EEPROM-ból
     *
     * Ha az adatok érvénytelenek, visszaállítja az alapértelmezett értékeket.
     *
     * @param data Cél struktúra referencia
     * @param address EEPROM kezdőcím (alapértelmezett: 0)
     * @param className Osztálynév a debug üzenetekhez
     * @return CRC16 ellenőrző összeg
     */
    static uint16_t load(T &data, uint16_t address = 0, const char *className = "Ismeretlen") {
        bool valid = false;
        uint16_t crc = getIfValid(data, valid, address, className);

        if (!valid) {
            DEBUG("[%s] EEPROM tartalom érvénytelen a %d címen, alapértékek mentése!\n", className, address);
            return save(data, address, className);
        }

        DEBUG("[%s] EEPROM betöltés sikeres a %d címről\n", className, address);
        return crc;
    }

    /**
     * @brief Adatok mentése az EEPROM-ba
     *
     * @param data Mentendő struktúra referencia
     * @param address EEPROM kezdőcím (alapértelmezett: 0)
     * @param className Osztálynév a debug üzenetekhez
     * @return CRC16 ellenőrző összeg (0 ha sikertelen)
     */
    static uint16_t save(const T &data, uint16_t address = 0, const char *className = "Ismeretlen") {
        // RAII-stílusú Core1 audio védelem
        EepromSafeWrite::Guard guard;

        uint16_t crc = Utils::calcCRC16(reinterpret_cast<const uint8_t *>(&data), sizeof(T));

        DEBUG("[%s] Adatok mentése EEPROM %d címre (%d bájt)...", className, address, sizeof(T));

        EEPROM.put(address, data);
        EEPROM.put(address + sizeof(T), crc);

        bool commitSuccess = EEPROM.commit();

        if (commitSuccess) {
            DEBUG("Sikeres (CRC: %d)\n", crc);
            return crc;
        } else {
            DEBUG("SIKERTELEN!\n");
            return 0;
        }
    }

    /**
     * @brief Adatok ellenőrzése és betöltése ha érvényesek
     *
     * @param data Cél struktúra referencia
     * @param valid Kimeneti paraméter: true ha az adatok érvényesek
     * @param address EEPROM kezdőcím
     * @param className Osztálynév a debug üzenetekhez
     * @return CRC16 ellenőrző összeg
     */
    static uint16_t getIfValid(T &data, bool &valid, uint16_t address = 0, const char *className = "Ismeretlen") {
        T tempData;
        uint16_t storedCrc;

        // Adatok és CRC beolvasása
        EEPROM.get(address, tempData);
        EEPROM.get(address + sizeof(T), storedCrc);

        // CRC ellenőrzés
        uint16_t calculatedCrc = Utils::calcCRC16(reinterpret_cast<const uint8_t *>(&tempData), sizeof(T));
        valid = (storedCrc == calculatedCrc);

        DEBUG("[%s] EEPROM ellenőrzés %d címen. Tárolt CRC: %d, Számított CRC: %d -> %s\n", className, address, storedCrc, calculatedCrc, valid ? "Érvényes" : "ÉRVÉNYTELEN");

        if (valid) {
            data = tempData;
            return storedCrc;
        }

        return 0; // Érvénytelen CRC esetén
    }

    /**
     * @brief Szükséges EEPROM méret számítása a típushoz
     * @return Szükséges bájtok száma (adat + CRC)
     */
    static constexpr size_t getRequiredSize() { return sizeof(T) + sizeof(uint16_t); }
};

#endif // STORE_EEPROM_BASE_H