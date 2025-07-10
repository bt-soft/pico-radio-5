#ifndef SPECTRUM_VISUALIZATION_COMPONENT_H
#define SPECTRUM_VISUALIZATION_COMPONENT_H

#include "AudioProcessor.h"
#include "UIComponent.h"
#include <TFT_eSPI.h>
#include <vector>

/**
 * @brief Spektrum vizualizációs komponens - MiniAudioFft funkcionalitás alapján
 *
 * Adaptált a jelenlegi projekt AudioProcessor adatstruktúrájához és UI architektúrájához.
 * A MiniAudioFft etalon megjelenését követi, de a jelenlegi projekt sajátosságaihoz igazítva.
 */
class SpectrumVisualizationComponent : public UIComponent {

  public:
    constexpr static float MAX_DISPLAY_FREQUENCY_AM = 6000.0f;  // Maximális frekvencia AM módban
    constexpr static float MAX_DISPLAY_FREQUENCY_FM = 15000.0f; // Maximális frekvencia FM módban

    /**
     * @brief Megjelenítési módok
     */
    enum class DisplayMode {
        Off,             // Kikapcsolva - csak "Off" szöveg
        SpectrumLowRes,  // Alacsony felbontású spektrum (12 sáv)
        SpectrumHighRes, // Magas felbontású spektrum (pixel szintű)
        Oscilloscope,    // Oszcilloszkóp
        Waterfall,       // Vízesés diagram
        Envelope,        // Burkoló
        CWWaterfall,     // CW hangolássegéd
        RTTYWaterfall    // RTTY hangolássegéd
    };

    enum class RadioMode {
        AM, // AM mód
        FM  // FM mód
    };

    /**
     * @brief Konstruktor
     * @param x X pozíció
     * @param y Y pozíció
     * @param w Szélesség
     * @param h Magasság
     * @param radioMode Működési mód (AM vagy FM)
     */
    SpectrumVisualizationComponent(int x, int y, int w, int h, RadioMode radioMode);

    /**
     * @brief Destruktor
     */
    virtual ~SpectrumVisualizationComponent();

    // UIComponent interface
    virtual void draw() override;
    virtual bool handleTouch(const TouchEvent &event) override;

    /**
     * @brief Kezdeti mód beállítása (pl. config-ból)
     * @param mode Beállítandó DisplayMode
     */
    void setInitialMode(DisplayMode mode);

    /**
     * @brief Teljes újrarajzolás kényszerítése
     */
    void forceRedraw();

    /**
     * @brief Aktuális megjelenítési mód lekérdezése
     */
    DisplayMode getCurrentMode() const { return currentMode; }

    /**
     * @brief Módkijelző láthatóságának beállítása
     * @param visible true = látható, false = rejtett
     */
    void setModeIndicatorVisible(bool visible);

    /**
     * @brief Kényszerített újrarajzolás
     */
    void forceRedrawOnNextFrame();

    /**
     * @brief Config érték alapján DisplayMode konvertálás
     * @param configValue Config-ból származó érték (AudioComponentType)
     * @return Megfelelő DisplayMode
     */
    static DisplayMode configValueToDisplayMode(uint8_t configValue);

    /**
     * @brief DisplayMode alapján config érték konvertálás
     * @param mode DisplayMode
     * @return Config-ba mentendő érték (AudioComponentType)
     */
    static uint8_t displayModeToConfigValue(DisplayMode mode);

    /**
     * @brief Aktuális mód beállítása config-ba
     */
    void setCurrentModeToConfig();

    /**
     * @brief Mód betöltése config-ból
     */
    void loadModeFromConfig();

  private:
    // Vízesés színpaletta - MiniAudioFft-ből
    static const uint16_t WATERFALL_COLORS[16];

    float maxDisplayFrequencyHz; // Maximális megjelenítendő frekvencia

