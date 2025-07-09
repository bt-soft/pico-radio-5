#include "SpectrumVisualizationComponent.h"
#include "Config.h"
#include "pico/mutex.h"
#include "rtVars.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>

// Waterfall színpaletta - MiniAudioFft-ből átvéve
const uint16_t SpectrumVisualizationComponent::WATERFALL_COLORS[16] = {
    0x0000,                        // TFT_BLACK (index 0)
    0x0000,                        // TFT_BLACK (index 1)
    0x0000,                        // TFT_BLACK (index 2)
    0x001F,                        // Nagyon sötét kék
    0x081F,                        // Sötét kék
    0x0810,                        // Sötét zöldeskék
    0x0800,                        // Sötétzöld
    0x0C00,                        // Közepes zöld
    0x1C00,                        // Világosabb zöld
    0xFC00,                        // Narancs
    0xFDE0,                        // Világos sárga
    0xFFE0,                        // Sárga
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF // Fehér a csúcsokhoz
};

/**
 * @brief Konstruktor
 */
SpectrumVisualizationComponent::SpectrumVisualizationComponent(int x, int y, int w, int h, float maxDisplayFreq)
    : UIComponent(Rect(x, y, w, h)), maxDisplayFrequencyHz(maxDisplayFreq), currentMode(DisplayMode::Off), modeIndicatorVisible(true), modeIndicatorHideTime(0), lastTouchTime(0), envelopeLastSmoothedValue(0.0f),
      sprite(nullptr), spriteCreated(false), indicatorFontHeight(0) {

    // Font magasság meghatározása
    ::tft.setFreeFont();
    ::tft.setTextSize(1);
    indicatorFontHeight = ::tft.fontHeight();

    DEBUG("SpectrumVisualization: Komponens létrehozva %dx%d, max freq: %.0f Hz\n", w, h, maxDisplayFreq);
}

/**
 * @brief Destruktor
 */
SpectrumVisualizationComponent::~SpectrumVisualizationComponent() {
    if (sprite && spriteCreated) {
        sprite->deleteSprite();
        delete sprite;
    }
}

/**
 * @brief Komponens megjelenítése (UIComponent interface)
 */
void SpectrumVisualizationComponent::draw() {
    if (currentMode == DisplayMode::Off) {
        renderOffMode();
        return;
    }

    // Mutex védelem alatt adatok olvasása
    SharedAudioData data;
    bool success = false;

    // Mutex lock és adat másolás
    if (mutex_try_enter(&g_sharedAudioData.dataMutex, nullptr)) {
        data = g_sharedAudioData;
        success = true;
        mutex_exit(&g_sharedAudioData.dataMutex);
    }

    if (!success) {
        // Nincs adat vagy mutex hiba - muted állapot megjelenítése
        renderMutedState();
        return;
    }

    switch (currentMode) {
        case DisplayMode::SpectrumLowRes:
            renderSpectrumLowRes(data);
            break;
        case DisplayMode::SpectrumHighRes:
            renderSpectrumHighRes(data);
            break;
        case DisplayMode::Oscilloscope:
            renderOscilloscope(data);
            break;
        case DisplayMode::Waterfall:
            renderWaterfall(data);
            break;
        case DisplayMode::Envelope:
            renderEnvelope(data);
            break;
        case DisplayMode::CWWaterfall:
            renderCWWaterfall(data);
            break;
        case DisplayMode::RTTYWaterfall:
            renderRTTYWaterfall(data);
            break;
        default:
            break;
    }

    // Mode indicator megjelenítése ha szükséges
    if (modeIndicatorVisible && millis() < modeIndicatorHideTime) {
        renderModeIndicator();
    }
}

/**
 * @brief Touch esemény kezelése
 */
