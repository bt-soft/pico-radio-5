#ifndef __SCREEN_AM_H
#define __SCREEN_AM_H

#include "CommonVerticalButtons.h"
#include "CwDecoder.h" // Új include
#include "ScreenRadioBase.h"
#include "UITextBox.h" // Új include

class ScreenAM : public ScreenRadioBase, public CommonVerticalButtons::Mixin<ScreenAM> {

  public:
    static bool audioDecoderRun;
    static ScreenAM *that;

    // ===================================================================
    // Konstruktor és destruktor
    // ===================================================================

    /**
     * @brief ScreenAM konstruktor - AM rádió képernyő inicializálás
     */
    ScreenAM();

    /**
     * @brief Virtuális destruktor - Automatikus cleanup
     */
    virtual ~ScreenAM();

    // ===================================================================
    // UIScreen interface megvalósítás
    // ===================================================================

    /**
     * @brief Rotary encoder eseménykezelés - AM frekvencia hangolás
     * @param event Rotary encoder esemény (forgatás irány, érték, gombnyomás)
     * @return true ha sikeresen kezelte az eseményt, false egyébként
     *
     * @details AM frekvencia hangolási logika:
     * - Rotary forgatás → frekvencia léptetés (AM band-nek megfelelően)
     * - Automatikus Si4735 beállítás és band tábla mentés
     * - Frekvencia kijelző azonnali frissítése
     * - Dialógus aktív esetén esemény továbbítása
     * - MW: 9/10 kHz lépések, LW: 1 kHz, SW: 5 kHz lépések
     */
    virtual bool handleRotary(const RotaryEvent &event) override;

    /**
     * @brief Folyamatos loop hívás - Optimalizált teljesítmény
     * @details Event-driven architektúra - NINCS gombállapot polling!
     *
     * Csak valóban szükséges frissítések:
     * - S-Meter (jelerősség) valós idejű frissítése AM módban
     *
     * Gombállapotok frissítése CSAK:
     * - Képernyő aktiválásakor (activate())
     * - Specifikus eseményekkor (eseménykezelőkben)
     */
    virtual void handleOwnLoop() override;

    /**
     * @brief Statikus képernyő tartalom kirajzolása
     * @details Csak a statikus UI elemeket rajzolja:
     * - S-Meter skála (vonalak, számok) AM módban
     *
     * A dinamikus tartalom (pl. S-Meter érték) a loop()-ban frissül.
     */
    virtual void drawContent() override;

    /**
     * @brief Képernyő aktiválása - Event-driven gombállapot szinkronizálás
     * @details Ez az EGYETLEN hely, ahol gombállapotokat szinkronizálunk!
     *
     * Szinkronizálási pontok:
     * - Mute gomb ↔ rtv::muteStat állapot
     * - FM gomb ↔ aktuális band típus (AM vs FM)
     * - AGC/Attenuator gombok ↔ Si4735 állapotok (TODO)
     * - Bandwidth gomb ↔ AM szűrő beállítások
     */
    virtual void activate() override;

    /**
     * @brief Képernyő deaktiválása - Cleanup és állapotok visszaállítása
     *
     */
    virtual void deactivate() override;

    static void processAudioDecoder();

    /**
     * @brief Dialógus bezárásának kezelése - Gombállapot szinkronizálás
     * @details Az utolsó dialógus bezárásakor frissíti a gombállapotokat
     *
     * Funkcionalitás:
     * - Alap UIScreen::onDialogClosed() hívása
     * - Ha ez volt az utolsó dialógus -> updateAllVerticalButtonStates() + updateHorizontalButtonStates()
     * - Biztosítja a konzisztens gombállapotokat dialógus bezárás után
     */
    virtual void onDialogClosed(UIDialogBase *closedDialog) override;

  private:
    // ===================================================================
    // UI komponensek layout és management
    // ===================================================================

    /**
     * @brief UI komponensek létrehozása és képernyőn való elhelyezése
     * @details Létrehozza és pozicionálja az összes UI elemet:
     * - Állapotsor (felül)
     * - Frekvencia kijelző (középen)
     * - S-Meter (jelerősség mérő)
     * - Függőleges gombsor (jobb oldal) - Közös FMScreen-nel
     * - Vízszintes gombsor (alul) - FM gombbal
     */
    void layoutComponents();

    /**
     * @brief Frissíti a FreqDisplay szélességét az aktuális band típus alapján
     * @details Dinamikusan állítja be a frekvencia kijelző szélességét
     */
    void updateFreqDisplayWidth();

    /**
     * @brief AM specifikus gombok hozzáadása a közös gombokhoz
     * @param buttonConfigs A már meglévő gomb konfigurációk vektora
     * @details Felülírja az ős metódusát, hogy hozzáadja az AM specifikus gombokat
     */
    virtual void addSpecificHorizontalButtons(std::vector<UIHorizontalButtonBar::ButtonConfig> &buttonConfigs) override;

    // ===================================================================
    // Event-driven gombállapot szinkronizálás
    // ===================================================================

    /**
     * @brief AM specifikus vízszintes gombsor állapotainak szinkronizálása
     * @details CSAK aktiváláskor hívódik meg! Event-driven architektúra.
     *
     * Szinkronizált állapotok:
     * - AM specifikus gombok alapértelmezett állapotai
     */
    void updateHorizontalButtonStates();

    /**
     * @brief Step gomb állapotának frissítése
     * @details SSB/CW módban csak akkor engedélyezett, ha BFO be van kapcsolva
     */
    void updateStepButtonState();

    /**
     * @brief BFO gomb állapotának frissítése
     * @details Csak SSB/CW módban engedélyezett
     */
    void updateBFOButtonState();

    // ===================================================================
    // AM specifikus gomb eseménykezelők
    // ===================================================================

    /**
     * @brief BFO gomb eseménykezelő - Beat Frequency Oscillator
     * @param event Gomb esemény (Clicked)
     * @details AM specifikus funkcionalitás
     */
    void handleBFOButton(const UIButton::ButtonEvent &event);

    /**
     * @brief AfBW gomb eseménykezelő - Audio Filter Bandwidth
     * @param event Gomb esemény (Clicked)
     * @details AM specifikus funkcionalitás
     */
    void handleAfBWButton(const UIButton::ButtonEvent &event);

    /**
     * @brief AntCap gomb eseménykezelő - Antenna Capacitor
     * @param event Gomb esemény (Clicked)
     * @details AM specifikus funkcionalitás
     */
    void handleAntCapButton(const UIButton::ButtonEvent &event);

    /**
     * @brief Demod gomb eseménykezelő - Demodulation
     * @param event Gomb esemény (Clicked)
     * @details AM specifikus funkcionalitás
     */
    void handleDemodButton(const UIButton::ButtonEvent &event);

    /**
     * @brief Step gomb eseménykezelő - Frequency Step
     * @param event Gomb esemény (Clicked)
     * @details AM specifikus funkcionalitás
     */
    void handleStepButton(const UIButton::ButtonEvent &event);

  private:
    // ===================================================================
    // AM specifikus tagváltozók
    // ===================================================================
    std::shared_ptr<CwDecoder> cwDecoder;
    std::shared_ptr<UITextBox> decodedTextBox;
    SpectrumVisualizationComponent::DisplayMode lastSpectrumMode_ = SpectrumVisualizationComponent::DisplayMode::Off;
};

#endif // __SCREEN_AM_H