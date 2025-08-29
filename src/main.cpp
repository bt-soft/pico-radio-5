#include <Arduino.h>

#include "PicoMemoryInfo.h"
#include "PicoSensorUtils.h"
#include "SplashScreen.h"

//-------------------- Config
#include "AudioCore1Manager.h"
#include "AudioProcessor.h"
#include "BandStore.h"
#include "Config.h"
#include "EepromLayout.h"
#include "StationStore.h"
#include "StoreEepromBase.h"
extern Config config;
extern FmStationStore fmStationStore;
extern AmStationStore amStationStore;
extern BandStore bandStore;

//------------------- Rotary Encoder
#include <RPi_Pico_TimerInterrupt.h>
RPI_PICO_Timer rotaryTimer(0); // 0-ás timer használata
#include "RotaryEncoder.h"
RotaryEncoder rotaryEncoder = RotaryEncoder(PIN_ENCODER_CLK, PIN_ENCODER_DT, PIN_ENCODER_SW, ROTARY_ENCODER_STEPS_PER_NOTCH);
#define ROTARY_ENCODER_SERVICE_INTERVAL_IN_MSEC 1 // 1msec

// A CW/RTTY dekóderhez szükséges
RPI_PICO_Timer audioDecoderTimer(1); // 1-es timer használata

//------------------ TFT
#include <TFT_eSPI.h>
TFT_eSPI tft;
uint16_t SCREEN_W;
uint16_t SCREEN_H;

//------------------ SI4735
#include "Band.h" // Band típusok konstansaihoz
#include "Si4735Manager.h"
Si4735Manager *pSi4735Manager = nullptr; // Si4735Manager: NEM lehet (hardware inicializálás miatt) statikus, mert HW inicializálások is vannak benne

//-------------------- Screens
// Globális képernyőkezelő pointer - inicializálás a setup()-ban történik
#include "ScreenManager.h"
ScreenManager *screenManager = nullptr;
IScreenManager **iScreenManager = (IScreenManager **)&screenManager; // A UIComponent használja

// -----------------------------------------------------------------------------

/**
 * @brief  Hardware timer interrupt service routine a rotaryhoz
 */
bool rotaryTimerHardwareInterruptHandler(struct repeating_timer *t) {
    rotaryEncoder.service();
    return true;
}

// -----------------------------------------------------------------------------

//-

/**
 * @brief Program belépési pont.
 */
void setup() {
#ifdef __DEBUG
    Serial.begin(115200);
#endif

    // PICO AD inicializálása
    PicoSensorUtils::init();

    // Beeper
    pinMode(PIN_BEEPER, OUTPUT);
    digitalWrite(PIN_BEEPER, LOW);

    // TFT LED háttérvilágítás kimenet
    pinMode(PIN_TFT_BACKGROUND_LED, OUTPUT);
    Utils::setTftBacklight(TFT_BACKGROUND_LED_MAX_BRIGHTNESS); // TFT inicializálása DC módban
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK); // Fekete háttér a splash screen-hez

    // UI komponensek számára képernyő méretek inicializálása
    SCREEN_W = tft.width();
    SCREEN_H = tft.height();

#ifdef DEBUG_WAIT_FOR_SERIAL
    Utils::debugWaitForSerial(tft);
