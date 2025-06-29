#include "Si4735Runtime.h"

#define SIGNAL_QUALITY_CACHE_TIMEOUT_MS (1 * 1000) // 1 másodperc cache timeout

/**
 * Manage Squelch
 */
void Si4735Runtime::manageSquelch() {

    // Ha nem katív a squelch, akkor nem csinálunk semmit
    if (config.data.currentSquelch <= 0) {
        return;
    }

    // Csak akkor fusson, ha a globális némítás ki van kapcsolva
    if (!rtv::muteStat) {
        // Realtime signal quality adatok a pontos squelch működéshez
        SignalQualityData signalData = getSignalQualityRealtime();
        uint8_t signalQuality = config.data.squelchUsesRSSI ? signalData.rssi : signalData.snr;

        if (signalQuality >= config.data.currentSquelch) {
            // Jel a küszöb felett -> Némítás kikapcsolása (ha szükséges)
            if (rtv::SCANpause == true) { // Ez a feltétel még mindig furcsa itt, de meghagyjuk
                if (isSquelchMuted) {     // Csak akkor kapcsoljuk ki, ha eddig némítva volt
                    si4735.setAudioMute(false);
                    isSquelchMuted = false; // Állapot frissítése
                }
                rtv::squelchDecay = millis(); // Időzítőt mindig reseteljük, ha jó a jel
            }
        } else {
            // Jel a küszöb alatt -> Némítás bekapcsolása késleltetés után (ha szükséges)
            if (millis() > (rtv::squelchDecay + SQUELCH_DECAY_TIME)) {
                if (!isSquelchMuted) { // Csak akkor kapcsoljuk be, ha eddig nem volt némítva
                    si4735.setAudioMute(true);
                    isSquelchMuted = true; // Állapot frissítése
                }
            }
        }
    } else {
        // Ha a globális némítás be van kapcsolva (rtv::muteStat == true),
        // akkor a squelch állapotát is némítottra állítjuk, hogy szinkronban legyen.
        // Ez fontos, hogy amikor a globális némítást kikapcsolják, a squelch helyesen tudjon működni.
        if (!isSquelchMuted) {
            // Nem kell ténylegesen mute parancsot küldeni, mert már némítva van globálisan,
            // csak a belső állapotot frissítjük.
            isSquelchMuted = true;
        }
        // A decay timert is resetelhetjük itt, hogy ne némítson azonnal, ha a global mute megszűnik
        rtv::squelchDecay = millis();
    }
}

/**
 * AGC beállítása
 */
void Si4735Runtime::checkAGC() {

    // Először lekérdezzük az SI4735 chip aktuális AGC állapotát.
    //  Ez a hívás frissíti az SI4735 objektum belső állapotát az AGC-vel kapcsolatban (pl. hogy engedélyezve van-e vagy sem).
    si4735.getAutomaticGainControl();

    // Mit szeretnénk beállítani?
    AgcGainMode desiredMode = static_cast<AgcGainMode>(config.data.agcGain);

    // Most engedélyezve van az AGC?
    bool chipAgcEnabled = si4735.isAgcEnabled();
    bool stateChanged = false; // Jelző, hogy történt-e változás, küldtünk-e AGC parancsot?

    // Ha a felhasználó kikapcsolta az AGC-t, akkor állítsuk le a chip AGC-t is
    if (desiredMode == AgcGainMode::Off) {
        // A felhasználó az AGC kikapcsolását kérte.
        if (chipAgcEnabled) {
            DEBUG("Si4735Runtime::checkAGC() -> AGC Off\n");
            si4735.setAutomaticGainControl(1, 0); // disabled -> AGCDIS = 1, AGCIDX = 0
            stateChanged = true;
        }

    } else if (desiredMode == AgcGainMode::Automatic) {
        // Ha az AGC nincs engedélyezve az AGC, de a felhasználó az AGC engedélyezését kérte
        // Ez esetben az AGC-t engedélyezzük (0), és a csillapítást nullára állítjuk (0).
        // Ez a teljesen automatikus AGC működést jelenti.
        if (!chipAgcEnabled) {
            DEBUG("Si4735Runtime::checkAGC() -> AGC Automatic\n");
            si4735.setAutomaticGainControl(0, 0); // enabled -> AGCDIS = 0, AGCIDX = 0
            stateChanged = true;
        }
    } else if (desiredMode == AgcGainMode::Manual) {

        // Csak ha nem azonos az AGC-gain index, akkor állítsuk be a chip AGC-t
        if (config.data.currentAGCgain != si4735.getAgcGainIndex()) {
            DEBUG("Si4735Runtime::checkAGC() -> AGC Manual, att: %d\n", config.data.currentAGCgain);
            // A felhasználó manuális AGC beállítást kért
            si4735.setAutomaticGainControl(1, config.data.currentAGCgain); //-> AGCDIS = 1, AGCIDX = a konfig szerint
            stateChanged = true;
        }
    }

    // Ha küldtünk parancsot, olvassuk vissza az állapotot, hogy az SI4735 C++ objektum belső jelzője frissüljön
    if (stateChanged) {
        si4735.getAutomaticGainControl(); // Állapot újraolvasása
    }
}

