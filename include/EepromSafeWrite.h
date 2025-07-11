#pragma once

#include "AudioCore1Manager.h"

/**
 * @brief EEPROM biztonságos írás Core1 audio szüneteltetésével
 *
 * Ez a wrapper biztosítja, hogy EEPROM írás közben a Core1 audio
 * feldolgozás szünetelve legyen.
 */
class EepromSafeWrite {
  public:
    /**
     * @brief EEPROM biztonságos írás indítása
     */
    static void begin() { AudioCore1Manager::pauseCore1Audio(); }

    /**
     * @brief EEPROM biztonságos írás befejezése
     */
    static void end() { AudioCore1Manager::resumeCore1Audio(); }

    /**
     * @brief RAII-stílusú EEPROM védelem automatikus destruktorral
     */
    class Guard {
      public:
        Guard() { begin(); }
        ~Guard() { end(); }
    };
};
