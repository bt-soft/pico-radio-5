#include "PicoSensorUtils.h"

namespace PicoSensorUtils {

// --- Konstansok ---
#define AD_RESOLUTION 12 // 12 bites az ADC
#define V_REFERENCE 3.3f
#define CONVERSION_FACTOR (1 << AD_RESOLUTION)

// Külső feszültségosztó ellenállásai a VBUS méréshez (A0-ra kötve)
#define EXTERNAL_VBUSDIVIDER_RATIO ((VBUS_DIVIDER_R1 + VBUS_DIVIDER_R2) / VBUS_DIVIDER_R2) // Feszültségosztó aránya

/**
 * AD inicializálása
 */
void init() { analogReadResolution(AD_RESOLUTION); }

/**
 * ADC olvasás és VBUS feszültség kiszámítása KÜLSŐ osztóval
 * @return A VBUS mért feszültsége Voltban.
 */
float readVBusExternal() {
    unsigned long currentTime = millis();

    // Ellenőrizzük, hogy a cache még érvényes-e
    if (sensorCache.vBusExtValid && (currentTime - sensorCache.vBusExtLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS)) {
        return sensorCache.vBusExtValue;
    }

    // Cache lejárt vagy nem érvényes, új mérés
    float voltageOut = (analogRead(PIN_VBUS_EXTERNAL_MEASURE_INPUT) * V_REFERENCE) / CONVERSION_FACTOR;
    float vBusExtVoltage = voltageOut * EXTERNAL_VBUSDIVIDER_RATIO;

    // Cache frissítése
    sensorCache.vBusExtValue = vBusExtVoltage;
    sensorCache.vBusExtLastRead = currentTime;
    sensorCache.vBusExtValid = true;

    return vBusExtVoltage;
}

/**
 * @brief Kiolvassa a processzor hőmérsékletét
 * @details A hőmérsékletet az ADC0 bemeneten keresztül olvassa, és cache-eli az értéket.
 * @return A processzor hőmérséklete Celsius fokban
 */
float readCoreTemperature() {
    unsigned long currentTime = millis();
    if (sensorCache.temperatureValid && (currentTime - sensorCache.temperatureLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS)) {
        return sensorCache.temperatureValue;
    }
    float temperature = analogReadTemp(); // A4
    sensorCache.temperatureValue = temperature;
    sensorCache.temperatureLastRead = currentTime;
    sensorCache.temperatureValid = true;
    return temperature;
}

/**
 * @brief Cache törlése
 * @details A következő olvasásnál új mérést fog végezni
 */
void clearCache() {
    sensorCache.temperatureValid = false;
    sensorCache.vBusExtValid = false;
    sensorCache.vBusIntValid = false;
}

/**
 * Cache státusz lekérdezése
 * @param vBusExtValid kimeneti paraméter - VBUS KÜLSŐ cache érvényessége
 * @param vBusIntValid kimeneti paraméter - VBUS BELSŐ cache érvényessége
 * @param temperatureValid kimeneti paraméter - hőmérséklet cache érvényessége
 */
void getCacheStatus(bool &vBusExtValid, bool &vBusIntValid, bool &temperatureValid) {
    unsigned long currentTime = millis();
    vBusExtValid = sensorCache.vBusExtValid && (currentTime - sensorCache.vBusExtLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS);
    vBusIntValid = sensorCache.vBusIntValid && (currentTime - sensorCache.vBusIntLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS);
    temperatureValid = sensorCache.temperatureValid && (currentTime - sensorCache.temperatureLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS);
}

} // namespace PicoSensorUtils
