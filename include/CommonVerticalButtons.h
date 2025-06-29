#ifndef __COMMON_VERTICAL_BUTTONS_H
#define __COMMON_VERTICAL_BUTTONS_H

#include "FrequencyInputDialog.h"
#include "ScreenRadioBase.h"
#include "Si4735Manager.h"
#include "UIButton.h"
#include "ValueChangeDialog.h"
#include "rtVars.h"

// ===================================================================
// UNIVERZÁLIS GOMB AZONOSÍTÓK - Egységes ID rendszer
// ===================================================================

namespace VerticalButtonIDs {
static constexpr uint8_t MUTE = 10;    ///< Némítás gomb (univerzális)
static constexpr uint8_t VOLUME = 11;  ///< Hangerő beállítás gomb (univerzális)
static constexpr uint8_t AGC = 12;     ///< Automatikus erősítés szabályozás (univerzális)
static constexpr uint8_t ATT = 13;     ///< Csillapító (univerzális)
static constexpr uint8_t SQUELCH = 14; ///< Zajzár beállítás (univerzális)
static constexpr uint8_t FREQ = 15;    ///< Frekvencia input (univerzális)
static constexpr uint8_t SETUP = 16;   ///< Beállítások képernyő (univerzális)
static constexpr uint8_t MEMO = 17;    ///< Memória funkciók (univerzális)
} // namespace VerticalButtonIDs

/**
 * @brief Közös függőleges gombsor statikus osztály dialógus támogatással
 * @details Handler függvények képesek dialógusokat megjeleníteni
 */
class CommonVerticalButtons {
  public:
    // =====================================================================
    // HANDLER FÜGGVÉNY TÍPUSOK - Egységes ScreenRadioBase alapú megközelítés
    // =====================================================================
    using HandlerFunc = void (*)(const UIButton::ButtonEvent &, ScreenRadioBase *);

    // Kompatibilitás miatt megtartjuk a régi neveket, de mind azonos típusra mutatnak
    using CommonHandlerFunc = HandlerFunc;
    using Si4735HandlerFunc = HandlerFunc;
    using DialogHandlerFunc = HandlerFunc;

    /**
     * @brief Gomb statikus adatok struktúrája
     */
    struct ButtonDefinition {
        uint8_t id;                         ///< Gomb azonosító
        const char *label;                  ///< Gomb felirata
        UIButton::ButtonType type;          ///< Gomb típusa
        UIButton::ButtonState initialState; ///< Kezdeti állapot
        uint16_t height;                    ///< Gomb magassága
        HandlerFunc handler;                ///< Egységes handler függvény
    };

    // =====================================================================
    // UNIVERZÁLIS GOMBKEZELŐ METÓDUSOK - Dialógus támogatással
    // =====================================================================

    /**
     * @brief Segédmetódus gombállapot frissítéséhez ScreenRadioBase-en keresztül (RTTI-mentes)
     */
    static void updateButtonStateInScreen(ScreenRadioBase *screen, uint8_t buttonId, UIButton::ButtonState state) {
        if (!screen)
            return;

        // Vegigmegyünk a screen összes gyerek komponensén
        auto &components = screen->getChildren();
        for (auto &component : components) {

            // Próbáljuk meg UIButton-ként kezelni (raw pointer cast)
            UIButton *button = reinterpret_cast<UIButton *>(component.get());

            // Ellenőrizzük, hogy valóban UIButton-e az ID alapján
            // (Az UIButton ID-k a VerticalButtonIDs tartományban vannak)
            if (button && button->getId() == buttonId) {
                button->setButtonState(state);
                break;
            }
        }
    }

    /**
     * @brief MUTE gomb kezelő
     */
    static void handleMuteButton(const UIButton::ButtonEvent &event, ScreenRadioBase *screen = nullptr) {
        if (event.state != UIButton::EventButtonState::On && event.state != UIButton::EventButtonState::Off) {
            return;
        }
        rtv::muteStat = event.state == UIButton::EventButtonState::On;
        ::pSi4735Manager->getSi4735().setAudioMute(rtv::muteStat);
    }

    /**
     * @brief VOLUME gomb kezelő - ValueChangeDialog megjelenítése
     */
    static void handleVolumeButton(const UIButton::ButtonEvent &event, ScreenRadioBase *screen) {
        if (event.state != UIButton::EventButtonState::Clicked || !screen) {
            return;
        }

        // ValueChangeDialog létrehozása a statikus változó pointerével
        auto volumeDialog = std::make_shared<ValueChangeDialog>(
            screen,                                                                    //
            "Volume Control", "Adjust radio volume (0-63):",                           // Cím, felirat
            &config.data.currVolume,                                                   // Pointer a statikus változóra
            Si4735Constants::SI4735_MIN_VOLUME, Si4735Constants::SI4735_MAX_VOLUME, 1, // Min, Max, Step
            [](const std::variant<int, float, bool> &newValue) {                       // Callback a változásra
                if (std::holds_alternative<int>(newValue)) {
                    int volume = std::get<int>(newValue);
                    DEBUG("Volume changed to: %d\n", volume);
                    ::pSi4735Manager->getSi4735().setVolume(static_cast<uint8_t>(volume));
                }
            },
            nullptr,             // Nincs külön dialog bezárás callback
            Rect(-1, -1, 280, 0) // Auto-magasság
        );
        screen->showDialog(volumeDialog);
    }

