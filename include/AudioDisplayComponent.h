#ifndef __AUDIO_DISPLAY_COMPONENT_H
#define __AUDIO_DISPLAY_COMPONENT_H

#include "AudioAnalyzer.h"
#include "UIComponent.h"
#include <TFT_eSPI.h>

/**
 * @brief Audio kijelző komponens - Spektrum, oszcilloszkóp, waterfall megjelenítés
 */
class AudioDisplayComponent : public UIComponent {
  public:
    /**
     * @brief Konstruktor
     * @param x X pozíció
     * @param y Y pozíció
     * @param width Szélesség
     * @param height Magasság
     * @param analyzer Audio elemző referencia
     */
    AudioDisplayComponent(int16_t x, int16_t y, int16_t width, int16_t height, AudioAnalyzer &analyzer);

    /**
     * @brief Destruktor
     */
    virtual ~AudioDisplayComponent();

    /**
     * @brief Kirajzolás
     */
    virtual void draw() override;

    /**
     * @brief Touch esemény kezelése
     * @param event Touch esemény
     * @return true ha kezelte az eseményt
     */
    virtual bool handleTouch(const TouchEvent &event) override;

    /**
     * @brief Aktuális mód beállítása
     * @param mode Új kijelzési mód
     */
    void setDisplayMode(AudioDisplayMode mode);

    /**
     * @brief Aktuális mód lekérése
     * @return Jelenlegi kijelzési mód
     */
    AudioDisplayMode getDisplayMode() const { return currentMode; }

    /**
     * @brief Következő módra váltás
     */
    void nextMode();

  private:
    AudioAnalyzer &audioAnalyzer;
    AudioDisplayMode currentMode;
    AudioData lastAudioData;

    // Sprite a villódzásmentes rajzoláshoz
    TFT_eSprite *sprite;
    bool spriteCreated;

    // Mód felirat megjelenítés
    uint32_t modeDisplayStartTime;
    static constexpr uint32_t MODE_DISPLAY_DURATION = 20000; // 20 másodperc

    // Waterfall specifikus adatok
    static constexpr int WATERFALL_LINES = 64;
    uint16_t waterfallData[WATERFALL_LINES][AudioData::SPECTRUM_BINS];
    int waterfallCurrentLine;

    // CW/RTTY hangolássegéd
    struct CWRTTYHelper {
        float targetFreq1; // CW vagy RTTY mark frekvencia
        float targetFreq2; // RTTY space frekvencia (CW esetén 0)
        int centerBin1;    // FFT bin a target freq1-hez
        int centerBin2;    // FFT bin a target freq2-höz (RTTY)
        bool isRTTY;       // true = RTTY mód, false = CW mód
    } cwRttyHelper;

    /**
     * @brief Sprite létrehozása
     */
    void createSprite(TFT_eSPI &tft);

    /**
     * @brief Sprite törlése
     */
    void destroySprite();

    /**
     * @brief Kis felbontású spektrum rajzolása
     */
    void drawLowResSpectrum();

    /**
     * @brief Nagy felbontású spektrum rajzolása
     */
    void drawHighResSpectrum();

    /**
     * @brief Oszcilloszkóp rajzolása
     */
    void drawOscilloscope();

    /**
     * @brief Burkológörbe rajzolása
     */
    void drawEnvelope();

    /**
     * @brief Waterfall rajzolása
     */
    void drawWaterfall();

    /**
     * @brief CW/RTTY hangolássegéd waterfall
     */
    void drawWaterfallCWRTTY();

    /**
     * @brief Mód felirat rajzolása
     */
    void drawModeLabel();

    /**
     * @brief "MUTED" felirat rajzolása
     */
    void drawMutedLabel();

    /**
     * @brief Mód nevének lekérése
     * @param mode Mód
     * @return Mód neve
     */
    const char *getModeString(AudioDisplayMode mode) const;

    /**
     * @brief Frekvencia->FFT bin konverzió
     * @param frequency Frekvencia Hz-ben
     * @return FFT bin index
     */
    int frequencyToBin(float frequency) const;

    /**
     * @brief CW/RTTY helper inicializálása
     */
    void initCWRTTYHelper();

    /**
     * @brief Waterfall adat frissítése
     */
    void updateWaterfallData();

    /**
     * @brief Szín lekérése magnitúdó alapján (waterfall-hoz)
     * @param magnitude Magnitúdó (0-4095)
     * @return RGB565 szín
     */
    uint16_t getMagnitudeColor(uint16_t magnitude) const;
};

#endif // __AUDIO_DISPLAY_COMPONENT_H
