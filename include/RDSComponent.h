#ifndef __RDS_COMPONENT_H
#define __RDS_COMPONENT_H

#include "UIComponent.h"

/**
 * @brief RDS információk megjelenítésére szolgáló UI komponens
 * @details Ez a komponens felelős az RDS (Radio Data System) adatok
 * megjelenítéséért FM rádió vételkor. Megjeleníti az állomásnevet,
 * program típust, radio text üzenetet és dátum/idő információt.
 *
 * Funkciók:
 * - RDS állomásnév megjelenítése
 * - Program típus (PTY) megjelenítése
 * - Radio text görgetése, ha túl hosszú
 * - Dátum és idő megjelenítése
 * - Automatikus frissítés csak változás esetén
 * - Optimalizált rajzolás (markForRedraw rendszer)
 */
class RDSComponent : public UIComponent {
  public:
    // Alapértelmezett méretek és beállítások
    static constexpr uint16_t DEFAULT_HEIGHT = 80;
    static constexpr uint16_t STATION_AREA_HEIGHT = 20;
    static constexpr uint16_t PROGRAM_TYPE_AREA_HEIGHT = 20;
    static constexpr uint16_t RADIO_TEXT_AREA_HEIGHT = 20;
    static constexpr uint16_t DATETIME_AREA_HEIGHT = 20;
    static constexpr uint32_t RDS_UPDATE_INTERVAL_MS = 2000; // RDS frissítési időköz - 2 másodperc
    static constexpr uint32_t SCROLL_INTERVAL_MS = 100;      // Scroll lépések közötti idő
    static constexpr uint8_t SCROLL_STEP_PIXELS = 2;         // Scroll lépés mérete pixelben

  private:
    // ===================================================================
    // PTY (Program Type) tábla
    // ===================================================================

    /**
     * @brief RDS Program Type (PTY) nevek táblája
     * @details Az RDS standard 32 különböző program típust definiál (0-31).
     * Minden PTY kódhoz tartozik egy szöveges leírás.
     */
    static const char *RDS_PTY_NAMES[];
    static const uint8_t RDS_PTY_COUNT;

    /**
     * @brief PTY kód konvertálása szöveges leírássá
     * @param ptyCode A PTY kód (0-31)
     * @return String A PTY szöveges leírása
     */
    String convertPtyCodeToString(uint8_t ptyCode);

    // Időzítés és UI kezelés
    uint32_t lastScrollUpdate;
    bool dataChanged;

    // Layout területek
    Rect stationNameArea;
    Rect programTypeArea;
    Rect radioTextArea;
    Rect dateTimeArea;

    // Radio text scroll kezelés
    TFT_eSprite *scrollSprite;
    int scrollOffset;
    uint16_t radioTextPixelWidth;
    bool needsScrolling;
    bool scrollSpriteCreated;

    // Színek
    uint16_t stationNameColor;
    uint16_t programTypeColor;
    uint16_t radioTextColor;
    uint16_t dateTimeColor;
    uint16_t backgroundColor;

    /**
     * @brief RDS adatok frissítése a Si4735Manager-től
     */
    void updateRdsData();

    /**
     * @brief Állomásnév kirajzolása
     */
    void drawStationName();

    /**
     * @brief Program típus kirajzolása
     */
    void drawProgramType();

    /**
     * @brief Radio text kirajzolása (scroll támogatással)
     */
    void drawRadioText();

    /**
     * @brief Dátum és idő kirajzolása
     */
    void drawDateTime();

    /**
     * @brief Scroll sprite inicializálása
     */
    void initializeScrollSprite();

    /**
     * @brief Scroll sprite felszabadítása
     */
    void cleanupScrollSprite();

    /**
     * @brief Radio text scroll kezelése
     */
    void handleRadioTextScroll();

    /**
     * @brief Radio text feldolgozása - többszörös szóközök kezelése
     * @param radioText A feldolgozandó radio text
     * @return String A feldolgozott radio text
     */
    String processRadioText(const String &radioText);

    /**
     * @brief Alapértelmezett layout számítása
     */
    void calculateDefaultLayout();

  public:
    /**
     * @brief RDSComponent konstruktor
     * @param bounds Komponens határai (opcionális, csak kompatibilitásért)
     */
    RDSComponent(const Rect &bounds = Rect(0, 0, 0, 0));

    /**
     * @brief Destruktor - erőforrások felszabadítása
     */
    virtual ~RDSComponent(); // UIComponent interface implementáció
    virtual void draw() override;
    virtual void markForRedraw(bool markChildren = false);

    /**
     * @brief RDS adatok frissítése (loop-ban hívandó)
     * @details Ellenőrzi az RDS adatok változását és szükség esetén
     * frissíti a megjelenítést. Optimalizált: csak változás esetén rajzol.
     */
    void updateRDS();

    /**
     * @brief RDS adatok törlése
     * @details Törli az összes megjelenített RDS adatot és a cache-t.
     * Használatos amikor nincs RDS vétel vagy AM módra váltunk.
     */
    void clearRDS();

    /**
     * @brief RDS cache törlése frekvencia változáskor
     * @details Azonnal törli az összes RDS adatot és reseteli az időzítőket.
     * Használatos frekvencia váltáskor, amikor az RDS adatok már nem érvényesek.
     */
    void clearRdsOnFrequencyChange();

    /**
     * @brief Ellenőrzi, hogy van-e érvényes RDS adat
     * @return true ha van érvényes RDS vétel
     */
    bool hasValidRDS() const;

    // Layout konfigurálás metódusok

    /**
     * @brief Állomásnév területének beállítása
     * @param area Az állomásnév megjelenítési területe
     */
    void setStationNameArea(const Rect &area);

    /**
     * @brief Program típus területének beállítása
     * @param area A program típus megjelenítési területe
     */
    void setProgramTypeArea(const Rect &area);

    /**
     * @brief Radio text területének beállítása
     * @param area A radio text megjelenítési területe
     */
    void setRadioTextArea(const Rect &area);

    /**
     * @brief Dátum/idő területének beállítása
     * @param area A dátum/idő megjelenítési területe
     */
    void setDateTimeArea(const Rect &area);

    // Színek beállítása

    /**
     * @brief RDS színek testreszabása
     * @param stationColor Állomásnév színe
     * @param typeColor Program típus színe
     * @param textColor Radio text színe
     * @param timeColor Dátum/idő színe
     * @param bgColor Háttérszín
     */
    void setRdsColors(uint16_t stationColor, uint16_t typeColor, uint16_t textColor, uint16_t timeColor, uint16_t bgColor);
};

#endif // __RDS_COMPONENT_H