    /**
     * @brief AGC gomb kezelő
     */
    static void handleAGCButton(const UIButton::ButtonEvent &event, ScreenRadioBase *screen = nullptr) {

        if (event.state != UIButton::EventButtonState::On && event.state != UIButton::EventButtonState::Off) {
            return;
        }

        if (event.state == UIButton::EventButtonState::On) {
            // Az attenuátor gomb OFF állapotba helyezése, ha az AGC be van kapcsolva
            updateButtonStateInScreen(screen, VerticalButtonIDs::ATT, UIButton::ButtonState::Off);
            config.data.agcGain = static_cast<uint8_t>(Si4735Runtime::AgcGainMode::Automatic); // AGC Automatic
        } else if (event.state == UIButton::EventButtonState::Off) {
            config.data.agcGain = static_cast<uint8_t>(Si4735Runtime::AgcGainMode::Off); // AGC OFF
        }

        // AGC beállítása
        ::pSi4735Manager->checkAGC();

        // StatusLine update
        if (screen->getStatusLineComp()) {
            screen->getStatusLineComp()->updateAgc();
        }
    }

    /**
     * @brief ATTENUATOR gomb kezelő
     */
    static void handleAttenuatorButton(const UIButton::ButtonEvent &event, ScreenRadioBase *screen = nullptr) {

        // Bekapcsolás
        if (event.state == UIButton::EventButtonState::On) {

            // Az AGC gomb OFF állapotba helyezése, ha az Attenuator be van kapcsolva
            updateButtonStateInScreen(screen, VerticalButtonIDs::AGC, UIButton::ButtonState::Off);

            config.data.agcGain = static_cast<uint8_t>(Si4735Runtime::AgcGainMode::Manual); // AGC ->Manual

            // ValueChangeDialog létrehozása a statikus változó pointerével
            uint8_t maxGain = ::pSi4735Manager->isCurrentDemodFM() ? Si4735Constants::SI4735_MAX_ATTENNUATOR_FM : Si4735Constants::SI4735_MAX_ATTENNUATOR_AM;

            auto attDialog = std::make_shared<ValueChangeDialog>(
                screen,                                                    //
                "RF attenuation", "Adjust attenuation:",                   // Cím, felirat
                &config.data.currentAGCgain,                               // Pointer a statikus változóra
                Si4735Constants::SI4735_MIN_ATTENNUATOR, maxGain, 1,       // Min, Max, Step
                [screen](const std::variant<int, float, bool> &newValue) { // Callback a változásra
                    if (std::holds_alternative<int>(newValue)) {

                        DEBUG("Attenuation changed to: %d\n", std::get<int>(newValue));

                        // AGC beállítása
                        ::pSi4735Manager->checkAGC();

                        // StatusLine update
                        if (screen->getStatusLineComp()) {
                            screen->getStatusLineComp()->updateAgc();
                        }
                    }
                },
                nullptr,             // Nincs külön dialog bezárás callback
                Rect(-1, -1, 280, 0) // Auto-magasság
            );
            screen->showDialog(attDialog);

        } else if (event.state == UIButton::EventButtonState::Off) {
            config.data.agcGain = static_cast<uint8_t>(Si4735Runtime::AgcGainMode::Off); // AGC OFF

            // AGC beállítása
            ::pSi4735Manager->checkAGC();

            // StatusLine update
            if (screen->getStatusLineComp()) {
                screen->getStatusLineComp()->updateAgc();
            }
        }
    }

