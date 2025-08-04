#ifndef __PICO_SENSOR_UTILS_H
#define __PICO_SENSOR_UTILS_H

#include <Arduino.h>

#include "defines.h" // PIN_VBUS

namespace PicoSensorUtils {

// --- Konstansok ---
#define AD_RESOLUTION 12 // 12 bites az ADC
#define V_REFERENCE 3.3f
#define CONVERSION_FACTOR (1 << AD_RESOLUTION)

// Külső feszültségosztó ellenállásai a VBUS méréshez (A0-ra kötve)
#define DIVIDER_RATIO ((VBUS_DIVIDER_R1 + VBUS_DIVIDER_R2) / VBUS_DIVIDER_R2) // Feszültségosztó aránya

// Cache konstans
#define PICO_SENSORS_CACHE_TIMEOUT_MS (5 * 1000) // 5 másodperc a cache idő

// Cache struktúra
struct SensorCache {
    // float vbusValue;                   // VBUS feszültség utolsó mért értéke (Volt)
    // unsigned long vbusLastRead;        // VBUS utolsó mérésének időpontja (ms)
    // bool vbusValid;                    // VBUS cache érvényessége
    float temperatureValue;            // Hőmérséklet utolsó mért értéke (Celsius)
    unsigned long temperatureLastRead; // Hőmérséklet utolsó mérésének időpontja (ms)
    bool temperatureValid;             // Hőmérséklet cache érvényessége

    float vsysValue;            // VSYS feszültség utolsó mért értéke (Volt)
    unsigned long vsysLastRead; // VSYS utolsó mérésének időpontja (ms)
    bool vsysValid;             // VSYS cache érvényessége

    // Konstruktor: minden értéket alaphelyzetbe állít
    SensorCache() : temperatureValue(0.0f), temperatureLastRead(0), temperatureValid(false), vsysValue(0.0f), vsysLastRead(0), vsysValid(false) {}
    SensorCache(const SensorCache &) = default;
};

// Globális cache példány
static SensorCache sensorCache;

/**
 * AD inicializálása
 */
void init();

/**
 * A Pico VSYS (tápfeszültség) mérése az ADC3 (GPIO29) bemeneten keresztül.
 * A Pico belső feszültségosztója miatt a mért értéket 3-mal kell szorozni.
 * Az eredményt cache-eljük, hogy ne legyen felesleges mérés.
 * @return VSYS feszültség Voltban
 */
float readVSys();

// /**
//  * ADC olvasás és VBUS feszültség kiszámítása külső osztóval
//  * @return A VBUS mért feszültsége Voltban.
//  */
// float readVBus() {
//     unsigned long currentTime = millis();

//     // Ellenőrizzük, hogy a cache még érvényes-e
//     if (sensorCache.vbusValid && (currentTime - sensorCache.vbusLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS)) {
//         return sensorCache.vbusValue;
//     }

//     // Cache lejárt vagy nem érvényes, új mérés
//     float voltageOut = (analogRead(PIN_VBUS_INPUT) * V_REFERENCE) / CONVERSION_FACTOR;
//     float vbusVoltage = voltageOut * DIVIDER_RATIO;

//     // Cache frissítése
//     sensorCache.vbusValue = vbusVoltage;
//     sensorCache.vbusLastRead = currentTime;
//     sensorCache.vbusValid = true;

//     return vbusVoltage;
// }

/**
 * Kiolvassa a processzor hőmérsékletét
 * @return processzor hőmérséklete Celsius fokban
 */
float readCoreTemperature();

/**
 * Cache törlése - következő olvasásnál új mérést fog végezni
 */
void clearCache();

/**
 * Cache státusz lekérdezése
 * @param vsysValid kimeneti paraméter - VSYS cache érvényessége
 * @param temperatureValid kimeneti paraméter - hőmérséklet cache érvényessége
 */
// void getCacheStatus(bool &vbusValid, bool &tempValid);

}; // namespace PicoSensorUtils

#endif // __PICO_SENSOR_UTILS_H
