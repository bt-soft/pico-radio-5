#ifndef __SI4735_BASE_H
#define __SI4735_BASE_H

#include <Arduino.h>
#include <SI4735.h>

#include "Band.h"
#include "Config.h"
#include "defines.h"
#include "rtVars.h"

namespace Si4735Constants {
// Hangerő beállítások
static constexpr int SI4735_MIN_VOLUME = 0;
static constexpr int SI4735_MAX_VOLUME = 63;

// Antenna kapacitás beállítások
static constexpr int SI4735_MAX_ANT_CAP_FM = 191;
static constexpr int SI4735_MAX_ANT_CAP_AM = 6143;

// AGC
static constexpr int SI4735_MIN_ATTENNUATOR = 1;     // Minimum attenuator érték
static constexpr int SI4735_MAX_ATTENNUATOR_FM = 26; // FM: 0-26 közötti tartomány az LNA (Low Noise Amplifier) számára
static constexpr int SI4735_MAX_ATTENNUATOR_AM = 37; // AM/SSB: 0-37+ATTN_BACKUP közötti tartomány

}; // namespace Si4735Constants

/**
 * @brief Si4735Base osztály
 */
class Si4735Base {

  protected:
    SI4735 si4735;

  public:
    Si4735Base() {}

    /**
     * @brief  SI4735 referencia lekérése
     * @return SI4735 referencia
     */
    SI4735 &getSi4735() { return si4735; }

    /**
     * @brief  I2C bus address setup
     */
    inline int16_t getDeviceI2CAddress() { return si4735.getDeviceI2CAddress(PIN_SI4735_RESET); }

    /**
     * @brief Sets the I2C Bus Address
     * @param senPin 0 -  SI4735 device: when the pin SEN (16 on SSOP version or pin 6 on QFN version) is set to low (GND - 0V);
     *               1 -  Si4735 device: when the pin SEN (16 on SSOP version or pin 6 on QFN version) is set to high (+3.3V).
     *               If you are using an SI4732 device, reverse the above logic (1 - GND or 0 - +3.3V).
     */
    inline void setDeviceI2CAddress(uint8_t senPin) { si4735.setDeviceI2CAddress(senPin); }

    /**
     * @brief Inicializálja a Si4735 eszközt.
     * @param pin if 0 or greater, sets the MCU digital pin that controls the external circuit.
     */
    inline void setAudioMuteMcuPin(uint8_t pin) { si4735.setAudioMuteMcuPin(pin); }
};

#endif // __SI4735_BASE_H