    /**
     * @brief SQUELCH gomb kezelő - ValueChangeDialog megjelenítése
     */
    static void handleSquelchButton(const UIButton::ButtonEvent &event, ScreenRadioBase *screen) {

        if (event.state == UIButton::EventButtonState::On) {
            const char *squelchPrompt = config.data.squelchUsesRSSI ? "RSSI Value[dBuV]:" : "SNR Value[dB]:";

            // ValueChangeDialog létrehozása a statikus változó pointerével
            auto squelchDialog = std::make_shared<ValueChangeDialog>(
                screen,                                                    //
                "Squelch Level", squelchPrompt,                            // Cím, felirat
                &config.data.currentSquelch,                               // Pointer a statikus változóra
                MIN_SQUELCH, MAX_SQUELCH, 1,                               // Min, Max, Step  (rtVars.h-ban definiálva)
                [screen](const std::variant<int, float, bool> &newValue) { // Callback a változásra
                    // if (std::holds_alternative<int>(newValue) == 0) {
                    //     updateButtonStateInScreen(screen, VerticalButtonIDs::SQUELCH, UIButton::ButtonState::Off);
                    // }
                },
                nullptr,             // Nincs külön dialog bezárás callback
                Rect(-1, -1, 280, 0) // Auto-magasság
            );
            screen->showDialog(squelchDialog);
        } else {
            config.data.currentSquelch = 0; // Squelch kikapcsolása
            DEBUG("Squelch disabled\n");
        }
    } /**
       * @brief FREQUENCY gomb kezelő - FrequencyInputDialog megjelenítése
       */
    static void handleFrequencyButton(const UIButton::ButtonEvent &event, ScreenRadioBase *screen) {
        if (event.state != UIButton::EventButtonState::Clicked || !screen) {
            return;
        }

        auto freqDialog = std::make_shared<FrequencyInputDialog>(screen,                           // Szülő képernyő
                                                                 "Frequency Input",                // Cím
                                                                 nullptr,                          // Magyarázó szöveg (nem kötelező)
                                                                 Rect(-1, -1, 250, 220),           // Méret
                                                                 [screen](uint16_t newFrequency) { // Frekvencia változás callback
                                                                     // Elkérjük az aktuális band táblát
                                                                     BandTable &currentBand = ::pSi4735Manager->getCurrentBand();

                                                                     currentBand.currFreq = newFrequency;                              // Beállítjuk a band táblában az új frekit
                                                                     ::pSi4735Manager->getSi4735().setFrequency(currentBand.currFreq); // Ráhangolódunk a rádióval
                                                                     screen->getFreqDisplayComp()->setFrequency(currentBand.currFreq); // A kijelzőt is beállítjuk
                                                                 });

        screen->showDialog(freqDialog);
    }

    /**
     * @brief SETUP gomb kezelő - Képernyőváltás Setup képernyőre
     */
    static void handleSetupButton(const UIButton::ButtonEvent &event, ScreenRadioBase *screen) {
        if (event.state == UIButton::EventButtonState::Clicked && screen) {
            IScreenManager *screenManager = screen->getScreenManager();
            if (!screenManager) {
                DEBUG("ERROR: Could not get screenManager from screen in handleSetupButton!\n");
                return;
            }
            DEBUG("Switching to Setup screen\n");
            screenManager->switchToScreen(SCREEN_NAME_SETUP);
        }
    }

    /**
     * @brief MEMORY gomb kezelő - MemoryScreen képernyőre váltás
     */
    static void handleMemoryButton(const UIButton::ButtonEvent &event, ScreenRadioBase *screen) {
        if (event.state != UIButton::EventButtonState::Clicked || !screen) {
            return;
        }

        DEBUG("Memory screen requested\n");

        // MemoryScreen képernyőre váltás
        auto screenManager = screen->getScreenManager();
        if (!screenManager) {
            DEBUG("ERROR: Could not get screenManager from screen in handleMemoryButton!\n");
            return;
        }

        DEBUG("Switching to Memory screen\n");
        screenManager->switchToScreen(SCREEN_NAME_MEMORY);
    }

    /**
     * @brief Központi gomb definíciók
     */
    static const std::vector<ButtonDefinition> &getButtonDefinitions() {
        static const std::vector<ButtonDefinition> BUTTON_DEFINITIONS = //
            {
                // ID, Label, Type, InitialState, Height, Handler
                {VerticalButtonIDs::MUTE, "Mute", UIButton::ButtonType::Toggleable, UIButton::ButtonState::Off, 32, handleMuteButton},
                {VerticalButtonIDs::VOLUME, "Vol", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off, 32, handleVolumeButton},
                {VerticalButtonIDs::AGC, "AGC", UIButton::ButtonType::Toggleable, UIButton::ButtonState::Off, 32, handleAGCButton},
                {VerticalButtonIDs::ATT, "Att", UIButton::ButtonType::Toggleable, UIButton::ButtonState::Off, 32, handleAttenuatorButton},
                {VerticalButtonIDs::SQUELCH, "Sql", UIButton::ButtonType::Toggleable, UIButton::ButtonState::Off, 32, handleSquelchButton},
                {VerticalButtonIDs::FREQ, "Freq", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off, 32, handleFrequencyButton},
                {VerticalButtonIDs::SETUP, "Setup", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off, 32, handleSetupButton},
                {VerticalButtonIDs::MEMO, "Memo", UIButton::ButtonType::Pushable, UIButton::ButtonState::Off, 32, handleMemoryButton} //
            };
        return BUTTON_DEFINITIONS;
    }

