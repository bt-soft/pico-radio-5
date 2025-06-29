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
    float vbusValue;
    float temperatureValue;
    unsigned long vbusLastRead;
    unsigned long temperatureLastRead;
    bool vbusValid;
    bool temperatureValid;

    SensorCache() : vbusValue(0.0f), temperatureValue(0.0f), vbusLastRead(0), temperatureLastRead(0), vbusValid(false), temperatureValid(false) {}
};

// Globális cache példány
static SensorCache sensorCache;

/**
 * AD inicializálása
 */
inline void init() { analogReadResolution(AD_RESOLUTION); }

/**
 * ADC olvasás és VBUS feszültség kiszámítása külső osztóval
 * @return A VBUS mért feszültsége Voltban.
 */
inline float readVBus() {
    unsigned long currentTime = millis();

    // Ellenőrizzük, hogy a cache még érvényes-e
    if (sensorCache.vbusValid && (currentTime - sensorCache.vbusLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS)) {
        return sensorCache.vbusValue;
    }

    // Cache lejárt vagy nem érvényes, új mérés
    float voltageOut = (analogRead(PIN_VBUS_INPUT) * V_REFERENCE) / CONVERSION_FACTOR;
    float vbusVoltage = voltageOut * DIVIDER_RATIO;

    // Cache frissítése
    sensorCache.vbusValue = vbusVoltage;
    sensorCache.vbusLastRead = currentTime;
    sensorCache.vbusValid = true;

    return vbusVoltage;
}

/**
 * Kiolvassa a processzor hőmérsékletét
 * @return processzor hőmérséklete Celsius fokban
 */
inline float readCoreTemperature() {
    unsigned long currentTime = millis();

    // Ellenőrizzük, hogy a cache még érvényes-e
    if (sensorCache.temperatureValid && (currentTime - sensorCache.temperatureLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS)) {
        return sensorCache.temperatureValue;
    }

    // Cache lejárt vagy nem érvényes, új mérés
    float temperature = analogReadTemp();

    // Cache frissítése
    sensorCache.temperatureValue = temperature;
    sensorCache.temperatureLastRead = currentTime;
    sensorCache.temperatureValid = true;

    return temperature;
}

/**
 * Cache törlése - következő olvasásnál új mérést fog végezni
 */
inline void clearCache() {
    sensorCache.vbusValid = false;
    sensorCache.temperatureValid = false;
}

/**
 * Cache státusz lekérdezése
 * @param vbusValid kimeneti paraméter - VBUS cache érvényessége
 * @param tempValid kimeneti paraméter - hőmérséklet cache érvényessége
 */
inline void getCacheStatus(bool &vbusValid, bool &tempValid) {
    unsigned long currentTime = millis();
    vbusValid = sensorCache.vbusValid && (currentTime - sensorCache.vbusLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS);
    tempValid = sensorCache.temperatureValid && (currentTime - sensorCache.temperatureLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS);
}

}; // namespace PicoSensorUtils

#endif // __PICO_SENSOR_UTILS_H
