#ifndef __SPECTRUM_ANALYZER_H
#define __SPECTRUM_ANALYZER_H

#include "AudioProcessor.h"
#include "UIColorPalette.h"
#include "UIComponent.h"
#include <memory>

/**
 * @brief Spektrum analizátor komponens - Bar grafikon csúcsérték kitartással
 *
 * @details Valós idejű audio spektrum megjelenítés:
 * - Low-res spektrum adatok használata (16 sáv)
 * - Bar grafikon megjelenítés színátmenettel
 * - Peak hold funkció (csúcsértékek lassú csökkenése)
 * - Automatikus skálázás és dB kijelzés
 */
class SpectrumAnalyzer : public UIComponent {
  public:
    // ===================================================================
    // Konstansok és konfigurációs értékek
    // ===================================================================

    static constexpr uint16_t DEFAULT_WIDTH = 240;  ///< Alapértelmezett szélesség
    static constexpr uint16_t DEFAULT_HEIGHT = 80;  ///< Alapértelmezett magasság
    static constexpr uint16_t MIN_BAR_WIDTH = 2;    ///< Minimális bar szélesség
    static constexpr uint16_t BAR_SPACING = 1;      ///< Bar-ok közötti távolság
    static constexpr float PEAK_DECAY_RATE = 0.92f; ///< Peak csökkenési ráta (per frame)
    static constexpr uint16_t PEAK_HOLD_TIME = 500; ///< Peak tartási idő ms-ben

    // dB skála beállítások
    static constexpr float DB_MIN = -80.0f;            ///< Minimum dB érték
    static constexpr float DB_MAX = 0.0f;              ///< Maximum dB érték
    static constexpr float DB_RANGE = DB_MAX - DB_MIN; ///< dB tartomány

    // ===================================================================
    // Konstruktor és destruktor
    // ===================================================================

    /**
     * @brief SpectrumAnalyzer konstruktor - Spektrum analizátor létrehozása
     * @param bounds Komponens pozíciója és mérete
     */
    SpectrumAnalyzer(const Rect &bounds);

    /**
     * @brief Destruktor - Sprite és erőforrások felszabadítása
     */
    ~SpectrumAnalyzer();

    // ===================================================================
    // UIComponent interface megvalósítás
    // ===================================================================

    /**
     * @brief Komponens rajzolása
     */
    virtual void draw() override;

    /**
     * @brief Komponens frissítése
     */
    void update();

    // ===================================================================
    // Spektrum analizátor specifikus funkciók
    // ===================================================================

    /**
     * @brief Engedélyezi/letiltja a komponenst
     * @param enabled true = engedélyezett, false = letiltott
     */
    void setEnabled(bool enabled);

    /**
     * @brief Engedélyezett állapot lekérdezése
     * @return true ha engedélyezett, false egyébként
     */
    bool isEnabled() const { return enabled; }

    /**
     * @brief Színpaletta beállítása
     * @param lowColor Alacsony szint színe
     * @param midColor Közepes szint színe
     * @param highColor Magas szint színe
     * @param peakColor Csúcsérték színe
     */
    void setColors(uint16_t lowColor, uint16_t midColor, uint16_t highColor, uint16_t peakColor);

  private:
    // ===================================================================
    // Belső adatok és állapot
    // ===================================================================

    bool enabled;            ///< Komponens engedélyezett állapota
    uint32_t lastUpdateTime; ///< Utolsó frissítés időpontja

    // Bar grafikon adatok
    uint16_t barCount;                                            ///< Bar-ok száma
    uint16_t barWidth;                                            ///< Egy bar szélessége
    float currentLevels[AudioProcessorConstants::LOW_RES_BINS];   ///< Aktuális szintek
    float peakLevels[AudioProcessorConstants::LOW_RES_BINS];      ///< Csúcsértékek
    uint32_t peakHoldTime[AudioProcessorConstants::LOW_RES_BINS]; ///< Peak tartási idők

    // Színek
    uint16_t colorLow;        ///< Alacsony szint színe
    uint16_t colorMid;        ///< Közepes szint színe
    uint16_t colorHigh;       ///< Magas szint színe
    uint16_t colorPeak;       ///< Csúcsérték színe
    uint16_t colorBackground; ///< Háttér színe

    // Sprite támogatás villogás ellen
    TFT_eSprite *sprite; ///< Sprite a villogásmentes rajzoláshoz
    bool spriteCreated;  ///< Sprite létrehozva flag

    // ===================================================================
    // Belső segédfüggvények
    // ===================================================================

    /**
     * @brief Bar-ok számításának és méretezésének inicializálása
     */
    void calculateBarLayout();

    /**
     * @brief Sprite létrehozása villogás ellen
     */
    void createSprite();

    /**
     * @brief Sprite törlése
     */
    void deleteSprite();

    /**
     * @brief Egy bar rajzolása sprite-ba
     * @param barIndex Bar indexe
     * @param x Sprite-beli X pozíció
     * @param level Normalizált szint (0.0-1.0)
     * @param peakLevel Normalizált peak szint (0.0-1.0)
     */
    void drawBarToSprite(uint16_t barIndex, uint16_t x, float level, float peakLevel);

    /**
     * @brief dB érték magasságra konvertálása
     * @param dbValue dB érték
     * @return Pixel magasság
     */
    uint16_t dbToHeight(float dbValue) const;

    /**
     * @brief Szín interpoláció szint alapján
     * @param level Normalizált szint (0.0-1.0)
     * @return Interpolált szín
     */
    uint16_t getColorForLevel(float level) const;

    /**
     * @brief Spektrum adatok lekérése az AudioProcessor-ból
     * @return true ha sikerült új adatokat lekérni
     */
    bool updateSpectrumData();

    /**
     * @brief Peak értékek frissítése és csökkenése
     * @param deltaTime Eltelt idő ms-ben
     */
    void updatePeakHold(uint32_t deltaTime);

    /**
     * @brief Egy bar megrajzolása
     * @param display TFT display objektum
     * @param barIndex Bar indexe
     * @param x Bar X pozíciója
     * @param level Normalizált szint (0.0-1.0)
     * @param peakLevel Normalizált peak szint (0.0-1.0)
     */
    void drawBar(TFT_eSPI &display, uint16_t barIndex, uint16_t x, float level, float peakLevel);
};

#endif // __SPECTRUM_ANALYZER_H