    // Aktuális állapot
    DisplayMode currentMode;
    DisplayMode lastRenderedMode; // Utolsó renderelt mód flicker elkerüléshez
    bool modeIndicatorVisible;
    uint32_t modeIndicatorHideTime; // Mikor tűnjön el a kijelző
    uint32_t lastTouchTime;         // Utolsó érintés időbélyege
    bool needsForceRedraw;          // Kényszerített újrarajzolás

    // Envelope előző érték simításhoz
    float envelopeLastSmoothedValue;

    // Sprite a komplex rajzolásokhoz (waterfall, envelope)
    TFT_eSprite *sprite;
    bool spriteCreated;

    // Font magasság cache a módkijelzőhöz
    int indicatorFontHeight;

    RadioMode radioMode; // Működési mód (AM vagy FM)

    // ===================================================================
    // Belső segédfüggvények
    // ===================================================================

    /**
     * @brief Módok közötti váltás
     */
    void cycleThroughModes();

    /**
     * @brief Aktuális mód szöveges neve
     */
    const char *getModeText() const;

    /**
     * @brief Keret rajzolása
     */
    void drawFrame();

    /**
     * @brief Mode indicator megjelenítése
     */
    void renderModeIndicator();

    /**
     * @brief Muted állapot megjelenítése
     */
    void renderMutedState();

    /**
     * @brief Off mód megjelenítése
     */
    void renderOffMode();

    // ===================================================================
    // Megjelenítési módok specifikus rajzolási függvényei
    // ===================================================================

    /**
     * @brief Alacsony felbontású spektrum rajzolása
     */
    void renderSpectrumLowRes(const SharedAudioData &data);

    /**
     * @brief Magas felbontású spektrum rajzolása
     */
    void renderSpectrumHighRes(const SharedAudioData &data);

    /**
     * @brief Oszcilloszkóp rajzolása
     */
    void renderOscilloscope(const SharedAudioData &data);

    /**
     * @brief Vízesés diagram rajzolása
     */
    void renderWaterfall(const SharedAudioData &data);

    /**
     * @brief Burkoló rajzolása
     */
    void renderEnvelope(const SharedAudioData &data);

    /**
     * @brief CW hangolássegéd rajzolása
     */
    void renderCWWaterfall(const SharedAudioData &data);

    /**
     * @brief RTTY hangolássegéd rajzolása
     */
    void renderRTTYWaterfall(const SharedAudioData &data);

    /**
     * @brief Spektrum szín kiszámítása
     */
    uint16_t getSpectrumColor(float intensity);

    /**
     * @brief Gradient sáv rajzolása
     */
    void drawGradientBar(int x, int y, int w, int h, float intensity);

    /**
     * @brief Gradient sáv rajzolása sprite-ba
     */
    void drawGradientBarToSprite(int x, int y, int w, int h, float intensity);

    /**
     * @brief Színek interpolálása
     */
    uint16_t interpolateColor(uint16_t color1, uint16_t color2, float ratio);

    /**
     * @brief Grafikon magasságának számítása
     * @return Grafikon magasság (módkijelző nélkül)
     */
    int getGraphHeight() const;

    /**
     * @brief Módkijelző területének magassága
     * @return Módkijelző magasság
     */
    int getIndicatorHeight() const;

    /**
     * @brief Effektív magasság számítása
     * @return Teljes magasság (módkijelzővel vagy anélkül)
     */
    int getEffectiveHeight() const;

    /**
     * @brief Sprite létrehozásának biztosítása
     */
    void ensureSpriteCreated();

    /**
     * @brief X pozíció getter
     */
    int getX() const { return bounds.x; }

    /**
     * @brief Y pozíció getter
     */
    int getY() const { return bounds.y; }

    /**
     * @brief Szélesség getter
     */
    int getWidth() const { return bounds.width; }

    /**
     * @brief Magasság getter
     */
    int getHeight() const { return bounds.height; }
};

#endif // SPECTRUM_VISUALIZATION_COMPONENT_H
