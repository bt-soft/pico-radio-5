#include "ScreenManager.h"

#include "ScanScreen.h"
#include "ScreenAM.h"
#include "ScreenEmpty.h"
#include "ScreenFM.h"
#include "ScreenMemory.h"
#include "ScreenScreenSaver.h"
#include "ScreenSetup.h"
#include "ScreenSetupAudioProc.h"
#include "ScreenSetupSi4735.h"
#include "ScreenSetupSystem.h"
#include "ScreenTest.h"

/**
 * @brief Képernyőkezelő osztály konstruktor
 */
void ScreenManager::registerDefaultScreenFactories() {
    registerScreenFactory(SCREEN_NAME_FM, []() { return std::make_shared<ScreenFM>(); });
    registerScreenFactory(SCREEN_NAME_AM, []() { return std::make_shared<ScreenAM>(); });
    registerScreenFactory(SCREEN_NAME_SCREENSAVER, []() { return std::make_shared<ScreenScreenSaver>(); });
    registerScreenFactory(SCREEN_NAME_MEMORY, []() { return std::make_shared<ScreenMemory>(); });
    registerScreenFactory(SCREEN_NAME_SCAN, []() { return std::make_shared<ScanScreen>(); });

    // Setup képernyők regisztrálása
    registerScreenFactory(SCREEN_NAME_SETUP, []() { return std::make_shared<ScreenSetup>(); });
    registerScreenFactory(SCREEN_NAME_SETUP_SYSTEM, []() { return std::make_shared<ScreenSetupSystem>(); });
    registerScreenFactory(SCREEN_NAME_SETUP_SI4735, []() { return std::make_shared<ScreenSetupSi4735>(); });
    registerScreenFactory(SCREEN_NAME_SETUP_AUDIO_PROC, []() { return std::make_shared<ScreenSetupAudioProc>(); });

    // Teszt képernyők regisztrálása
    // registerScreenFactory(SCREEN_NAME_TEST, []() { return std::make_shared<ScreenTest>(); });
    // registerScreenFactory(SCREEN_NAME_EMPTY, []() { return std::make_shared<ScreenEmpty>(); });
}
