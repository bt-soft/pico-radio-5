#ifndef __SCREEN_RADIO_BASE_H
#define __SCREEN_RADIO_BASE_H

#include "SMeter.h"
#include "ScreenFrequDisplayBase.h"
#include "SpectrumVisualizationComponent.h"
#include "StatusLine.h"
#include "UIHorizontalButtonBar.h"

// Forward deklaráció a seek callback függvényhez
void radioSeekProgressCallback(uint16_t frequency);

/**
 * @brief Közös vízszintes gombsor gomb azonosítók
 * @details Minden RadioScreen alapú képernyő közös gombjai
 */
namespace CommonHorizontalButtonIDs {
static constexpr uint8_t HAM_BUTTON = 50;  ///< Ham rádió funkcionalitás
static constexpr uint8_t BAND_BUTTON = 51; ///< Band (sáv) kezelés
static constexpr uint8_t SCAN_BUTTON = 52; ///< Scan (folyamatos keresés)
} // namespace CommonHorizontalButtonIDs

/**
 * @class RadioScreen
 * @brief Rádió vezérlő képernyők közös alaposztálya
 * @details Ez az absztrakciós réteg az UIScreen és a konkrét rádiós képernyők között.
 *
 * **Fő funkciók:**
 * - FrequDisplay (frekvencia kijelző) és akkumulátor állapot kijelző
 * - Seek (automatikus állomáskeresés) valós idejű frissítéssel
 * - Frekvencia és band kezelés
 * - Közös vízszintes gombsor (HAM, BAND, SCAN) kezelése
 * - S-Meter (jelerősség mérő) komponens integrációja
 *
 *
 * **Örökölhető osztályok:**
 * - ScreenFM - FM rádió vezérlés
 * - ScreenAM - AM/MW/LW/SW rádió vezérlés
 * - ScreenScreenSaver - Képernyőkímélő funkció
 *
 */
class ScreenRadioBase : public ScreenFrequDisplayBase {

    // Friend deklaráció a seek callback számára
    friend void radioSeekProgressCallback(uint16_t frequency);

  public:
    // ===================================================================
    // Konstruktor és destruktor
    // ===================================================================

    /**
     * @brief RadioScreen konstruktor - Rádió képernyő alaposztály inicializálás
     * @param name Képernyő egyedi neve
     */
    ScreenRadioBase(const char *name);

    /**
     * @brief Virtuális destruktor - Automatikus cleanup
     */
    virtual ~ScreenRadioBase();

    /**
     * @brief StatusLine komponens lekérése
     */
    inline std::shared_ptr<StatusLine> getStatusLineComp() const { return statusLineComp; }

  protected:
    // ===================================================================
    // UI komponensek factory metódusok
    // ===================================================================

    // Állapotsor komponens
    std::shared_ptr<StatusLine> statusLineComp;

    /**
     * @brief Létrehozza az állapotsor komponenst
     * A metódust az a képernyő hívja meg, aki akar ilyen komponenst
     */
    inline void createStatusLine() {
        // StatusLine komponens létrehozása - bal felső sarok (0,0)
        statusLineComp = std::make_shared<StatusLine>(0, 0);
        addChild(statusLineComp);
    }

    // ===================================================================
    // Közös vízszintes gombsor kezelés
    // ===================================================================

    // Közös vízszintes gombsor komponens (alsó navigációs gombok)
    std::shared_ptr<UIHorizontalButtonBar> horizontalButtonBar;

    /**
     * @brief Közös vízszintes gombsor létrehozása és inicializálása
     * @details Létrehozza a közös gombokat, amiket minden RadioScreen használ
     * A leszármazott osztályok ezt kiterjeszthetik saját specifikus gombokkal
     */
    virtual void createCommonHorizontalButtons();

    /**
     * @brief Közös vízszintes gombsor állapotainak szinkronizálása
     * @details Csak aktiváláskor hívódik meg! Event-driven architektúra.
     * A leszármazott osztályok felülírhatják saját specifikus állapotokkal
     */
    virtual void updateCommonHorizontalButtonStates();