bool SpectrumVisualizationComponent::handleTouch(const TouchEvent &touch) {
    if (!bounds.contains(touch.x, touch.y)) {
        return false;
    }

    // Debounce - minimum 200ms várakozás az utolsó érintés óta
    unsigned long currentTime = millis();
    if (currentTime - lastTouchTime < 200) {
        return true; // Elnyeljük az eseményt, de nem változtatunk módot
    }

    lastTouchTime = currentTime;

    // Következő módra váltás
    cycleThroughModes();

    // Mode indicator megjelenítése 2 másodpercig
    modeIndicatorVisible = true;
    modeIndicatorHideTime = currentTime + 2000;

    DEBUG("SpectrumVisualization: Touch detected, new mode: %d\n", static_cast<int>(currentMode));
    return true;
}

/**
 * @brief Módok közötti váltás
 */
void SpectrumVisualizationComponent::cycleThroughModes() {
    switch (currentMode) {
        case DisplayMode::Off:
            currentMode = DisplayMode::SpectrumLowRes;
            break;
        case DisplayMode::SpectrumLowRes:
            currentMode = DisplayMode::SpectrumHighRes;
            break;
        case DisplayMode::SpectrumHighRes:
            currentMode = DisplayMode::Oscilloscope;
            break;
        case DisplayMode::Oscilloscope:
            currentMode = DisplayMode::Waterfall;
            break;
        case DisplayMode::Waterfall:
            currentMode = DisplayMode::Envelope;
            break;
        case DisplayMode::Envelope:
            currentMode = DisplayMode::CWWaterfall;
            break;
        case DisplayMode::CWWaterfall:
            currentMode = DisplayMode::RTTYWaterfall;
            break;
        case DisplayMode::RTTYWaterfall:
            currentMode = DisplayMode::Off;
            break;
    }
}

/**
 * @brief Aktuális mód szöveges neve
 */
const char *SpectrumVisualizationComponent::getModeText() const {
    switch (currentMode) {
        case DisplayMode::Off:
            return "Off";
        case DisplayMode::SpectrumLowRes:
            return "Spectrum Low";
        case DisplayMode::SpectrumHighRes:
            return "Spectrum High";
        case DisplayMode::Oscilloscope:
            return "Oscilloscope";
        case DisplayMode::Waterfall:
            return "Waterfall";
        case DisplayMode::Envelope:
            return "Envelope";
        case DisplayMode::CWWaterfall:
            return "CW Tuning";
        case DisplayMode::RTTYWaterfall:
            return "RTTY Tuning";
        default:
            return "Unknown";
    }
}

/**
 * @brief Keret rajzolása
 */
void SpectrumVisualizationComponent::drawFrame() {
    const int frameThickness = 1;
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;

    // Maximális keret területe (tisztítás)
    int maxOuterX = x - frameThickness;
    int maxOuterY = y - frameThickness;
    int maxOuterW = width + (frameThickness * 2);
    int maxOuterH = height + (frameThickness * 2);
    ::tft.fillRect(maxOuterX, maxOuterY, maxOuterW, maxOuterH, TFT_BLACK);

    // Aktuális keret rajzolása
    int currentFrameX = x - frameThickness;
    int currentFrameY = y - frameThickness;
    int currentFrameW = width + (frameThickness * 2);
    int currentFrameH = height + (frameThickness * 2);
    ::tft.drawRect(currentFrameX, currentFrameY, currentFrameW, currentFrameH, TFT_COLOR(80, 80, 80));
}

/**
 * @brief Mode indicator megjelenítése
 */
void SpectrumVisualizationComponent::renderModeIndicator() {
    if (!modeIndicatorVisible)
        return;

    const char *modeText = getModeText();

    // Font beállítások
    ::tft.setFreeFont();
    ::tft.setTextSize(1);
    ::tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    ::tft.setTextDatum(BC_DATUM);

    // Indicator pozíciója (komponens alatt)
    int indicatorH = indicatorFontHeight + 4;
    int indicatorY = bounds.y + bounds.height + 2;

    // Háttér törlése
    ::tft.fillRect(bounds.x, indicatorY, bounds.width, indicatorH, TFT_BLACK);

    // Szöveg megjelenítése
    ::tft.drawString(modeText, bounds.x + bounds.width / 2, bounds.y + bounds.height);
}