#endif

    // Csak az általános információkat jelenítjük meg először (SI4735 nélkül)
    // Program cím és build info megjelenítése
    tft.setFreeFont();
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(PROGRAM_NAME, tft.width() / 2, 20);

    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Version " + String(PROGRAM_VERSION), tft.width() / 2, 50);
    tft.drawString(PROGRAM_AUTHOR, tft.width() / 2, 70);

    // Build info
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Build: " + String(__DATE__) + " " + String(__TIME__), tft.width() / 2, 100);

    // Inicializálási progress
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Initializing...", tft.width() / 2, 140);

    // EEPROM inicializálása (A fordítónak muszáj megadni egy típust, itt most egy Config_t-t használunk, igaziból mindegy)
    tft.drawString("Loading EEPROM...", tft.width() / 2, 160);
    StoreEepromBase<Config_t>::init(); // Meghívjuk a statikus init metódust

    // Ha a bekapcsolás alatt nyomva tartjuk a rotary gombját, akkor töröljük a konfigot
    if (digitalRead(PIN_ENCODER_SW) == LOW) {
        DEBUG("Encoder button pressed during startup, restoring defaults...\n");
        Utils::beepTick();
        delay(1500);
        if (digitalRead(PIN_ENCODER_SW) == LOW) { // Ha még mindig nyomják

            DEBUG("Restoring default settings...\n");
            Utils::beepTick();
            config.loadDefaults();
            fmStationStore.loadDefaults();
            amStationStore.loadDefaults();
            bandStore.loadDefaults();

            DEBUG("Save default settings...\n");
            Utils::beepTick();
            config.checkSave();
            bandStore.checkSave(); // Band adatok mentése
            fmStationStore.checkSave();
            amStationStore.checkSave();

            Utils::beepTick();
            DEBUG("Default settings restored!\n");
        }
    } else {
        // konfig betöltése
        tft.drawString("Loading config...", tft.width() / 2, 180);
        config.load();
    }

    // Rotary Encoder beállítása
    rotaryEncoder.setDoubleClickEnabled(true);                                  // Dupla kattintás engedélyezése
    rotaryEncoder.setAccelerationEnabled(config.data.rotaryAcceleratonEnabled); // Gyorsítás engedélyezése a rotary enkóderhez
    // Pico HW Timer1 beállítása a rotaryhoz
    rotaryTimer.attachInterruptInterval(ROTARY_ENCODER_SERVICE_INTERVAL_IN_MSEC * 1000, rotaryTimerHardwareInterruptHandler);

    // Kell kalibrálni a TFT Touch-t?
    if (Utils::isZeroArray(config.data.tftCalibrateData)) {
        Utils::beepError();
        Utils::tftTouchCalibrate(tft, config.data.tftCalibrateData);
    }

    // Beállítjuk a touch scren-t
    tft.setTouch(config.data.tftCalibrateData);

    // Állomáslisták és band adatok betöltése az EEPROM-ból (a config után!)
    tft.drawString("Loading stations & bands...", tft.width() / 2, 200);
    bandStore.load(); // Band adatok betöltése
    fmStationStore.load();
    amStationStore.load();

    // Splash screen megjelenítése inicializálás közben
    // Most átváltunk a teljes splash screen-re az SI4735 infókkal
    SplashScreen *splash = new SplashScreen(tft);
    splash->show(true, 8);

    // Splash screen megjelenítése progress bar-ral    // Lépés 1: I2C inicializálás
    splash->updateProgress(1, 9, "Initializing I2C...");

    // Az si473x (Nem a default I2C lábakon [4,5] van!!!)
    Wire.setSDA(PIN_SI4735_I2C_SDA); // I2C for SI4735 SDA
    Wire.setSCL(PIN_SI4735_I2C_SCL); // I2C for SI4735 SCL
    Wire.begin();
    delay(300);

    // Si4735Manager inicializálása itt
    splash->updateProgress(2, 9, "Initializing SI4735 Manager...");
    if (pSi4735Manager == nullptr) {
        pSi4735Manager = new Si4735Manager();
        // BandStore beállítása a Si4735Manager-ben
        pSi4735Manager->setBandStore(&bandStore);
    }

    // KRITIKUS: Band tábla dinamikus adatainak EGYSZERI inicializálása RÖGTÖN a Si4735Manager létrehozása után!
    pSi4735Manager->initializeBandTableData(true); // forceReinit = true az első inicializálásnál

    // Si4735 inicializálása
    splash->updateProgress(3, 9, "Detecting SI4735...");
    int16_t si4735Addr = pSi4735Manager->getDeviceI2CAddress();
    if (si4735Addr == 0) {
        Utils::beepError();
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("SI4735 NOT DETECTED!", tft.width() / 2, tft.height() / 2);
        DEBUG("Si4735 not detected");
        while (true) // nem megyünk tovább
            ;
    } // Lépés 4: SI4735 konfigurálás
    splash->updateProgress(4, 6, "Configuring SI4735...");
    pSi4735Manager->setDeviceI2CAddress(si4735Addr == 0x11 ? 0 : 1); // Sets the I2C Bus Address, erre is szükség van...    splash->drawSI4735Info(si4735Manager->getSi4735());

    delay(300);
    //--------------------------------------------------------------------

    // Lépés 5: Frekvencia beállítások
    splash->updateProgress(4, 9, "Setting up radio...");
    pSi4735Manager->init(true);
    pSi4735Manager->getSi4735().setVolume(config.data.currVolume); // Hangerő visszaállítása

    delay(100);

    // Kezdő képernyőtípus beállítása
    splash->updateProgress(5, 9, "Preparing display...");
    const char *startScreenName = pSi4735Manager->getCurrentBandType() == FM_BAND_TYPE ? SCREEN_NAME_FM : SCREEN_NAME_AM;
    delay(100);

    //--------------------------------------------------------------------
    // Lépés 7: Core1 Audio Manager inicializálása
    splash->updateProgress(6, 9, "Starting Core1 audio processor...");

    // Core1 Audio Manager inicializálása a megfelelő FFT gain referenciákkal
    bool core1InitSuccess = AudioCore1Manager::init(config.data.audioFftConfigAm,                           // AM FFT gain referencia
                                                    config.data.audioFftConfigFm,                           // FM FFT gain referencia
                                                    PIN_AUDIO_INPUT,                                        // Audio bemenet pin
                                                    AudioProcessorConstants::DEFAULT_FM_SAMPLING_FREQUENCY, // Mintavételezési frekvencia, FM -> 30kHz
                                                    AudioProcessorConstants::DEFAULT_FFT_SAMPLES            // Kezdeti FFT méret -> 512
    );
    if (!core1InitSuccess) {
        DEBUG("HIBA: A Core1 Audio Manager inicializálás sikertelen!\n");
        Utils::beepError();
        // Folytatjuk anélkül is, de spectrum nem fog működni
    }

    delay(100);

    // Lépés 8: ScreenManager inicializálása
    splash->updateProgress(7, 9, "Preparing display...");

    // ScreenManager inicializálása itt, amikor minden más már kész
    if (screenManager == nullptr) {
        screenManager = new ScreenManager();
    }

    screenManager->switchToScreen(startScreenName); // A kezdő képernyő

    delay(100);

    // Lépés 9: Finalizálás
    splash->updateProgress(8, 9, "Starting up...");

    delay(100); // Rövidebb delay

    splash->updateProgress(9, 9, "Starting OK");

    delay(100); // Rövidebb delay

    // Splash screen eltűntetése
    splash->hide();

    //--------------------------------------------------------------------

    // SplashScreen törlése, már nincs rá szükség
    delete splash;

    // Csippantunk egyet
    Utils::beepTick();
}

