#pragma once

#include <functional>
#include <map>
#include <queue>
#include <vector> // Navigációs stack-hez

#include "Config.h"
#include "IScreenManager.h"
#include "UIScreen.h"

// Deferred action struktúra - biztonságos képernyőváltáshoz
struct DeferredAction {
    enum Type { SwitchScreen, GoBack };

    Type type;
    const char *screenName;
    void *params;

    DeferredAction(Type t, const char *name = nullptr, void *p = nullptr) : type(t), screenName(name), params(p) {}
};

// Képernyő factory típus
using ScreenFactory = std::function<std::shared_ptr<UIScreen>()>;

// Képernyőkezelő
class ScreenManager : public IScreenManager {

  private:
    std::map<String, ScreenFactory> screenFactories;
    std::shared_ptr<UIScreen> currentScreen;
    const char *previousScreenName;
    uint32_t lastActivityTime;

    // Navigációs stack - többszintű back navigációhoz
    std::vector<String> navigationStack;

    // Screensaver előtti képernyő neve - screensaver visszatéréshez
    String screenBeforeScreenSaver;

    // Deferred action queue - biztonságos képernyőváltáshoz
    std::queue<DeferredAction> deferredActions;
    bool processingEvents = false;

    void registerDefaultScreenFactories();

  public:
    ScreenManager();

    // Aktuális képernyő lekérdezése
    std::shared_ptr<UIScreen> getCurrentScreen() const;

    // Előző képernyő neve
    String getPreviousScreenName() const;

    // Képernyő factory regisztrálása
    void registerScreenFactory(const char *screenName, ScreenFactory factory);

    // Deferred képernyő váltás - biztonságos váltás eseménykezelés közben
    void deferSwitchToScreen(const char *screenName, void *params = nullptr);

    // Deferred vissza váltás
    void deferGoBack();

    // Deferred actions feldolgozása - a main loop-ban hívandó
    void processDeferredActions();

    // Képernyő váltás név alapján - biztonságos verzió - IScreenManager
    bool switchToScreen(const char *screenName, void *params = nullptr) override;

    // Azonnali képernyő váltás - csak biztonságos kontextusban hívható
    bool immediateSwitch(const char *screenName, void *params = nullptr, bool isBackNavigation = false);

    // Vissza az előző képernyőre - biztonságos verzió - IScreenManager
    bool goBack() override;

    // Azonnali visszaváltás - csak biztonságos kontextusban hívható
    bool immediateGoBack();

    // Touch esemény kezelése
    bool handleTouch(const TouchEvent &event);

    // Rotary encoder esemény kezelése
    bool handleRotary(const RotaryEvent &event);

    // Loop hívás
    void loop();

    /**
     * segédfüggvény a dialog állapot ellenőrzéséhez
     */
    bool isCurrentScreenDialogActive() override;
};