/**
 * @brief Muted állapot megjelenítése
 */
void SpectrumVisualizationComponent::renderMutedState() {
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    ::tft.setTextDatum(MC_DATUM);
    ::tft.setTextColor(TFT_YELLOW);
    ::tft.setFreeFont();
    ::tft.setTextSize(1);

    ::tft.drawString("-- Muted --", x + width / 2, y + effectiveH / 2);
}

/**
 * @brief Off mód megjelenítése
 */
void SpectrumVisualizationComponent::renderOffMode() {
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    ::tft.setTextDatum(MC_DATUM);
    ::tft.setTextColor(TFT_DARKGREY);
    ::tft.setFreeFont();
    ::tft.setTextSize(2);

    int centerX = x + width / 2;
    int centerY = y + effectiveH / 2;
    ::tft.drawString("Off", centerX, centerY);
}

/**
 * @brief Alacsony felbontású spektrum megjelenítése
 */
void SpectrumVisualizationComponent::renderSpectrumLowRes(const SharedAudioData &data) {
    drawFrame();

    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Sáv szélességének kiszámítása
    int barWidth = width / AudioProcessorConstants::LOW_RES_BINS;
    int barSpacing = 1;
    int actualBarWidth = barWidth - barSpacing;

    for (int i = 0; i < AudioProcessorConstants::LOW_RES_BINS; i++) {
        float value = data.data.spectrum.lowResBins[i];
        float peakValue = data.data.spectrum.lowResPeaks[i];

        // Sáv magasságának kiszámítása
        int barHeight = (int)(value * effectiveH);
        int peakHeight = (int)(peakValue * effectiveH);

        // Sáv pozíciója
        int barX = x + i * barWidth;
        int barY = y + effectiveH - barHeight;
        int peakY = y + effectiveH - peakHeight;

        // Háttér törlése
        ::tft.fillRect(barX, y, actualBarWidth, effectiveH, TFT_BLACK);

        // Sáv rajzolása gradienssel
        if (barHeight > 0) {
            drawGradientBar(barX, barY, actualBarWidth, barHeight, value);
        }

        // Peak vonal rajzolása
        if (peakHeight > 0 && peakHeight > barHeight + 2) {
            ::tft.drawFastHLine(barX, peakY, actualBarWidth, TFT_WHITE);
        }
    }
}

/**
 * @brief Magas felbontású spektrum megjelenítése
 */
void SpectrumVisualizationComponent::renderSpectrumHighRes(const SharedAudioData &data) {
    drawFrame();

    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Sprite használata a simább megjelenítéshez
    if (!sprite) {
        sprite = new TFT_eSprite(&::tft);
        if (sprite->createSprite(width, effectiveH)) {
            spriteCreated = true;
        } else {
            delete sprite;
            sprite = nullptr;
        }
    }

    if (sprite && spriteCreated) {
        sprite->fillSprite(TFT_BLACK);

        // Pixel szélességű vonalak
        for (int pixelX = 0; pixelX < width; pixelX++) {
            // FFT bin index kiszámítása
            float freq = (float)pixelX * maxDisplayFrequencyHz / width;
            int binIndex = (int)(freq * AudioProcessorConstants::FFT_SIZE / AudioProcessorConstants::SAMPLE_RATE);

            if (binIndex >= 0 && binIndex < AudioProcessorConstants::FFT_SIZE / 2) {
                float value = data.data.spectrum.highResBins[binIndex];
                int lineHeight = (int)(value * effectiveH);

                if (lineHeight > 0) {
                    uint16_t color = getSpectrumColor(value);
                    sprite->drawFastVLine(pixelX, effectiveH - lineHeight, lineHeight, color);
                }
            }
        }

        sprite->pushSprite(x, y);
    } else {
        // Fallback ha sprite nem elérhető
        renderSpectrumLowRes(data);
    }
}

/**
 * @brief Oszcilloszkóp megjelenítése
 */