/**
 * @brief Fő ciklus
 */
void loop() {

//------------------- EEPROM mentés figyelése
#define EEPROM_SAVE_CHECK_INTERVAL 1000 * 60 * 5 // 5 perc
    static uint32_t lastEepromSaveCheck = 0;
    if (millis() - lastEepromSaveCheck >= EEPROM_SAVE_CHECK_INTERVAL) {
        config.checkSave();
        bandStore.checkSave(); // Band adatok mentése
        fmStationStore.checkSave();
        amStationStore.checkSave();
        lastEepromSaveCheck = millis();
    }
//------------------- Memória információk megjelenítése
#ifdef SHOW_MEMORY_INFO
    static uint32_t lasDebugMemoryInfo = 0;
    if (millis() - lasDebugMemoryInfo >= MEMORY_INFO_INTERVAL) {
        PicoMemoryInfo::debugMemoryInfo();
        lasDebugMemoryInfo = millis();
    }
#endif
    //------------------- Touch esemény kezelése
    uint16_t touchX, touchY;
    bool touchedRaw = tft.getTouch(&touchX, &touchY);
    bool validCoordinates = true;
    if (touchedRaw) {
        if (touchX > tft.width() || touchY > tft.height()) {
            validCoordinates = false;
        }
    }

    static bool lastTouchState = false;
    static uint16_t lastTouchX = 0, lastTouchY = 0;
    bool touched = touchedRaw && validCoordinates;

    // Touch press event (immediate response)
    if (touched && !lastTouchState) {
        TouchEvent touchEvent(touchX, touchY, true);
        screenManager->handleTouch(touchEvent);
        lastTouchX = touchX;
        lastTouchY = touchY;
    } else if (!touched && lastTouchState) { // Touch release event (immediate response)
        TouchEvent touchEvent(lastTouchX, lastTouchY, false);
        screenManager->handleTouch(touchEvent);
    }

    lastTouchState = touched;

    //------------------- Rotary Encoder esemény kezelése
    // Rotary Encoder olvasása
    RotaryEncoder::EncoderState encoderState = rotaryEncoder.read();

    // Rotary encoder eseményeinek továbbítása a ScreenManager-nek
    if (encoderState.direction != RotaryEncoder::Direction::None || encoderState.buttonState != RotaryEncoder::ButtonState::Open) {

        // RotaryEvent létrehozása a ScreenManager típusaival
        RotaryEvent::Direction direction = RotaryEvent::Direction::None;
        if (encoderState.direction == RotaryEncoder::Direction::Up) {
            direction = RotaryEvent::Direction::Up;
        } else if (encoderState.direction == RotaryEncoder::Direction::Down) {
            direction = RotaryEvent::Direction::Down;
        }

        RotaryEvent::ButtonState buttonState = RotaryEvent::ButtonState::NotPressed;
        if (encoderState.buttonState == RotaryEncoder::ButtonState::Clicked) {
            buttonState = RotaryEvent::ButtonState::Clicked;
        } else if (encoderState.buttonState == RotaryEncoder::ButtonState::DoubleClicked) {
            buttonState = RotaryEvent::ButtonState::DoubleClicked;
        }

        // Esemény továbbítása a ScreenManager-nek
        RotaryEvent rotaryEvent(direction, buttonState, encoderState.value);
        if (screenManager) {
            screenManager->handleRotary(rotaryEvent);
        }
        // bool handled = screenManager->handleRotary(rotaryEvent);
        // DEBUG("Rotary event handled by screen: %s\n", handled ? "YES" : "NO");
    }

    if (screenManager) {
        // Deferred actions feldolgozása - biztonságos képernyőváltások végrehajtása
        screenManager->loop();
    }

    // SI4735 loop hívása, squelch és hardver némítás kezelése
    if (pSi4735Manager) {
        pSi4735Manager->loop();
    }

    // // Core1 Audio Manager debug információk kiírása
    // static uint32_t lasAudioCore1ManagerDebugInfo = 0;
    // if (millis() - lasAudioCore1ManagerDebugInfo >= 10 * 1000) {
    //     AudioCore1Manager::debugInfo(); // Core1 Audio Manager debug info kiírása
    //     lasAudioCore1ManagerDebugInfo = millis();
    // }
}
