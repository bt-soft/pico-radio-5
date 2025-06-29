#ifndef __Si4735_MANAGER_H
#define __Si4735_MANAGER_H

#include "Config.h"
#include "Si4735Rds.h"

class Si4735Manager : public Si4735Rds {

  public:
    /**
     * @brief Konstruktor, amely inicializálja a Si4735 eszközt.
     * @param band A Band objektum, amely kezeli a rádió sávokat.
     */
    Si4735Manager();

    /**
     * @brief Inicializáljuk az osztályt, beállítjuk a rádió sávot és hangerőt.
     * @param systemStart true rendszer startup: a bandSet useDefaults értékét állítja
     */
    void init(bool systemStart = false);

    /**
     * Loop függvény a squelchez és a hardver némításhoz.
     * Ez a függvény folyamatosan figyeli a squelch állapotát és kezeli a hardver némítást.
     */
    void loop();
};

#endif // __Si4735_MANAGER_H