void SpectrumVisualizationComponent::renderOscilloscope(const SharedAudioData &data) {
    drawFrame();

    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Háttér törlése
    ::tft.fillRect(x, y, width, effectiveH, TFT_BLACK);

    // Középvonal rajzolása
    int centerY = y + effectiveH / 2;
    ::tft.drawFastHLine(x, centerY, width, TFT_COLOR(40, 40, 40));

    // Oszcilloszkóp rajzolása
    int lastY = centerY;
    for (int pixelX = 1; pixelX < width; pixelX++) {
        // Minta index kiszámítása
        int sampleIndex = pixelX * AudioProcessorConstants::OSCILLOSCOPE_SAMPLES / width;
        if (sampleIndex >= AudioProcessorConstants::OSCILLOSCOPE_SAMPLES)
            sampleIndex = AudioProcessorConstants::OSCILLOSCOPE_SAMPLES - 1;

        // Y pozíció kiszámítása
        float sample = (float)data.data.oscilloscope.samples[sampleIndex] / 32768.0f; // Convert int16 to normalized float
        int currentY = centerY - (int)(sample * effectiveH / 4);                      // /4 a skálázáshoz

        // Korlátok között tartás
        if (currentY < y)
            currentY = y;
        if (currentY >= y + effectiveH)
            currentY = y + effectiveH - 1;

        // Vonal rajzolása
        ::tft.drawLine(x + pixelX - 1, lastY, x + pixelX, currentY, TFT_GREEN);
        lastY = currentY;
    }
}

/**
 * @brief Waterfall megjelenítése
 */
void SpectrumVisualizationComponent::renderWaterfall(const SharedAudioData &data) {
    drawFrame();

    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Waterfall megjelenítése közvetlenül az AudioProcessor adataiból
    for (int row = 0; row < effectiveH && row < AudioProcessorConstants::WATERFALL_HEIGHT; row++) {
        for (int col = 0; col < width; col++) {
            // Frekvencia bin kiszámítása
            int binIndex = (col * AudioProcessorConstants::SPECTRUM_BINS) / width;
            if (binIndex < AudioProcessorConstants::SPECTRUM_BINS) {
                uint8_t intensity = data.data.waterfall.waterfallBuffer[row][binIndex];
                uint16_t color = WATERFALL_COLORS[intensity >> 4]; // 4 bit shift a 16 színhez
                ::tft.drawPixel(x + col, y + row, color);
            }
        }
    }
}

/**
 * @brief Envelope megjelenítése
 */
void SpectrumVisualizationComponent::renderEnvelope(const SharedAudioData &data) {
    drawFrame();

    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Envelope simítás - use envelope data or oscilloscope peak as fallback
    float targetValue = (data.mode == AudioVisualizationType::ENVELOPE)       ? data.data.envelope.smoothedLevel
                        : (data.mode == AudioVisualizationType::OSCILLOSCOPE) ? abs(data.data.oscilloscope.peak)
                                                                              : data.data.spectrum.maxMagnitude;
    envelopeLastSmoothedValue = envelopeLastSmoothedValue * 0.8f + targetValue * 0.2f;

    // Háttér törlése
    ::tft.fillRect(x, y, width, effectiveH, TFT_BLACK);

    // Envelope szint megjelenítése
    int envelopeHeight = (int)(envelopeLastSmoothedValue * effectiveH);
    if (envelopeHeight > 0) {
        // Gradient háttér
        for (int i = 0; i < envelopeHeight; i++) {
            float ratio = (float)i / effectiveH;
            uint16_t color = getSpectrumColor(ratio);
            ::tft.drawFastHLine(x, y + effectiveH - 1 - i, width, color);
        }
    }

    // Statisztikák megjelenítése
    ::tft.setTextColor(TFT_WHITE, TFT_BLACK);
    ::tft.setTextDatum(TL_DATUM);
    ::tft.setFreeFont();
    ::tft.setTextSize(1);

    char statText[64];
    snprintf(statText, sizeof(statText), "CPU:%.1f%% Samples:%lu", data.statistics.cpuUsagePercent, data.statistics.samplesProcessed);
    ::tft.drawString(statText, x + 2, y + 2);
}

