#include "ScreenManager.h"

#include "ScreenAM.h"
#include "ScreenEmpty.h"
#include "ScreenFM.h"
#include "ScreenMemory.h"
#include "ScreenScreenSaver.h"
#include "ScreenTest.h"

/**
 * @brief Képernyőkezelő osztály konstruktor
 */
void ScreenManager::registerDefaultScreenFactories() {
    // registerScreenFactory(SCREEN_NAME_FM, [](TFT_eSPI &tft_param) { return std::make_shared<FMScreen>(tft_param, *si4735Manager); });
    // registerScreenFactory(SCREEN_NAME_AM, [](TFT_eSPI &tft_param) { return std::make_shared<AMScreen>(tft_param, *si4735Manager); });
    // registerScreenFactory(SCREEN_NAME_MEMORY, [](TFT_eSPI &tft_param) { return std::make_shared<MemoryScreen>(tft_param, *si4735Manager); });
    // registerScreenFactory(SCREEN_NAME_SCAN, [](TFT_eSPI &tft_param) { return std::make_shared<ScanScreen>(tft_param, si4735Manager); });

    registerScreenFactory(SCREEN_NAME_FM, []() { return std::make_shared<ScreenFM>(); });
    registerScreenFactory(SCREEN_NAME_AM, []() { return std::make_shared<ScreenAM>(); });
    registerScreenFactory(SCREEN_NAME_SCREENSAVER, []() { return std::make_shared<ScreenScreenSaver>(); });
    registerScreenFactory(SCREEN_NAME_MEMORY, []() { return std::make_shared<ScreenMemory>(); });

    // Setup képernyők regisztrálása
    // registerScreenFactory(SCREEN_NAME_SETUP, [](TFT_eSPI &tft_param) { return std::make_shared<SetupScreen>(tft_param); });
    // registerScreenFactory("SETUP_SYSTEM", [](TFT_eSPI &tft_param) { return std::make_shared<SetupSystemScreen>(tft_param); });
    // registerScreenFactory("SETUP_SI4735", [](TFT_eSPI &tft_param) { return std::make_shared<SetupSi4735Screen>(tft_param); });

    // Teszt képernyők regisztrálása
    registerScreenFactory(SCREEN_NAME_TEST, []() { return std::make_shared<ScreenTest>(); });
    registerScreenFactory(SCREEN_NAME_EMPTY, []() { return std::make_shared<ScreenEmpty>(); });
}