    // =====================================================================
    // FACTORY METÓDUSOK
    // =====================================================================

    /**
     * @brief Maximális gombszélesség kalkuláció
     */
    template <typename TFTType> static uint16_t calculateUniformButtonWidth(uint16_t buttonHeight = 32) {
        uint16_t maxWidth = 0;
        const auto &buttonDefs = getButtonDefinitions();

        for (const auto &def : buttonDefs) {
            uint16_t width = UIButton::calculateWidthForText(def.label, false, buttonHeight);
            maxWidth = std::max(maxWidth, width);
        }
        return maxWidth;
    }

  private:
    /**
     * @brief Belső gombdefiníció létrehozó metódus
     */
    static std::vector<ButtonGroupDefinition> createButtonDefinitionsInternal(IScreenManager *screenManager, ScreenRadioBase *screen, uint16_t buttonWidth) {

        std::vector<ButtonGroupDefinition> definitions;
        const auto &buttonDefs = getButtonDefinitions();
        definitions.reserve(buttonDefs.size());

        for (const auto &def : buttonDefs) {
            std::function<void(const UIButton::ButtonEvent &)> callback;

            if (def.handler != nullptr) {
                // Egységes callback létrehozása - minden handler megkapja a ::pSi4735Manager-t és a Screen-t
                callback = [screen, handler = def.handler](const UIButton::ButtonEvent &e) { handler(e, screen); };
            } else {
                callback = [](const UIButton::ButtonEvent &e) { /* no-op */ };
            }

            definitions.push_back({def.id, def.label, def.type, callback, def.initialState, buttonWidth, def.height});
        }

        return definitions;
    }

  public:
    /**
     * @brief Gombdefiníciók létrehozása automatikus szélességgel
     */
    static std::vector<ButtonGroupDefinition> createButtonDefinitions(ScreenRadioBase *screen) { return createButtonDefinitionsInternal(nullptr, screen, 0); }

    /**
     * @brief Egységes szélességű gombdefiníciók létrehozása
     */
    template <typename TFTType> static std::vector<ButtonGroupDefinition> createUniformButtonDefinitions(ScreenRadioBase *screen) {
        uint16_t uniformWidth = calculateUniformButtonWidth<TFT_eSPI>(32);
        return createButtonDefinitionsInternal(nullptr, screen, uniformWidth);
    }

    // =====================================================================
    // MIXIN TEMPLATE - Screen osztályok kiegészítéséhez
    // =====================================================================

    template <typename ScreenType> class Mixin : public ButtonsGroupManager<ScreenType> {
      protected:
        std::vector<std::shared_ptr<UIButton>> createdVerticalButtons;

        /**
         * @brief Közös függőleges gombok létrehozása egységes szélességgel
         */
        void createCommonVerticalButtons() {
            ScreenType *self = static_cast<ScreenType *>(this);
            auto buttonDefs = CommonVerticalButtons::createUniformButtonDefinitions(self);
            ButtonsGroupManager<ScreenType>::layoutVerticalButtonGroup(buttonDefs, &createdVerticalButtons, 0, 0, 5, 60, 32, 3, 4);
        }

        /**
         *  @brief Egy adott függőleges gomb állapotának frissítése
         *  @param buttonId A gomb azonosítója
         * @param state Az új állapot, amelyet beállítunk
         */
        void updateVerticalButtonState(uint8_t buttonId, UIButton::ButtonState state) {
            for (auto &button : createdVerticalButtons) {
                if (button && button->getId() == buttonId) {
                    button->setButtonState(state);
                    break;
                }
            }
        }

        /**
         * @brief Minden függőleges gomb állapotának frissítése
         * @param ::pSi4735Manager ::pSi4735Manager példány, amely a rádió állapotát kezeli
         */
        void updateAllVerticalButtonStates() {

            updateVerticalButtonState(VerticalButtonIDs::MUTE, rtv::muteStat ? UIButton::ButtonState::On : UIButton::ButtonState::Off);

            bool agcAuto = config.data.agcGain == static_cast<uint8_t>(Si4735Runtime::AgcGainMode::Automatic);
            updateVerticalButtonState(VerticalButtonIDs::AGC, agcAuto ? UIButton::ButtonState::On : UIButton::ButtonState::Off);

            bool attEnabled = config.data.agcGain == static_cast<uint8_t>(Si4735Runtime::AgcGainMode::Manual) && config.data.currentAGCgain > 0;
            updateVerticalButtonState(VerticalButtonIDs::ATT, attEnabled ? UIButton::ButtonState::On : UIButton::ButtonState::Off);

            updateVerticalButtonState(VerticalButtonIDs::SQUELCH, config.data.currentSquelch > 0 ? UIButton::ButtonState::On : UIButton::ButtonState::Off);
        }
    };
};

#endif // __COMMON_VERTICAL_BUTTONS_H