/**
 * @brief Spektrum szín kiszámítása
 */
uint16_t SpectrumVisualizationComponent::getSpectrumColor(float intensity) {
    // Intenzitás alapján színezés (0.0 - 1.0)
    intensity = std::max(0.0f, std::min(1.0f, intensity));

    if (intensity < 0.2f) {
        // Sötét kék -> kék
        float ratio = intensity / 0.2f;
        return interpolateColor(TFT_COLOR(0, 0, 64), TFT_BLUE, ratio);
    } else if (intensity < 0.4f) {
        // Kék -> zöld
        float ratio = (intensity - 0.2f) / 0.2f;
        return interpolateColor(TFT_BLUE, TFT_GREEN, ratio);
    } else if (intensity < 0.6f) {
        // Zöld -> sárga
        float ratio = (intensity - 0.4f) / 0.2f;
        return interpolateColor(TFT_GREEN, TFT_YELLOW, ratio);
    } else if (intensity < 0.8f) {
        // Sárga -> narancs
        float ratio = (intensity - 0.6f) / 0.2f;
        return interpolateColor(TFT_YELLOW, TFT_ORANGE, ratio);
    } else {
        // Narancs -> piros -> fehér
        float ratio = (intensity - 0.8f) / 0.2f;
        if (ratio < 0.5f) {
            return interpolateColor(TFT_ORANGE, TFT_RED, ratio * 2.0f);
        } else {
            return interpolateColor(TFT_RED, TFT_WHITE, (ratio - 0.5f) * 2.0f);
        }
    }
}

/**
 * @brief Gradient sáv rajzolása
 */
void SpectrumVisualizationComponent::drawGradientBar(int x, int y, int w, int h, float intensity) {
    for (int i = 0; i < h; i++) {
        float ratio = (float)i / h;
        uint16_t color = getSpectrumColor(intensity * ratio);
        ::tft.drawFastHLine(x, y + h - 1 - i, w, color);
    }
}

/**
 * @brief Színek interpolálása
 */
uint16_t SpectrumVisualizationComponent::interpolateColor(uint16_t color1, uint16_t color2, float ratio) {
    ratio = std::max(0.0f, std::min(1.0f, ratio));

    // RGB565 színek szétbontása
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;

    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;

    // Interpolálás
    uint8_t r = r1 + (uint8_t)((r2 - r1) * ratio);
    uint8_t g = g1 + (uint8_t)((g2 - g1) * ratio);
    uint8_t b = b1 + (uint8_t)((b2 - b1) * ratio);

    // RGB565 visszaállítása
    return (r << 11) | (g << 5) | b;
}

/**
 * @brief Grafikon magasságának számítása
 */
int SpectrumVisualizationComponent::getGraphHeight() const { return bounds.height - getIndicatorHeight(); }

/**
 * @brief Módkijelző területének magassága
 */
int SpectrumVisualizationComponent::getIndicatorHeight() const { return indicatorFontHeight + 4; }

/**
 * @brief Effektív magasság számítása
 */
int SpectrumVisualizationComponent::getEffectiveHeight() const { return bounds.height - getIndicatorHeight(); }

/**
 * @brief Kezdeti mód beállítása
 */
void SpectrumVisualizationComponent::setInitialMode(DisplayMode mode) { currentMode = mode; }

/**
 * @brief Teljes újrarajzolás kényszerítése
 */
void SpectrumVisualizationComponent::forceRedraw() {
    // Force redraw by clearing the area and calling draw
    ::tft.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, TFT_BLACK);
    draw();
}

/**
 * @brief Módkijelző láthatóságának beállítása
 */
void SpectrumVisualizationComponent::setModeIndicatorVisible(bool visible) { modeIndicatorVisible = visible; }
/**
 * @brief CW hangolássegéd rajzolása
 */