/**
 * Manage Audio Mute
 * (SSB/CW frekvenciaváltáskor a zajszűrés miatt)
 */
void Si4735Runtime::manageHardwareAudioMute() {
#define MIN_ELAPSED_HARDWARE_AUDIO_MUTE_TIME 0 // Noise surpression SSB in mSec. 0 mSec = off //Was 0 (LWH)

    // Stop muting only if this condition has changed
    if (hardwareAudioMuteState and ((millis() - hardwareAudioMuteElapsed) > MIN_ELAPSED_HARDWARE_AUDIO_MUTE_TIME)) {
        // Ha a mute állapotban vagyunk és eltelt a minimális idő, akkor kikapcsoljuk a mute-t
        hardwareAudioMuteState = false;
        si4735.setHardwareAudioMute(false);
    }
}

/**
 * Manage Audio Mute
 * (SSB/CW frekvenciaváltáskor a zajszűrés miatt)
 */
void Si4735Runtime::hardwareAudioMuteOn() {
    si4735.setHardwareAudioMute(true);
    hardwareAudioMuteState = true;
    hardwareAudioMuteElapsed = millis();
}

/**
 * @brief Frissíti a signal quality cache-t
 */
void Si4735Runtime::updateSignalCache() {

    // Először frissítsük a chip állapotát
    si4735.getCurrentReceivedSignalQuality();

    uint8_t newRssi = si4735.getCurrentRSSI();
    uint8_t newSnr = si4735.getCurrentSNR();

    signalCache.rssi = newRssi;
    signalCache.snr = newSnr;
    signalCache.timestamp = millis();
    signalCache.isValid = true;
}

/**
 * @brief Frissíti a signal quality cache-t, ha szükséges
 */
void Si4735Runtime::updateSignalCacheIfNeeded() {
    unsigned long currentTime = millis();
    unsigned long timeDiff = currentTime - signalCache.timestamp;

    // Overflow védelem: ha a currentTime kisebb mint timestamp (overflow történt)
    bool timeoutExpired = (currentTime < signalCache.timestamp) || (timeDiff >= SIGNAL_QUALITY_CACHE_TIMEOUT_MS);

    if (!signalCache.isValid || timeoutExpired) {
        updateSignalCache();
    }
}

/**
 * @brief Invalidálja a signal quality cache-t (következő lekérdezéskor frissül)
 */
void Si4735Runtime::invalidateSignalCache() {
    signalCache.isValid = false;
    signalCache.timestamp = 0;
}

/**
 * @brief Lekéri a signal quality adatokat cache-elt módon (max 1mp késleltetés)
 * @return SignalQualityData A cache-elt signal quality adatok
 */
SignalQualityData Si4735Runtime::getSignalQuality() {
    updateSignalCacheIfNeeded();
    return signalCache;
}

/**
 * @brief Lekéri csak az RSSI értéket cache-elt módon
 * @return uint8_t RSSI érték
 */
uint8_t Si4735Runtime::getRSSI() { return getSignalQuality().rssi; }

/**
 * @brief Lekéri csak az SNR értéket cache-elt módon
 * @return uint8_t SNR érték
 */
uint8_t Si4735Runtime::getSNR() { return getSignalQuality().snr; }

/**
 * @brief Lekéri a signal quality adatokat valós időben (közvetlen chip lekérdezés)
 * @return SignalQualityData A friss signal quality adatok
 */
SignalQualityData Si4735Runtime::getSignalQualityRealtime() {

    // Frissítsük a chip állapotát
    si4735.getCurrentReceivedSignalQuality();

    SignalQualityData realtimeData;
    realtimeData.rssi = si4735.getCurrentRSSI();
    realtimeData.snr = si4735.getCurrentSNR();
    realtimeData.timestamp = millis();
    realtimeData.isValid = true;

    // DEBUG("Si4735Runtime::getSignalQualityRealtime() -> RSSI: %d, SNR: %d\n", realtimeData.rssi, realtimeData.snr);

    return realtimeData;
}
