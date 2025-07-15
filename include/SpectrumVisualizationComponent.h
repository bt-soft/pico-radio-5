#ifndef SPECTRUM_VISUALIZATION_COMPONENT_H
#define SPECTRUM_VISUALIZATION_COMPONENT_H

#include <TFT_eSPI.h>
#include <vector>

#include "AudioProcessor.h"
#include "Band.h"
#include "ConfigData.h"
#include "UIComponent.h"

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
     * @brief Módok közötti váltás
     */
    void cycleThroughModes();

    // /**
    //  * @brief Kezdeti mód beállítása
    //  */
    // void setInitialMode(DisplayMode mode);

    /**
     * @brief Mód betöltése config-ból
     */
    void loadModeFromConfig();

    /**
     * @brief Módkijelző láthatóságának beállítása
     */
    void setModeIndicatorVisible(bool visible);

    /**
     * @brief Ellenőrzi, hogy egy megjelenítési mód elérhető-e az aktuális rádió módban
     * @param mode A vizsgálandó megjelenítési mód
     * @return true ha a mód elérhető, false ha nem
     */
    bool isModeAvailable(DisplayMode mode) const;

    /**
     * @brief Beállítja, hogy a keret rajzolása szükséges-e
     * @param drawn true ha a keretet rajzolni kell, false ha nem
     */
    inline void setBorderDrawn() { needBorderDrawn = true; }

    /**
     * @brief Frissíti a maximális megjelenítési frekvenciát
     * @param maxDisplayFrequencyHz Az FFT mintavételezési frekvenciája
     */
    inline void setMaxDisplayFrequencyHz(uint16_t maxDisplayFrequencyHz) {
        maxDisplayFrequencyHz_ = maxDisplayFrequencyHz;
        frequencyLabelsDrawn_ = true;
    }

  private:
    RadioMode radioMode_;
    DisplayMode currentMode_;
    DisplayMode lastRenderedMode_;
    bool modeIndicatorVisible_;
    bool modeIndicatorDrawn_;   // Flag to avoid redrawing the indicator unnecessarily
    bool frequencyLabelsDrawn_; // Flag to avoid redrawing frequency labels unnecessarily
    bool needBorderDrawn;       // Flag to indicate if the border needs to be redrawn
    uint32_t modeIndicatorHideTime_;
    uint32_t lastTouchTime_;
    uint32_t lastFrameTime_; // FPS limitáláshoz
    uint16_t maxDisplayFrequencyHz_;
    float envelopeLastSmoothedValue_;

    // Frame-alapú adaptív autogain rendszer
    static constexpr int FRAME_HISTORY_SIZE = 16; // Több frame átlagolás a stabilabb működésért
    float frameMaxHistory_[FRAME_HISTORY_SIZE];   // Frame maximum értékek története
    int frameHistoryIndex_;                       // Jelenlegi index a circular bufferben
    bool frameHistoryFull_;                       // Már tele van-e a history buffer
    float adaptiveGainFactor_;                    // Frame-alapú adaptív gain faktor
    uint32_t lastGainUpdateTime_;                 // Utolsó gain frissítés ideje

    static constexpr uint32_t GAIN_UPDATE_INTERVAL_MS = 750; // Lassabb frissítés a stabilabb működésért
    static constexpr float TARGET_MAX_UTILIZATION = 0.75f;   // 75%-os maximális kitöltés
    static constexpr float GAIN_SMOOTH_FACTOR = 0.2f;        // Lassabb simítási faktor a stabilabb működésért
    static constexpr float MIN_SIGNAL_THRESHOLD = 0.1f;      // Minimum jel küszöb

    // Sprite handling
    TFT_eSprite *sprite_;
    bool spriteCreated_;
    int indicatorFontHeight_;

    // Peak detection buffer (24 bands max)
    static constexpr int MAX_SPECTRUM_BANDS = 24;
    int Rpeak_[MAX_SPECTRUM_BANDS];

    // CW/RTTY hangolási segéd változók
    TuningAidType currentTuningAidType_;
    uint16_t currentTuningAidMinFreqHz_;
    uint16_t currentTuningAidMaxFreqHz_;

    // Waterfall buffer - egyszerűsített 2D vektor
    std::vector<std::vector<uint8_t>> wabuf;

    /**
     * @brief Sprite kezelő függvények (radio-2 alapján)
     */
    void manageSpriteForMode(DisplayMode modeToPrepareFor);

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
    void renderFrequencyLabels(uint16_t minDisplayFrequencyHz, uint16_t maxDisplayFrequencyHz);

    void startShowModeIndicator();

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
    uint16_t interpolateColor(uint16_t color1, uint16_t color2, float ratio);
    uint16_t valueToWaterfallColor(float val, float min_val, float max_val, byte colorProfileIndex);
    int getGraphHeight() const;
    int getIndicatorHeight() const;
    int getEffectiveHeight() const;

    /**
     * @brief Core1 audio adatok kezelése
     */
    bool getCore1SpectrumData(const double **outData, uint16_t *outSize, float *outBinWidth, float *outAutoGain);
    bool getCore1OscilloscopeData(const int **outData);
    float getCore1BinWidthHz();
    uint16_t getCore1FftSize();

    /**
     * @brief Autogain kezelés
     */
    bool isAutoGainMode();

    /**
     * @brief Frame-alapú adaptív autogain rendszer
     */
    void updateFrameBasedGain(float currentFrameMaxValue);
    float getAdaptiveScale(float baseConstant);
    void resetAdaptiveGain();
    float getCurrentGainFactor() const { return adaptiveGainFactor_; }
    float getAverageFrameMax() const;
    float isMutedDrawn;

    /**
     * @brief Config konverziós függvények
     */
    DisplayMode configValueToDisplayMode(uint8_t configValue);
    uint8_t displayModeToConfigValue(DisplayMode mode);
    void setCurrentModeToConfig();

    /**
     * @brief Kiírja a spektrum terület közepére, hogy "-- Muted --"
     */
    void drawMutedMessage();

    /**
     * @brief Spektrum mód dekódolása szöveggé
     */
    const char *decodeModeToStr();

    /**
     * @brief Optimális FFT méret meghatározása a megjelenítési módhoz
     * @param mode A megjelenítési mód
     * @return Az optimális FFT méret
     */
    uint16_t getOptimalFftSizeForMode(DisplayMode mode) const;

    /**
     * @brief Optimális FFT Mintavételezési frekvencia meghatározása a megjelenítési módhoz
     * @param mode A megjelenítési mód
     * @return Az optimális FFT méret
     */
    uint16_t getOptimalFftSampleFrequencyForAfbandwidth(DisplayMode mode) const;

    /**
     * @brief Core1 FFT paraméterek (frekvencia/méret) beállítása
     */
    void setFftParametersForDisplayMode();
};

#endif // SPECTRUM_VISUALIZATION_COMPONENT_H
