#ifndef __SCREEN_MANAGER_H
#define __SCREEN_MANAGER_H

#include <functional>
#include <map>    // Tartalmazva volt, de explicit jobb
#include <queue>  // Tartalmazva volt, de explicit jobb
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
    ScreenManager() : previousScreenName(nullptr), lastActivityTime(millis()) { registerDefaultScreenFactories(); }

    // Aktuális képernyő lekérdezése
    std::shared_ptr<UIScreen> getCurrentScreen() const { return currentScreen; }

    // Előző képernyő neve
    String getPreviousScreenName() const { return previousScreenName; }

    // Képernyő factory regisztrálása
    void registerScreenFactory(const char *screenName, ScreenFactory factory) { screenFactories[screenName] = factory; }

    // Deferred képernyő váltás - biztonságos váltás eseménykezelés közben
    void deferSwitchToScreen(const char *screenName, void *params = nullptr) {
        DEBUG("ScreenManager: Deferring switch to screen '%s'\n", screenName);
        deferredActions.push(DeferredAction(DeferredAction::SwitchScreen, screenName, params));
    }

    // Deferred vissza váltás
    void deferGoBack() {
        DEBUG("ScreenManager: Deferring go back\n");
        deferredActions.push(DeferredAction(DeferredAction::GoBack));
    }

    // Deferred actions feldolgozása - a main loop-ban hívandó
    void processDeferredActions() {

        while (!deferredActions.empty()) {
            const DeferredAction &action = deferredActions.front();

            DEBUG("ScreenManager: Processing deferred action type=%d\n", static_cast<int>(action.type));

            if (action.type == DeferredAction::SwitchScreen) {
                immediateSwitch(action.screenName, action.params);
            } else if (action.type == DeferredAction::GoBack) {
                immediateGoBack();
            }

            deferredActions.pop();
        }
    }

    // Képernyő váltás név alapján - biztonságos verzió - IScreenManager
    bool switchToScreen(const char *screenName, void *params = nullptr) override {
        if (processingEvents) {
            // Eseménykezelés közben - halasztott váltás
            deferSwitchToScreen(screenName, params);
            return true;
        } else {
            // Biztonságos - azonnali váltás
            return immediateSwitch(screenName, params);
        }
    }

    // Azonnali képernyő váltás - csak biztonságos kontextusban hívható
    bool immediateSwitch(const char *screenName, void *params = nullptr, bool isBackNavigation = false) {

        // Ha már ez a képernyő aktív, nem csinálunk semmit
        if (currentScreen && STREQ(currentScreen->getName(), screenName)) {
            return true;
        }

        // Factory keresése
        auto it = screenFactories.find(screenName);
        if (it == screenFactories.end()) {
            DEBUG("ScreenManager: Screen factory not found for '%s'\n", screenName);
            return false;
        }

        // Navigációs stack kezelése KÉPERNYŐVÁLTÁS ELŐTT - csak forward navigációnál
        if (currentScreen && !isBackNavigation) {
            const char *currentName = currentScreen->getName();

            // SCREENSAVER SPECIÁLIS KEZELÉS:
            if (STREQ(screenName, SCREEN_NAME_SCREENSAVER)) {
                // Ha képernyővédőre váltunk, eltároljuk az aktuális képernyő nevét
                screenBeforeScreenSaver = String(currentName);
                DEBUG("ScreenManager: Screensaver activated from '%s'\n", currentName);

            } else if (!STREQ(currentName, SCREEN_NAME_SCREENSAVER)) {
                // Normál forward navigáció - jelenlegi képernyő hozzáadása a stackhez
                // (de csak ha nem screensaver-ről váltunk)
                navigationStack.push_back(String(currentName));
                DEBUG("ScreenManager: Added '%s' to navigation stack (size: %d)\n", currentName, navigationStack.size());
            }

        } else if (isBackNavigation) {
            DEBUG("ScreenManager: Back navigation - not adding to stack\n");
        }

        // Jelenlegi képernyő törlése
        if (currentScreen) {
            const char *currentName = currentScreen->getName();

            // previousScreenName csak akkor frissül, ha nem képernyővédőre váltunk
            // Ez biztosítja, hogy a képernyővédő után vissza tudjunk térni az eredeti képernyőre
            if (!STREQ(screenName, SCREEN_NAME_SCREENSAVER)) {
                previousScreenName = currentName;
            }

            currentScreen->deactivate();
            currentScreen.reset(); // Memória felszabadítása
            DEBUG("ScreenManager: Destroyed screen '%s'\n", currentName);
        }

        // TFT display törlése a képernyőváltás előtt
        ::tft.fillScreen(TFT_BLACK);
        DEBUG("ScreenManager: Display cleared for screen switch\n");

        // Új képernyő létrehozása
        currentScreen = it->second();
        if (currentScreen) {
            currentScreen->setScreenManager(this);
            if (params) {
                currentScreen->setParameters(params);
            }
            // Fontos: Az activate() hívása *előtt* állítjuk be a lastActivityTime-ot,
            // ha nem a képernyővédőre váltunk, hogy az activate() felülírhassa, ha akarja.
            if (!STREQ(screenName, SCREEN_NAME_SCREENSAVER)) {
                lastActivityTime = millis();
            }
            currentScreen->activate();
            DEBUG("ScreenManager: Created and activated screen '%s'\n", screenName);
            return true;
        } else {
            DEBUG("ScreenManager: Failed to create screen '%s'\n", screenName);
        }
        return false;
    }

    // Vissza az előző képernyőre - biztonságos verzió - IScreenManager
    bool goBack() override {
        if (processingEvents) {
            // Eseménykezelés közben - halasztott váltás
            deferGoBack();
            return true;
        } else {
            // Biztonságos - azonnali váltás
            return immediateGoBack();
        }
    }

    // Azonnali visszaváltás - csak biztonságos kontextusban hívható
    bool immediateGoBack() {
        // Speciális kezelés: ha screensaver-ből jövünk vissza
        if (currentScreen && STREQ(currentScreen->getName(), SCREEN_NAME_SCREENSAVER)) {
            if (!screenBeforeScreenSaver.isEmpty()) {
                DEBUG("ScreenManager: Going back from screensaver to '%s'\n", screenBeforeScreenSaver.c_str());
                String targetScreen = screenBeforeScreenSaver;
                screenBeforeScreenSaver = String();                          // Clear after use
                return immediateSwitch(targetScreen.c_str(), nullptr, true); // isBackNavigation = true
            }
        }

        // Navigációs stack használata a többszintű back navigációhoz
        if (!navigationStack.empty()) {
            String previousScreen = navigationStack.back();
            navigationStack.pop_back();
            DEBUG("ScreenManager: Going back to '%s' from stack (remaining: %d)\n", previousScreen.c_str(), navigationStack.size());
            return immediateSwitch(previousScreen.c_str(), nullptr, true); // isBackNavigation = true
        }

        // Fallback - régi egyszintű viselkedés
        if (previousScreenName != nullptr) {
            DEBUG("ScreenManager: Fallback to old previousScreenName: '%s'\n", previousScreenName);
            return immediateSwitch(previousScreenName, nullptr, true); // isBackNavigation = true
        }

        DEBUG("ScreenManager: No screen to go back to\n");
        return false;
    }

    // Touch esemény kezelése
    bool handleTouch(const TouchEvent &event) {
        if (currentScreen) {
            if (!STREQ(currentScreen->getName(), SCREEN_NAME_SCREENSAVER)) {
                lastActivityTime = millis();
            }
            processingEvents = true;
            bool result = currentScreen->handleTouch(event);
            processingEvents = false;
            return result;
        }
        return false;
    }

    // Rotary encoder esemény kezelése
    bool handleRotary(const RotaryEvent &event) {
        if (currentScreen) {
            if (!STREQ(currentScreen->getName(), SCREEN_NAME_SCREENSAVER)) {
                lastActivityTime = millis();
            }
            processingEvents = true;
            bool result = currentScreen->handleRotary(event);
            processingEvents = false;
            return result;
        }
        return false;
    }

    // Loop hívás
    void loop() {

        // Először a halasztott műveletek feldolgozása
        processDeferredActions();

        if (currentScreen) {

            // Képernyővédő időzítő ellenőrzése
            uint32_t screenSaverTimeoutMs = config.data.screenSaverTimeoutMinutes * 60 * 1000; // Percek milliszekundumra konvertálva

            if (screenSaverTimeoutMs > 0 &&                                  // Ha a képernyővédő engedélyezve van (idő > 0)
                !STREQ(currentScreen->getName(), SCREEN_NAME_SCREENSAVER) && // És nem a képernyővédőn vagyunk
                lastActivityTime != 0 &&                                     // És volt már aktivitás
                (millis() - lastActivityTime > screenSaverTimeoutMs)) {      // És lejárt az idő

                switchToScreen(SCREEN_NAME_SCREENSAVER);
                // lastActivityTime frissül, amikor a felhasználó újra interakcióba lép a képernyővédőn,
                // és visszaváltáskor az immediateSwitch-ben.
            }

            // Csak akkor rajzolunk, ha valóban szükséges
            if (currentScreen->isRedrawNeeded()) {
                currentScreen->draw();
            }

            currentScreen->loop();
        }
    }

    /**
     * segédfüggvény a dialog állapot ellenőrzéséhez
     */
    bool isCurrentScreenDialogActive() override {

        auto currentScreen = this->getCurrentScreen();
        if (currentScreen == nullptr) {
            return false;
        }

        return currentScreen->isDialogActive();
    }
};

#endif // __SCREEN_MANAGER_H