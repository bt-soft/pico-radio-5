#ifndef __ROTARYENCODER_H
#define __ROTARYENCODER_H
/**
 * Timer-based rotary encoder
 * Rotary Encoder Driver with Acceleration
 * Supports Click, DoubleClick, Long Click
 *
 * inspired:  http://www.mikrocontroller.net/articles/Drehgeber
 *
 */

#include <Arduino.h>

#define ROTARY_ENCODER_STEPS_PER_NOTCH 2  // Impulzusok száma egy lépéshez, Rotary függő ezt ki kell kísérletezni!!

#define ROTARY_ENCODER_RECOMMENDED_SERVICE_INTERVAL_MSEC 1  // A javasolt service() hívás periódus idő = 1msec

// ----------------------------------------------------------------------------

#define ENC_NORMAL (1 << 1)  // use Peter Danneger's decoder
#define ENC_FLAKY (1 << 2)   // use Table-based decoder

// ----------------------------------------------------------------------------

#ifndef ENC_DECODER
#define ENC_DECODER ENC_NORMAL
#endif

#if ENC_DECODER == ENC_FLAKY
#ifndef ENC_HALFSTEP
#define ENC_HALFSTEP 1  // use table for half step per default
#endif
#endif

class RotaryEncoder {
   public:
    // Forgás iránya
    enum Direction {
        None,  // nincs irány
        Up,    // jobbra/fel
        Down   // balra/le
    };

    // Gomb állapota
    enum ButtonState {
        Open,          // nyitva
        Pressed,       // lenyomva
        Held,          // nyomva tartava
        Released,      // elengedve
        Clicked,       // klikk
        DoubleClicked  // duplaklikk
    };

    // Encoder állapotát tároló struktúra
    struct EncoderState {
        Direction direction;
        ButtonState buttonState;
        int16_t value;  // aktuális érték
    };

   private:
    const uint8_t pinA;
    const uint8_t pinB;
    const uint8_t pinBTN;
    const bool pinsActive;
    volatile int16_t delta;
    volatile int16_t last;
    uint8_t steps;
    volatile uint16_t acceleration;
    bool accelerationEnabled;
#if ENC_DECODER != ENC_NORMAL
    static const int8_t table[16];
#endif
    volatile ButtonState buttonState;
    bool doubleClickEnabled;
    uint16_t keyDownTicks = 0;
    uint8_t doubleClickTicks = 0;
    unsigned long lastButtonCheck = 0;

    /**
     * Tekergetés állapot lekérdezése
     */
    int16_t getValue(void);
    /**
     * Gomb állapot lekérdezése
     */
    ButtonState getButton(void);

   public:
    /**
     * Konstruktor
     */
    RotaryEncoder(uint8_t A, uint8_t B, uint8_t BTN = -1, uint8_t stepsPerNotch = 1, bool pinsActive = LOW);

    /**
     * Ezt a függvényt hívja meg a megszakítás vagy az időzítő rutin
     */
    void service();

    /**
     * Gyorsulás engedélyezése/letiltása
     */
    void setAccelerationEnabled(const bool &enabled) {
        accelerationEnabled = enabled;
        if (accelerationEnabled == false) {
            acceleration = 0;
        }
    }

    const bool getAccelerationEnabled() { return accelerationEnabled; }

    /**
     * Dupla kattintás engedélyezése/letiltása
     */
    void setDoubleClickEnabled(const bool &enabled) { doubleClickEnabled = enabled; }

    const bool getDoubleClickEnabled() { return doubleClickEnabled; }

    /**
     * Az enkóder jelenlegi állapotának lekérdezése
     */
    EncoderState read();
};

#endif  // ROTARYENCODER_H
