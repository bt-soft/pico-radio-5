#include "ScreenManager.h"

#include "Si4735Manager.h"
extern Si4735Manager *si4735Manager; // main.cpp-ból

/**
 * @brief Képernyőkezelő osztály konstruktor
 */
void ScreenManager::registerDefaultScreenFactories() {
    // registerScreenFactory(SCREEN_NAME_FM, [](TFT_eSPI &tft_param) { return std::make_shared<FMScreen>(tft_param, *si4735Manager); });
    // registerScreenFactory(SCREEN_NAME_AM, [](TFT_eSPI &tft_param) { return std::make_shared<AMScreen>(tft_param, *si4735Manager); });
    // registerScreenFactory(SCREEN_NAME_MEMORY, [](TFT_eSPI &tft_param) { return std::make_shared<MemoryScreen>(tft_param, *si4735Manager); });
    // registerScreenFactory(SCREEN_NAME_SCAN, [](TFT_eSPI &tft_param) { return std::make_shared<ScanScreen>(tft_param, si4735Manager); });
    // registerScreenFactory(SCREEN_NAME_SCREENSAVER, [](TFT_eSPI &tft_param) { return std::make_shared<ScreenSaverScreen>(tft_param, *si4735Manager); }); // setup képernyők regisztrálása
    // registerScreenFactory(SCREEN_NAME_SETUP, [](TFT_eSPI &tft_param) { return std::make_shared<SetupScreen>(tft_param); });
    // registerScreenFactory("SETUP_SYSTEM", [](TFT_eSPI &tft_param) { return std::make_shared<SetupSystemScreen>(tft_param); });
    // registerScreenFactory("SETUP_SI4735", [](TFT_eSPI &tft_param) { return std::make_shared<SetupSi4735Screen>(tft_param); });

    // test képernyők regisztrálása
    registerScreenFactory(SCREEN_NAME_TEST, []() { return std::make_shared<ScreenTest>(); });
    registerScreenFactory(SCREEN_NAME_EMPTY, []() { return std::make_shared<ScreenEmpty>(); });
}

// MemoryScreen paraméter kezelés implementációja
// void ScreenManager::setMemoryScreenParams(bool autoAdd, const char *rdsName) { memoryScreenParamsBuffer = MemoryScreenParams(autoAdd, rdsName); }
// void ScreenManager::switchToMemoryScreen() { switchToScreen(SCREEN_NAME_MEMORY, &memoryScreenParamsBuffer); }