void SpectrumVisualizationComponent::renderCWWaterfall(const SharedAudioData &data) {
    drawFrame();

    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // CW waterfall megjelenítése - keskenyebb frekvencia tartomány
    for (int row = 0; row < effectiveH && row < AudioProcessorConstants::WATERFALL_HEIGHT; row++) {
        for (int col = 0; col < width; col++) {
            // CW-hez keskenyebb frekvencia sáv (pl. ±500Hz)
            float centerFreq = 600.0f; // CW center frekvencia
            float bandwidth = 1000.0f; // ±500Hz
            float freq = centerFreq - bandwidth / 2 + (col * bandwidth / width);

            int binIndex = (int)(freq * AudioProcessorConstants::SPECTRUM_BINS / AudioProcessorConstants::SAMPLE_RATE);
            if (binIndex >= 0 && binIndex < AudioProcessorConstants::SPECTRUM_BINS) {
                uint8_t intensity = data.data.waterfall.waterfallBuffer[row][binIndex];
                uint16_t color = WATERFALL_COLORS[intensity >> 4];
                ::tft.drawPixel(x + col, y + row, color);
            }
        }
    }

    // CW frekvencia jelzővonal (600Hz)
    int centerLine = width / 2;
    ::tft.drawFastVLine(x + centerLine, y, effectiveH, TFT_RED);

    // Frekvencia címkék
    ::tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    ::tft.setTextDatum(TC_DATUM);
    ::tft.setTextSize(1);
    ::tft.drawString("600Hz", x + centerLine, y + effectiveH + 2);
}

/**
 * @brief RTTY hangolássegéd rajzolása
 */
void SpectrumVisualizationComponent::renderRTTYWaterfall(const SharedAudioData &data) {
    drawFrame();

    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // RTTY waterfall megjelenítése - 170Hz shift
    for (int row = 0; row < effectiveH && row < AudioProcessorConstants::WATERFALL_HEIGHT; row++) {
        for (int col = 0; col < width; col++) {
            // RTTY-hez 170Hz shift (2125Hz és 2295Hz)
            float centerFreq = 2210.0f; // RTTY center
            float bandwidth = 500.0f;   // Szélesebb tartomány
            float freq = centerFreq - bandwidth / 2 + (col * bandwidth / width);

            int binIndex = (int)(freq * AudioProcessorConstants::SPECTRUM_BINS / AudioProcessorConstants::SAMPLE_RATE);
            if (binIndex >= 0 && binIndex < AudioProcessorConstants::SPECTRUM_BINS) {
                uint8_t intensity = data.data.waterfall.waterfallBuffer[row][binIndex];
                uint16_t color = WATERFALL_COLORS[intensity >> 4];
                ::tft.drawPixel(x + col, y + row, color);
            }
        }
    }

    // RTTY mark és space frekvencia vonalak
    float markFreq = 2295.0f;
    float spaceFreq = 2125.0f;
    float centerFreq = 2210.0f;
    float bandwidth = 500.0f;

    int markLine = (int)((markFreq - (centerFreq - bandwidth / 2)) * width / bandwidth);
    int spaceLine = (int)((spaceFreq - (centerFreq - bandwidth / 2)) * width / bandwidth);

    if (markLine >= 0 && markLine < width) {
        ::tft.drawFastVLine(x + markLine, y, effectiveH, TFT_GREEN);
    }
    if (spaceLine >= 0 && spaceLine < width) {
        ::tft.drawFastVLine(x + spaceLine, y, effectiveH, TFT_BLUE);
    }

    // Frekvencia címkék
    ::tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    ::tft.setTextDatum(TC_DATUM);
    ::tft.setTextSize(1);
    if (markLine >= 0 && markLine < width) {
        ::tft.drawString("M", x + markLine, y + effectiveH + 2);
    }
    if (spaceLine >= 0 && spaceLine < width) {
        ::tft.drawString("S", x + spaceLine, y + effectiveH + 2);
    }
}
