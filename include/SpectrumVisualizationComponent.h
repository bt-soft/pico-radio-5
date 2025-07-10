#ifndef SPECTRUM_VISUALIZATION_COMPONENT_H
#define SPECTRUM_VISUALIZATION_COMPONENT_H

#include "AudioProcessor.h"
#include "Band.h"
#include "ConfigData.h"
#include "UIComponent.h"
#include <TFT_eSPI.h>
#include <vector>

/**
 * @brief Rádió módok
 */
enum class RadioMode { AM = 0, FM = 1 };

/**
 * @brief Spektrum vizualizáció komponens a radio-2 projekt alapján
 */
class SpectrumVisualizationComponent : public UIComponent {
  public:
    /**
     * @brief Megjelenítési módok
     */
    enum class DisplayMode { Off = 0, SpectrumLowRes = 1, SpectrumHighRes = 2, Oscilloscope = 3, Envelope = 4, Waterfall = 5, CWWaterfall = 6, RTTYWaterfall = 7 };

    /**
     * @brief Hangolási segéd típusok (CW/RTTY)
     */
    enum class TuningAidType : uint8_t {
        CW_TUNING,
        RTTY_TUNING,
        OFF_DECODER // A fő dekóder ki van kapcsolva
    };

    /**
     * @brief Konstansok
     */
    static constexpr float MAX_DISPLAY_FREQUENCY_AM = 6000.0f;  // AM max frekvencia
    static constexpr float MAX_DISPLAY_FREQUENCY_FM = 15000.0f; // FM max frekvencia

    /**
     * @brief Waterfall színpaletta
     */
    static const uint16_t WATERFALL_COLORS[16];

    /**
     * @brief Konstruktor
     * @param x X pozíció
     * @param y Y pozíció
     * @param w Szélesség
     * @param h Magasság
     * @param radioMode Rádió mód (AM/FM)
     */
    SpectrumVisualizationComponent(int x, int y, int w, int h, RadioMode radioMode);

    /**
     * @brief Destruktor
     */
    ~SpectrumVisualizationComponent();

    // UIComponent interface
    void draw() override;
    bool handleTouch(const TouchEvent &touch) override;

    /**
     * @brief Keret rajzolása
     */
    void drawFrame();

    /**
     * @brief Teljes újrarajzolás kényszerítése
     */
    void forceRedraw();

    /**
     * @brief Módok közötti váltás
     */
    void cycleThroughModes();

    /**
     * @brief Aktuális mód szöveges neve
     */
    const char *getModeText() const;

    /**
     * @brief Kezdeti mód beállítása
     */
    void setInitialMode(DisplayMode mode);

    /**
     * @brief Mód betöltése config-ból
     */
    void loadModeFromConfig();

    /**
     * @brief Módkijelző láthatóságának beállítása
     */
    void setModeIndicatorVisible(bool visible);

  private:
    RadioMode radioMode_;
    DisplayMode currentMode_;
    DisplayMode lastRenderedMode_;
    bool needsForceRedraw_;
    bool modeIndicatorVisible_;
    uint32_t modeIndicatorHideTime_;
    uint32_t lastTouchTime_;
    float maxDisplayFrequencyHz_;
    float envelopeLastSmoothedValue_;

    // Sprite handling
    TFT_eSprite *sprite_;
    bool spriteCreated_;
    int indicatorFontHeight_;

    // Radio-2 alapú változók
    int currentYAnalyzer_;
    AudioProcessor *pAudioProcessor_;

    // Peak detection buffer (24 bands max)
    static constexpr int MAX_SPECTRUM_BANDS = 24;
    int Rpeak_[MAX_SPECTRUM_BANDS];

    // CW/RTTY hangolási segéd változók
    TuningAidType currentTuningAidType_;
    float currentTuningAidMinFreqHz_;
    float currentTuningAidMaxFreqHz_;

    // Waterfall buffer - egyszerűsített 2D vektor
    std::vector<std::vector<uint8_t>> wabuf;

    /**
     * @brief Sprite kezelő függvények (radio-2 alapján)
     */
    void manageSpriteForMode(DisplayMode modeToPrepareFor);
    void ensureSpriteCreated();

    /**
     * @brief Renderelő függvények - radio-2 alapján
     */
    void renderOffMode();
    void renderSpectrumLowRes();
    void renderSpectrumHighRes();
    void renderOscilloscope();
    void renderWaterfall();
    void renderEnvelope();
    void renderModeIndicator();

    /**
     * @brief Spectrum bar függvények (radio-2 alapján)
     */
    uint8_t getBandVal(int fft_bin_index, int min_bin_low_res, int num_bins_low_res_range, int total_bands);
    void drawSpectrumBar(int band_idx, double magnitude, int actual_start_x_on_screen, int peak_max_height_for_mode, int current_bar_width_pixels);

    /**
     * @brief CW/RTTY hangolási segéd függvények
     */
    void setTuningAidType(TuningAidType type);
    void renderCWWaterfall();
    void renderRTTYWaterfall();
    void renderTuningAid();

    /**
     * @brief Segéd függvények
     */
    uint16_t getSpectrumColor(float intensity);
    void drawGradientBar(int x, int y, int w, int h, float intensity);
    void drawGradientBarToSprite(int x, int y, int w, int h, float intensity);
    uint16_t interpolateColor(uint16_t color1, uint16_t color2, float ratio);
    uint16_t valueToWaterfallColor(float val, float min_val, float max_val, byte colorProfileIndex);
    int getGraphHeight() const;
    int getIndicatorHeight() const;
    int getEffectiveHeight() const;

    /**
     * @brief Config konverziós függvények
     */
    DisplayMode configValueToDisplayMode(uint8_t configValue);
    uint8_t displayModeToConfigValue(DisplayMode mode);
    void setCurrentModeToConfig();
};

#endif // SPECTRUM_VISUALIZATION_COMPONENT_H