    // ===================================================================
    // UIScreen interface override
    // ===================================================================

    /**
     * @brief RadioScreen aktiválása - Signal quality cache invalidálás
     * @details Minden RadioScreen aktiváláskor invalidálja a signal quality cache-t,
     * hogy az S-meter azonnal frissüljön. A leszármazott osztályok meghívhatják
     * ezt a szülő implementációt, majd hozzáadhatják saját aktiválási logikájukat.
     */
    virtual void activate() override;

    /**
     * @brief Lehetőség a leszármazott osztályoknak további gombok hozzáadására
     * @param buttonConfigs A már meglévő gomb konfigurációk vektora
     * @details A leszármazott osztályok felülírhatják ezt a metódust, hogy
     * hozzáadhassanak specifikus gombokat a közös gombokhoz
     */
    virtual void addSpecificHorizontalButtons(std::vector<UIHorizontalButtonBar::ButtonConfig> &buttonConfigs) {}

    /**
     * @brief Dialógus bezárásának kezelése
     * @param closedDialog A bezárt dialógus referencia
     */
    virtual void onDialogClosed(UIDialogBase *closedDialog) override;

    // ===================================================================
    // Közös gomb eseménykezelők
    // ===================================================================

    /**
     * @brief HAM gomb eseménykezelő - Ham rádió funkcionalitás
     * @param event Gomb esemény (Clicked)
     * @details Virtuális függvény - leszármazott osztályok felülírhatják
     */
    virtual void handleHamButton(const UIButton::ButtonEvent &event);

    /**
     * @brief BAND gomb eseménykezelő - Sáv (Band) kezelés
     * @param event Gomb esemény (Clicked)
     * @details Virtuális függvény - leszármazott osztályok felülírhatják
     */
    virtual void handleBandButton(const UIButton::ButtonEvent &event);

    /**
     * @brief Közös BAND gomb eseménykezelő - Sáv (Band) kezelés
     * @param isHamBand Igaz, ha a Ham sávot kell kezelni, hamis, ha más sáv
     * @details Ez a metódus a közös BAND gomb eseménykezelés logikáját tartalmazza.
     */
    void processBandButton(bool isHamBand);

    /**
     * @brief SCAN gomb eseménykezelő - Folyamatos keresés
     * @param event Gomb esemény (Clicked)
     * @details Virtuális függvény - leszármazott osztályok felülírhatják
     */
    virtual void handleScanButton(const UIButton::ButtonEvent &event);

    // ===================================================================
    // S-Meter (jelerősség mérő) komponens kezelés
    // ===================================================================

    /// S-Meter komponens - jelerősség és jel minőség megjelenítése
    std::shared_ptr<SMeter> smeterComp;

    /**
     * @brief Létrehozza az S-Meter komponenst
     * @param smeterBounds Az S-Meter komponens határai
     */
    inline void createSMeterComponent(const Rect &smeterBounds) {
        ColorScheme smeterColors = ColorScheme::defaultScheme();
        smeterColors.background = TFT_COLOR_BACKGROUND; // Fekete háttér a designhoz
        smeterComp = std::make_shared<SMeter>(smeterBounds, smeterColors);
        addChild(smeterComp);
    }

    /**
     * @brief S-Meter frissítése optimalizált időzítéssel
     * @param isFMMode true = FM mód, false = AM mód
     * @details 250ms-es intervallummal frissíti az S-meter-t (4 Hz)
     * Belső változás detektálással - csak szükség esetén rajzol újra
     */
    void updateSMeter(bool isFMMode);

    // ===================================================================
    // Spektrum vizualizáció komponens kezelés
    // ===================================================================

    /// Spektrum vizualizáció komponens - audio spektrum és oszcilloszkóp
    std::shared_ptr<SpectrumVisualizationComponent> spectrumComp;

