#pragma once

#include "CommonVerticalButtons.h"
#include "RDSComponent.h"
#include "ScreenRadioBase.h"
#include "StereoIndicator.h"

/**
 * @brief FM rádió képernyő osztály
 */
class ScreenFM : public ScreenRadioBase, public CommonVerticalButtons::Mixin<ScreenFM> {

  public:
    // ===================================================================
    // Konstruktor és destruktor
    // ===================================================================

    /**
     * @brief ScreenFM konstruktor - FM rádió képernyő inicializálás
     *
     * @details Automatikusan végrehajtja:
     * - Event-driven gombkezelés beállítás
     */
    ScreenFM();

    /**
     * @brief Virtuális destruktor - Automatikus cleanup
     */
    virtual ~ScreenFM();

    // ===================================================================
    // UIScreen interface megvalósítás
    // ===================================================================

    /**
     * @brief Rotary encoder eseménykezelés - FM frekvencia hangolás
     * @param event Rotary encoder esemény (forgatás irány, érték, gombnyomás)
     * @return true ha sikeresen kezelte az eseményt, false egyébként
     *
     * @details Frekvencia hangolási logika:
     * - Rotary forgatás → frekvencia léptetés
     * - Automatikus Si4735 beállítás és band tábla mentés
     * - Frekvencia kijelző azonnali frissítése
     * - Dialógus aktív esetén esemény továbbítása
     */
    virtual bool handleRotary(const RotaryEvent &event) override;

    /**
     * @brief Folyamatos loop hívás - Optimalizált teljesítmény
     * @details Event-driven architektúra - NINCS gombállapot polling!
     *
     * Csak valóban szükséges frissítések:
     * - S-Meter (jelerősség) valós idejű frissítése
     *
     * Gombállapotok frissítése CSAK:
     * - Képernyő aktiválásakor (activate())
     * - Specifikus eseményekkor (eseménykezelőkben)
     */
    virtual void handleOwnLoop() override;
    //
    /**
     * @brief Statikus képernyő tartalom kirajzolása
     * @details Csak a statikus UI elemeket rajzolja:
     * - S-Meter skála (vonalak, számok)
     *
     * A dinamikus tartalom (pl. S-Meter érték) a loop()-ban frissül.
     */
    virtual void drawContent() override;

    /**
     * @brief Képernyő aktiválása - Event-driven gombállapot szinkronizálás
     * @details Ez az EGYETLEN hely, ahol gombállapotokat szinkronizáljuk!
     * Emellett törli az RDS cache-t, ha más sávról jövünk.
     *
     * Szinkronizálási pontok:
     * - Mute gomb <-> rtv::muteStat állapot
     * - AM gomb <-> aktuális band típus
     * - AGC/Attenuator gombok <-> Si4735 állapotok (TODO)
     */
    virtual void activate() override;

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
     * - Függőleges gombsor (jobb oldal)
     * - Vízszintes gombsor (alul)
     */
    void layoutComponents();
    //
    /**
     *@brief Vízszintes gombsor létrehozása - Alsó navigációs gombok
     *@details Közös gombok + FM specifikus gombok(Seek -, Seek +)
     */
    void createHorizontalButtonBar();
    /**
     * @brief Egyedi függőleges gombok létrehozása - Memo gomb override-dal
     * @details Felülírja a CommonVerticalButtons alapértelmezett Memo kezelőjét
     */
    void createCommonVerticalButtons();

    // ===================================================================
    // Event-driven gombállapot szinkronizálás
    // ===================================================================
    /**
     *@brief Vízszintes gombsor állapotainak szinkronizálása
     *@details CSAK aktiváláskor hívódik meg !Event - driven architektúra.
     **Szinkronizált állapotok:
     *-Közös gombok állapotai(Ham, Band, Scan)
     * -FM specifikus gombok állapotai
     */
    void updateHorizontalButtonStates();

    /**
     * @brief FM specifikus gombok hozzáadása a közös gombokhoz
     * @param buttonConfigs A már meglévő gomb konfigurációk vektora
     * @details Felülírja az ős metódusát, hogy hozzáadja az FM specifikus gombokat
     */
    virtual void addSpecificHorizontalButtons(std::vector<UIHorizontalButtonBar::ButtonConfig> &buttonConfigs) override;
    //
    // ===================================================================
    // Vízszintes gomb eseménykezelők
    // ===================================================================
    /**
     *@brief SEEK DOWN gomb eseménykezelő - Automatikus hangolás lefelé
     *@param event Gomb esemény(Clicked)
     * @details Pushable gomb : Automatikus állomáskeresés lefelé
     */
    void handleSeekDownButton(const UIButton::ButtonEvent &event);

    /**
     * @brief SEEK UP gomb eseménykezelő - Automatikus hangolás felfelé
     * @param event Gomb esemény (Clicked)
     * @details Pushable gomb: Automatikus állomáskeresés felfelé
     */
    void handleSeekUpButton(const UIButton::ButtonEvent &event);

    /**
     * @brief Egyedi MEMO gomb eseménykezelő - Intelligens memória kezelés
     * @param event Gomb esemény (Clicked)
     * @details Ha az aktuális állomás még nincs a memóriában és van RDS állomásnév,
     * akkor automatikusan megnyitja a MemoryScreen-t név szerkesztő dialógussal
     */
    void handleMemoButton(const UIButton::ButtonEvent &event);

    // ===================================================================
    // UI komponens objektumok - Smart pointer kezelés
    // ===================================================================

    /**
     * @brief STEREO/MONO jelző komponens
     * @details Piros háttérrel STEREO, kék háttérrel MONO megjelenítés
     */
    std::shared_ptr<StereoIndicator> stereoIndicator;

    // ===================================================================
    // RDS komponens kezelés
    // ===================================================================

    /// RDS (Radio Data System) komponens - FM rádió adatok megjelenítése
    std::shared_ptr<RDSComponent> rdsComponent;

    /**
     * @brief Létrehozza az RDS komponenst
     * @param rdsBounds Az RDS komponens határai (opcionális, most már nem szükséges)
     */
    inline void createRDSComponent(const Rect &rdsBounds = Rect(0, 0, 0, 0)) {
        rdsComponent = std::make_shared<RDSComponent>(rdsBounds);
        addChild(rdsComponent);
    }

    /**
     * @brief RDS cache törlése frekvencia változáskor
     * @details Biztonságos RDS cache törlés null pointer ellenőrzéssel
     */
    inline void clearRDSCache() {
        if (rdsComponent) {
            rdsComponent->clearRdsOnFrequencyChange();
        }
    }
};
