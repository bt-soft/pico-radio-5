#include "PicoSensorUtils.h"

namespace PicoSensorUtils {

/**
 * AD inicializálása
 */
void init() { analogReadResolution(AD_RESOLUTION); }

/**
 * A Pico VSYS (tápfeszültség) mérése az ADC3 (GPIO29) bemeneten keresztül.
 * A Pico belső feszültségosztója miatt a mért értéket 3-mal kell szorozni.
 * Az eredményt cache-eljük, hogy ne legyen felesleges mérés.
 * @return VSYS feszültség Voltban
 */
float readVSys() {
    unsigned long currentTime = millis();
    if (sensorCache.vsysValid && (currentTime - sensorCache.vsysLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS)) {
        return sensorCache.vsysValue;
    }
    float voltageOut = (analogRead(29) * V_REFERENCE) / CONVERSION_FACTOR;
    float vsysVoltage = voltageOut * 3.0f;
    sensorCache.vsysValue = vsysVoltage;
    sensorCache.vsysLastRead = currentTime;
    sensorCache.vsysValid = true;
    return vsysVoltage;
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
    float temperature = analogReadTemp();
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
    sensorCache.vsysValid = false;
}

// /**
//  * Cache státusz lekérdezése
//  * @param vbusValid kimeneti paraméter - VBUS cache érvényessége
//  * @param tempValid kimeneti paraméter - hőmérséklet cache érvényessége
//  */
// void getCacheStatus(bool &vbusValid, bool &tempValid) {
//     unsigned long currentTime = millis();
//     //vbusValid = sensorCache.vbusValid && (currentTime - sensorCache.vbusLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS);
//     tempValid = sensorCache.temperatureValid && (currentTime - sensorCache.temperatureLastRead < PICO_SENSORS_CACHE_TIMEOUT_MS);
//     // VSYS státusz külön kérhető, ha szükséges
// }

} // namespace PicoSensorUtils