    /**
     * @brief Létrehozza a spektrum vizualizáció komponenst
     * @param spectrumBounds A spektrum komponens határai
     * @param radioMode Rádió mód (AM/FM)
     */
    inline void createSpectrumComponent(const Rect &spectrumBounds, RadioMode radioMode) {
        spectrumComp = std::make_shared<SpectrumVisualizationComponent>(spectrumBounds.x, spectrumBounds.y, spectrumBounds.width, spectrumBounds.height, radioMode);
        spectrumComp->loadModeFromConfig(); // AM/FM mód betöltése config-ból
        addChild(spectrumComp);
    }

    // ===================================================================
    // Seek (automatikus állomáskeresés) infrastruktúra
    // ===================================================================

    /**
     * @brief Seek keresés indítása lefelé valós idejű frekvencia frissítéssel
     * @details Beállítja a callback infrastruktúrát és indítja a seek-et
     *
     * Művelet:
     * 1. Callback infrastruktúra beállítása
     * 2. SI4735 seekStationProgress hívás SEEK_DOWN irányban
     * 3. Valós idejű frekvencia frissítés a callback-en keresztül
     * 4. Konfiguráció és band tábla frissítése
     */
    void seekStationDown();

    /**
     * @brief Seek keresés indítása felfelé valós idejű frekvencia frissítéssel
     * @details Beállítja a callback infrastruktúrát és indítja a seek-et
     *
     * Művelet:
     * 1. Callback infrastruktúra beállítása
     * 2. SI4735 seekStationProgress hívás SEEK_UP irányban
     * 3. Valós idejű frekvencia frissítés a callback-en keresztül
     * 4. Konfiguráció és band tábla frissítése
     */
    void seekStationUp();

    // ===================================================================
    // Rádió-specifikus utility metódusok
    // ===================================================================

    /**
     * @brief Frekvencia mentése a konfigurációba és band táblába
     * @details Szinkronizálja az aktuális frekvenciát minden szükséges helyre
     */
    void saveCurrentFrequency();

    /**
     * @brief Ellenőrzi, hogy az aktuális frekvencia benne van-e a memóriában
     * @return true ha a frekvencia elmentett állomás, false egyébként
     */
    bool checkCurrentFrequencyInMemory() const;

    /**
     * @brief Ellenőrzi, hogy az aktuális frekvencia benne van-e a memóriában
     * @return true ha a frekvencia elmentett állomás, false egyébként
     *
     * @details Ellenőrzi az aktuális frekvenciát a StationStore memóriában.
     * Ha talál egyezést, frissíti a StatusLine státuszát is.
     *
     * Használat frekvencia változáskor (rotary encoder, seek):
     * - Meghívja a StationStore keresést
     * - Frissíti a StatusLine::updateStationInMemory státuszt
     */
    bool checkAndUpdateMemoryStatus();

    /**
     * @brief A kijelző explicit frissítése
     * @details Frissíti a kijelző komponenseit (pl.: ha nem váltunk képernyőt, akkor csak ez marad)
     * Hasznos band váltás után, amikor ugyanaz a screen marad aktív
     */
    void refreshScreenComponents();

    /**
     * @brief Beállítja az audio spektrum és az audio processzor hangfrekvenciás sávszélességét
     */
    void setFftSamplingFrequencyAndSpektrumMaxDisplayFrequency();

  private:
    // ===================================================================
    // Dialog cleanup helper methods
    // ===================================================================
    /**
     *@brief Band váltás kezelése dialog bezárás után
     *@param dialog A bezárandó dialógus
     *@details Egyszerű metódus, ami elvégzi a dialog cleanup - ot és a szükséges képernyőváltást / refresh - t
     */
    void handleBandSwitchAfterDialog(UIDialogBase *dialog);

    /// Flag annak jelzésére, hogy az utolsó dialógus band dialógus volt-e
    bool lastDialogWasBandDialog = false;
};

#endif // __SCREEN_RADIO_BASE_H
