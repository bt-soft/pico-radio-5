#include "Si4735Manager.h"

/**
 * @brief Konstruktor, amely inicializálja a Si4735 eszközt.
 * @param config A konfigurációs objektum, amely tartalmazza a beállításokat.
 * @param band A Band objektum, amely kezeli a rádió sávokat.
 */
Si4735Manager::Si4735Manager() : Si4735Rds() {
    setAudioMuteMcuPin(PIN_AUDIO_MUTE); // Audio Mute pin
    // Audio unmute
    si4735.setAudioMute(false);
}

/**
 * @brief Inicializáljuk az osztályt, beállítjuk a rádió sávot és hangerőt.
 */
void Si4735Manager::init(bool systemStart) {

    DEBUG("Si4735Manager::init(%s) -> Start\n", systemStart ? "true" : "false");

    // A Band  visszaállítása a konfigból
    bandInit(systemStart);

    // A sávra preferált demodulációs mód betöltése
    bandSet(systemStart);

    // Hangerő beállítása
    si4735.setVolume(config.data.currVolume);

    // Rögtön be is állítjuk az AGC-t
    checkAGC();
}

/**
 * Loop függvény a squelchez és a hardver némításhoz.
 * Ez a függvény folyamatosan figyeli a squelch állapotát és kezeli a hardver némítást.
 */
void Si4735Manager::loop() {

    // Squelch kezelése
    manageSquelch();

    // Hardver némítás kezelése
    manageHardwareAudioMute();

    // Signal quality cache frissítése, ha szükséges
    updateSignalCacheIfNeeded();
}