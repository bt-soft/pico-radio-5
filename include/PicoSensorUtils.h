#pragma once

#include <Arduino.h>

#include "defines.h" // PIN_VBUS

namespace PicoSensorUtils {

// Cache konstans
#define PICO_SENSORS_CACHE_TIMEOUT_MS (5 * 1000) // 5 másodperc a cache idő

// Cache struktúra
struct SensorCache {
    float vBusExtValue;            // VBUS KÜLSŐ feszültség utolsó mért értéke (Volt)
    unsigned long vBusExtLastRead; // VBUS utolsó mérésének időpontja (ms)
    bool vBusExtValid;             // VBUS cache érvényessége

    float vBusIntValue;            // VBUS BELSŐ feszültség utolsó mért értéke (Volt)
    unsigned long vBusIntLastRead; // VBUS BELSŐ utolsó mérésének időpontja (ms)
    bool vBusIntValid;             // VBUS BELSŐ cache érvényessége

    float temperatureValue;            // Hőmérséklet utolsó mért értéke (Celsius)
    unsigned long temperatureLastRead; // Hőmérséklet utolsó mérésének időpontja (ms)
    bool temperatureValid;             // Hőmérséklet cache érvényessége

    // Konstruktor: minden értéket alaphelyzetbe állít
    SensorCache()
        : temperatureValue(0.0f), temperatureLastRead(0), temperatureValid(false), //
          vBusExtValue(0.0f), vBusExtLastRead(0), vBusExtValid(false),             //
          vBusIntValue(0.0f), vBusIntLastRead(0), vBusIntValid(false) {}

    SensorCache(const SensorCache &) = default;
};

// Globális cache példány
static SensorCache sensorCache;

/**
 * AD inicializálása
 */
void init();

/**
 * ADC olvasás és VBUS feszültség kiszámítása KÜLSŐ osztóval
 * @return A VBUS mért feszültsége Voltban.
 */
float readVBusExternal();

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
 * @param vBusExtValid kimeneti paraméter - VBUS KÜLSŐ cache érvényessége
 * @param vBusIntValid kimeneti paraméter - VBUS BELSŐ cache érvényessége
 * @param temperatureValid kimeneti paraméter - hőmérséklet cache érvényessége
 */
void inline getCacheStatus(bool &vBusExtValid, bool &vBusIntValid, bool &temperatureValid);

}; // namespace PicoSensorUtils
