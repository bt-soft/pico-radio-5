#include "SpectrumVisualizationComponent.h"
#include "Band.h"
#include "Config.h"
#include "ConfigData.h"
#include "pico/mutex.h"
#include "rtVars.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Waterfall színpaletta - MiniAudioFft-ből átvéve
const uint16_t SpectrumVisualizationComponent::WATERFALL_COLORS[16] = {
    0x0000, // TFT_BLACK (index 0)
    0x0000, // TFT_BLACK (index 1)
    0x0000, // TFT_BLACK (index 2)
    0x001F, // Nagyon sötét kék
    0x081F, // Sötét kék
    0x0810, // Sötét zöldeskék
    0x0800, // Sötétzöld
    0x0C00, // Közepes zöld
    0x1C00, // Világosabb zöld
    0xFC00, // Narancs
    0xFDE0, // Világos sárga
    0xFFE0, // Sárga
    0xFFFF, // Fehér a csúcsokhoz
    0xFFFF, // Fehér a csúcsokhoz
    0xFFFF, // Fehér a csúcsokhoz
    0xFFFF  // Fehér a csúcsokhoz
};

/**
 * @brief CW Waterfall megjelenítése
 */
void SpectrumVisualizationComponent::renderCWWaterfall(const SharedAudioData &data) {
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Sprite használata a villogás elkerülésére
    if (sprite && spriteCreated) {
        // Waterfall alaprész (ugyanaz mint a normál waterfall)
        sprite->scroll(0, 1);

        // Felül egy új sávot rajzolunk
        for (int pixelX = 0; pixelX < width; pixelX++) {
            int binIndex = (pixelX * AudioProcessorConstants::LOW_RES_BINS) / width;
            if (binIndex < AudioProcessorConstants::LOW_RES_BINS) {
                float value = data.spectrum.lowResBins[binIndex];

                // Dinamikus zaj elnyomás - hangerő függvénye
                float noiseFloor = 0.01f; // Alapértelmezett zaj küszöb
                if (::config.data.currVolume <= 10) {
                    // Alacsony hangerő esetén erősebb zaj elnyomás
                    noiseFloor = 0.05f + (10 - ::config.data.currVolume) * 0.01f; // 0.05-0.15 tartomány
                }

                // Zajszint alatti jelek elnyomása
                if (value < noiseFloor) {
                    value = 0.0f;
                }

                float normalizedValue = data.spectrum.maxMagnitude > 0 ? value / data.spectrum.maxMagnitude : 0.0f;

                uint8_t intensity = (uint8_t)(normalizedValue * 15.0f);
                if (intensity > 15)
                    intensity = 15;

                uint16_t color = WATERFALL_COLORS[intensity];
                sprite->drawPixel(pixelX, 0, color);
            }
        }

        // CW hangolás segítő vonalak - 600 Hz körül
        int cwFreqPixel = (600 * width) / (maxDisplayFrequencyHz);
        if (cwFreqPixel >= 0 && cwFreqPixel < width) {
            // Középső vonal (600 Hz)
            sprite->drawFastVLine(cwFreqPixel, 0, effectiveH, TFT_RED);
            // Sávszélesség jelzők (±50 Hz)
            int cwBandwidthPixels = (100 * width) / (maxDisplayFrequencyHz);
            if (cwBandwidthPixels < 2)
                cwBandwidthPixels = 2;

            int leftEdge = cwFreqPixel - cwBandwidthPixels / 2;
            int rightEdge = cwFreqPixel + cwBandwidthPixels / 2;

            if (leftEdge >= 0)
                sprite->drawFastVLine(leftEdge, 0, effectiveH, TFT_YELLOW);
            if (rightEdge < width)
                sprite->drawFastVLine(rightEdge, 0, effectiveH, TFT_YELLOW);
        }

        // Sprite megjelenítése
        sprite->pushSprite(x, y);
    } else {
        renderMutedState();
    }
}

/**
 * @brief Konstruktor
 */
SpectrumVisualizationComponent::SpectrumVisualizationComponent(int x, int y, int w, int h, RadioMode radioMode)
    : UIComponent(Rect(x, y, w, h)), radioMode(radioMode), currentMode(DisplayMode::Off), lastRenderedMode(DisplayMode::SpectrumLowRes), modeIndicatorVisible(true), modeIndicatorHideTime(0), lastTouchTime(0),
      needsForceRedraw(true), envelopeLastSmoothedValue(0.0f), sprite(nullptr), spriteCreated(false), indicatorFontHeight(0) {

    // Nyquist frekvencia kiszámítása (mintavételi frekvencia fele)
    float nyquistFrequency = AudioProcessorConstants::SAMPLE_RATE / 2.0f; // 10000 Hz

    // Maximális megjelenítési frekvencia beállítása a Nyquist frekvencia és a kívánt frekvencia közül a kisebb
    if (radioMode == RadioMode::AM) {
        maxDisplayFrequencyHz = std::min(MAX_DISPLAY_FREQUENCY_AM, nyquistFrequency); // AM: min(6000, 10000) = 6000 Hz
    } else {
        maxDisplayFrequencyHz = std::min(MAX_DISPLAY_FREQUENCY_FM, nyquistFrequency); // FM: min(15000, 10000) = 10000 Hz
    }

    // Font magasság meghatározása
    ::tft.setFreeFont();
    ::tft.setTextSize(1);
    indicatorFontHeight = ::tft.fontHeight();

    DEBUG("SpectrumVisualization: Komponens létrehozva %dx%d, radioMode=%s, maxFreq=%.0f Hz (Nyquist=%.0f Hz)\n", w, h, radioMode == RadioMode::AM ? "AM" : "FM", maxDisplayFrequencyHz, nyquistFrequency);
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

    // Sprite létrehozása ha még nincs (minden módhoz)
    ensureSpriteCreated();

    // Ha nincs sprite, ne próbáljunk rajzolni
    if (!sprite || !spriteCreated) {
        renderMutedState();
        return;
    }

    // Limit data access frequency to reduce mutex contention but allow smooth animation
    static uint32_t lastDataAccessTime = 0;
    static SharedAudioData cachedData;
    static bool hasCachedData = false;
    uint32_t currentTime = millis();

    // 45 FPS: >= 22 (kissé lassabb, de energiatakarékosabb)
    // 30 FPS: >= 33 (kompromisszum a sebesség és energiafogyasztás között)
    // 60FPS: >= 17 (optimális sebesség, de nagyobb CPU terhelés)
    // 120 FPS: >= 8 (ultra-sima, de nagyobb CPU terhelés)
    bool shouldAccessData = (currentTime - lastDataAccessTime >= 33); // Max 60 FPS data access (smooth animation)

    SharedAudioData data;
    bool success = false;

    if (shouldAccessData) {
        // Only try to access data if we really need to
        if (mutex_try_enter(&g_sharedAudioData.dataMutex, nullptr)) {
            data = g_sharedAudioData;
            cachedData = data; // Cache for next frames
            hasCachedData = true;
            success = true;
            mutex_exit(&g_sharedAudioData.dataMutex);
            lastDataAccessTime = currentTime;
        } else {
            // Mutex is busy - use cached data if available
            static int mutexConflictCount = 0;
            mutexConflictCount++;
            if (mutexConflictCount % 50 == 0) {
                DEBUG("SpectrumVisualization: Mutex busy, using cached data (conflicts: %d)\n", mutexConflictCount);
            }

            if (hasCachedData) {
                data = cachedData;
                success = true;
            }
        }
    } else if (hasCachedData) {
        // Use cached data for smooth rendering
        data = cachedData;
        success = true;
    }

    // Hangerő küszöb ellenőrzés - ha túl alacsony, akkor valószínűleg csak zajt látunk
    // Oszcilloszkóp esetén lazább küszöb, mert ott a jel közvetlenül látható
    bool volumeTooLow = false;
    if (currentMode == DisplayMode::Oscilloscope) {
        volumeTooLow = ::config.data.currVolume <= 2; // Oszcilloszkóp esetén csak 2 vagy kevesebb esetén muted
    } else {
        volumeTooLow = ::config.data.currVolume <= 5; // Spektrum esetén 5 vagy kevesebb esetén muted
    }

    // Dialog ellenőrzés - ha aktív dialógus van, ne rajzoljunk spektrumot
    bool dialogActive = UIComponent::iscurrentScreenDialogActive();

    if (!success || !data.enabled || rtv::muteStat || volumeTooLow || dialogActive) {
        // No data available, audio is muted, volume too low, or dialog is active - show muted state
        if (dialogActive) {
            // Ha dialog aktív, ne rajzoljunk semmit (hagyjuk ahogy van)
            return;
        } else {
            // Egyébként muted state-et mutatunk
            renderMutedState();
            return;
        }
    }

    // DEBUG: Adatok ellenőrzése
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 5000) { // 5 másodpercenként
        // Debug output with dtostrf for safe float formatting
        char maxMagBuf[10];
        char lowBin0Buf[10];
        dtostrf(data.spectrum.maxMagnitude, 4, 2, maxMagBuf);
        dtostrf(data.spectrum.lowResBins[0], 4, 3, lowBin0Buf);

        DEBUG("SpectrumVisualization: mode=%d, enabled=%s, muteStat=%s, volume=%d, dialogActive=%s, dataReady=%s, maxMag=%s, lowBin0=%s\n", (int)data.mode, data.enabled ? "true" : "false",
              rtv::muteStat ? "true" : "false", ::config.data.currVolume, dialogActive ? "true" : "false", data.spectrum.dataReady ? "true" : "false", maxMagBuf, lowBin0Buf);

        // Oszcilloszkóp specifikus debug
        if (data.mode == AudioVisualizationType::OSCILLOSCOPE) {
            DEBUG("Oscilloscope: dataReady=%s, peak=%.1f, rms=%.1f, sample0=%d, sample1=%d, currentMode=%d\n", data.oscilloscope.dataReady ? "true" : "false", data.oscilloscope.peak, data.oscilloscope.rms,
                  data.oscilloscope.samples[0], data.oscilloscope.samples[1], static_cast<int>(currentMode));
        }

        lastDebugTime = millis();
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

    // Állapot frissítése más módok esetén
    lastRenderedMode = currentMode;
    needsForceRedraw = false;

    // Mode indicator megjelenítése ha szükséges
    if (modeIndicatorVisible && millis() < modeIndicatorHideTime) {
        renderModeIndicator();
    }
}

/**
 * @brief Touch esemény kezelése
 */
bool SpectrumVisualizationComponent::handleTouch(const TouchEvent &touch) {
    // Csak akkor kezeljük le, ha ténylegesen a spektrum komponens területén van az érintés
    if (!bounds.contains(touch.x, touch.y)) {
        return false;
    }

    // További ellenőrzés: csak akkor kezeljük le az érintést, ha nem debounce időben vagyunk
    unsigned long currentTime = millis();
    if (currentTime - lastTouchTime < 200) {
        return false; // Ne nyeljuk el az eseményt, hadd menjen tovább más komponensekhez
    }

    // Debug info a touch pozícióról
    DEBUG("SpectrumVisualization: Touch at (%d,%d), bounds (%d,%d,%d,%d)\n", touch.x, touch.y, bounds.x, bounds.y, bounds.width, bounds.height);

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
            currentMode = DisplayMode::SpectrumLowRes; // Cirkuláris: vissza az elejére
            break;
    }

    // Keret újrarajzolása mód váltáskor
    drawFrame();

    // AudioProcessor mód beállítása a megjelenítési mód alapján
    AudioVisualizationType audioMode = AudioVisualizationType::OFF;
    switch (currentMode) {
        case DisplayMode::SpectrumLowRes:
        case DisplayMode::Waterfall:
        case DisplayMode::CWWaterfall:
        case DisplayMode::RTTYWaterfall:
            audioMode = AudioVisualizationType::SPECTRUM_LOW_RES;
            break;
        case DisplayMode::SpectrumHighRes:
            audioMode = AudioVisualizationType::SPECTRUM_HIGH_RES;
            break;
        case DisplayMode::Oscilloscope:
            audioMode = AudioVisualizationType::OSCILLOSCOPE;
            break;
        case DisplayMode::Envelope:
            audioMode = AudioVisualizationType::SPECTRUM_LOW_RES; // Envelope uses spectrum data
            break;
        default:
            audioMode = AudioVisualizationType::SPECTRUM_LOW_RES;
            break;
    }
    AudioProcessorCore1::setVisualizationMode(audioMode);

    setCurrentModeToConfig();

    // Kényszerített újrarajzolás jelzése
    needsForceRedraw = true;
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
 * @brief Keret rajzolása (csak egyszer, nem minden frame-ben)
 */
void SpectrumVisualizationComponent::drawFrame() {
    // A keret rajzolása statikus, nem kell minden frame-ben újrarajzolni
    // Ezt csak mód váltáskor vagy forceRedraw()-nál hívjuk
    const int frameThickness = 1;
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;

    // Tisztítás csak a keret területén
    int frameX = x - frameThickness;
    int frameY = y - frameThickness;
    int frameW = width + (frameThickness * 2);
    int frameH = height + (frameThickness * 2);

    // Csak a keret vonalakat rajzoljuk, ne töröljük a teljes területet
    ::tft.drawRect(frameX, frameY, frameW, frameH, TFT_COLOR(80, 80, 80));
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
    // Sprite létrehozása ha még nincs
    ensureSpriteCreated();

    if (!sprite || !spriteCreated) {
        // Fallback: direct TFT rendering
        int x = bounds.x;
        int y = bounds.y;
        int width = bounds.width;
        int height = bounds.height;
        int effectiveH = height - indicatorFontHeight - 4;

        ::tft.fillRect(x, y, width, effectiveH, TFT_BLACK);
        ::tft.setTextDatum(MC_DATUM);
        ::tft.setTextColor(TFT_YELLOW);
        ::tft.setFreeFont();
        ::tft.setTextSize(1);
        ::tft.drawString("-- Muted --", x + width / 2, y + effectiveH / 2);
        return;
    }

    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Sprite törlése (fekete háttér)
    sprite->fillSprite(TFT_BLACK);

    // Muted szöveg megjelenítése a sprite-ban
    sprite->setTextDatum(MC_DATUM);
    sprite->setTextColor(TFT_YELLOW);
    sprite->setFreeFont();
    sprite->setTextSize(1);
    sprite->drawString("-- Muted --", width / 2, effectiveH / 2);

    // Sprite megjelenítése
    sprite->pushSprite(x, y);
}

/**
 * @brief Off mód megjelenítése
 */
void SpectrumVisualizationComponent::renderOffMode() {
    // Csak újrarajzolás ha szükséges (mód váltás vagy kényszerített)
    bool needsRedraw = (lastRenderedMode != currentMode) || needsForceRedraw;

    if (needsRedraw) {
        // Keret rajzolása Off módban is
        drawFrame();

        int x = bounds.x;
        int y = bounds.y;
        int width = bounds.width;
        int height = bounds.height;
        int effectiveH = height - indicatorFontHeight - 4;

        // Tartalom területének törlése (keret meghagyásával)
        ::tft.fillRect(x, y, width, effectiveH, TFT_BLACK);

        ::tft.setTextDatum(MC_DATUM);
        ::tft.setTextColor(TFT_DARKGREY);
        ::tft.setFreeFont();
        ::tft.setTextSize(2);

        int centerX = x + width / 2;
        int centerY = y + effectiveH / 2;
        ::tft.drawString("Off", centerX, centerY);

        // Állapot frissítése
        lastRenderedMode = currentMode;
        needsForceRedraw = false;
    }
}

/**
 * @brief Alacsony felbontású spektrum megjelenítése
 */
void SpectrumVisualizationComponent::renderSpectrumLowRes(const SharedAudioData &data) {
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Sprite használata a villogás elkerülésére
    if (sprite && spriteCreated) {
        sprite->fillSprite(TFT_BLACK);

        // Sáv szélességének kiszámítása
        int barWidth = width / AudioProcessorConstants::LOW_RES_BINS;
        int barSpacing = 1;
        int actualBarWidth = barWidth - barSpacing;

        for (int i = 0; i < AudioProcessorConstants::LOW_RES_BINS; i++) {
            float value = data.spectrum.lowResBins[i];

            // Dinamikus zaj elnyomás - hangerő függvénye
            float noiseFloor = 0.01f; // Alapértelmezett zaj küszöb
            if (::config.data.currVolume <= 10) {
                // Alacsony hangerő esetén erősebb zaj elnyomás
                noiseFloor = 0.05f + (10 - ::config.data.currVolume) * 0.01f; // 0.05-0.15 tartomány
            }

            // Zajszint alatti jelek elnyomása
            if (value < noiseFloor) {
                value = 0.0f;
            }

            // Dinamikusabb normalizálás: magasabb erősítés és minimális küszöb
            float normalizedValue = 0.0f;
            if (data.spectrum.maxMagnitude > 0.01f) {
                // Erősítés faktor alkalmazása a jobb dinamizmusért
                normalizedValue = (value / data.spectrum.maxMagnitude) * 4.0f; // 4.0x erősítés (még dinamikusabb)
                normalizedValue = std::min(normalizedValue, 1.0f);             // Maximum 1.0-ra korlátozás
            }

            // Logaritmikus skálázás a kisebb jelek jobb láthatóságáért
            if (normalizedValue > 0.0f) {
                normalizedValue = sqrt(normalizedValue); // Gyökös skálázás a jobb dinamizmusért
            }

            // Sáv magasságának kiszámítása (minimum 2 pixel ha van jel)
            int barHeight = (int)(normalizedValue * effectiveH);
            if (barHeight == 0 && value > 0.001f) {
                barHeight = 2; // Minimum látható magasság
            }

            // Sáv pozíciója (sprite koordinátákban)
            int barX = i * barWidth;
            int barY = effectiveH - barHeight;

            // Sáv rajzolása gradienssel sprite-ba
            if (barHeight > 0) {
                drawGradientBarToSprite(barX, barY, actualBarWidth, barHeight, value);
            }
        }

        // Sprite megjelenítése
        sprite->pushSprite(x, y);
    } else {
        // Fallback ha sprite nem elérhető
        renderMutedState();
    }
}

/**
 * @brief Magas felbontású spektrum megjelenítése
 */
void SpectrumVisualizationComponent::renderSpectrumHighRes(const SharedAudioData &data) {
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Sprite használata a simább megjelenítéshez
    if (sprite && spriteCreated) {
        sprite->fillSprite(TFT_BLACK);

        // Pixel szélességű vonalak - javított kiszámítás a teljes spektrum kihasználásáért
        for (int pixelX = 0; pixelX < width; pixelX++) {
            // FFT bin index kiszámítása a megjelenítendő frekvenciatartomány alapján
            float freq = (float)pixelX * maxDisplayFrequencyHz / width;

            // Nyquist frekvencia alapján számítjuk a bin indexet
            float nyquistFreq = AudioProcessorConstants::SAMPLE_RATE / 2.0f;
            float exactBinIndex = freq * AudioProcessorConstants::SPECTRUM_BINS / nyquistFreq;
            int binIndex = (int)exactBinIndex;

            // Biztosítjuk, hogy a bin index a tartományon belül van
            if (binIndex >= 0 && binIndex < AudioProcessorConstants::SPECTRUM_BINS) {
                float value = data.spectrum.highResBins[binIndex];

                // Interpoláció a szomszédos bin-ekkel a simább megjelenítésért
                if (binIndex + 1 < AudioProcessorConstants::SPECTRUM_BINS) {
                    float fraction = exactBinIndex - binIndex;
                    float nextValue = data.spectrum.highResBins[binIndex + 1];
                    value = value * (1.0f - fraction) + nextValue * fraction;
                }

                // Dinamikus zaj elnyomás - hangerő függvénye
                float noiseFloor = 0.01f; // Alapértelmezett zaj küszöb
                if (::config.data.currVolume <= 10) {
                    // Alacsony hangerő esetén erősebb zaj elnyomás
                    noiseFloor = 0.05f + (10 - ::config.data.currVolume) * 0.01f; // 0.05-0.15 tartomány
                }

                // Zajszint alatti jelek elnyomása
                if (value < noiseFloor) {
                    value = 0.0f;
                }

                // Dinamikusabb normalizálás: magasabb erősítés és minimális küszöb
                float normalizedValue = 0.0f;
                if (data.spectrum.maxMagnitude > 0.01f) {
                    // Erősítés faktor alkalmazása a jobb dinamizmusért
                    normalizedValue = (value / data.spectrum.maxMagnitude) * 4.0f; // 4.0x erősítés (még dinamikusabb)
                    normalizedValue = std::min(normalizedValue, 1.0f);             // Maximum 1.0-ra korlátozás
                }

                // Logaritmikus skálázás a kisebb jelek jobb láthatóságáért
                if (normalizedValue > 0.0f) {
                    normalizedValue = sqrt(normalizedValue); // Gyökös skálázás a jobb dinamizmusért
                }

                int lineHeight = (int)(normalizedValue * effectiveH);
                if (lineHeight == 0 && value > 0.001f) {
                    lineHeight = 1; // Minimum látható magasság
                }

                if (lineHeight > 0) {
                    uint16_t color = getSpectrumColor(normalizedValue);
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
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Sprite használata a villogás elkerülésére
    if (sprite && spriteCreated) {
        sprite->fillSprite(TFT_BLACK);

        // Középvonal pozíciója
        int centerY = effectiveH / 2;

        // Középvonal rajzolása sprite-ba
        sprite->drawFastHLine(0, centerY, width, TFT_COLOR(40, 40, 40));

        // Debug info minden 2 másodpercben
        static uint32_t lastOscDebugTime = 0;
        if (millis() - lastOscDebugTime > 2000) {
            DEBUG("Oscilloscope render: dataReady=%s, peak=%.1f, rms=%.1f, samples[0]=%d, samples[10]=%d\n", data.oscilloscope.dataReady ? "true" : "false", data.oscilloscope.peak, data.oscilloscope.rms,
                  data.oscilloscope.samples[0], data.oscilloscope.samples[10]);
            lastOscDebugTime = millis();
        }

        // Valós oszcilloszkóp adatok rajzolása
        if (data.oscilloscope.dataReady) {
            int sampleCount = AudioProcessorConstants::OSCILLOSCOPE_SAMPLES;

            // Automatikus skálázás a peak érték alapján
            float scaleFactor = 1.0f;
            if (data.oscilloscope.peak > 0.1f) {
                // Nagyobb erősítés kis jelek esetén
                if (data.oscilloscope.peak < 200.0f) {
                    scaleFactor = 200.0f / data.oscilloscope.peak; // Nagy erősítés kis jelekhez
                } else {
                    scaleFactor = 1.0f / data.oscilloscope.peak * 0.8f; // 80%-os kihasználás nagy jeleknél
                }
            } else {
                scaleFactor = 50.0f; // Ha szinte nincs jel, 50x erősítés
            }

            bool hasVisibleSignal = false;

            // Próbáljunk egy teszt jelet is rajzolni, ha nincs valós jel
            bool drawTestSignal = (data.oscilloscope.peak < 50.0f); // 50 alatti peak esetén teszt jel

            for (int pixelX = 0; pixelX < width - 1; pixelX++) {
                if (drawTestSignal) {
                    // Teszt szinusz jel
                    float testSample1 = sin(2.0f * M_PI * pixelX / 50.0f) * 0.3f;
                    float testSample2 = sin(2.0f * M_PI * (pixelX + 1) / 50.0f) * 0.3f;

                    int y1 = centerY - (int)(testSample1 * effectiveH / 2);
                    int y2 = centerY - (int)(testSample2 * effectiveH / 2);

                    y1 = constrain(y1, 0, effectiveH - 1);
                    y2 = constrain(y2, 0, effectiveH - 1);

                    sprite->drawLine(pixelX, y1, pixelX + 1, y2, TFT_CYAN);
                    hasVisibleSignal = true;
                } else {
                    // Minta index kiszámítása
                    int sampleIndex = (pixelX * sampleCount) / width;
                    int nextSampleIndex = ((pixelX + 1) * sampleCount) / width;

                    if (sampleIndex < sampleCount && nextSampleIndex < sampleCount) {
                        // Minták normalizálása -1..+1 tartományra és skálázás
                        float sample1 = (data.oscilloscope.samples[sampleIndex] - 2048.0f) / 2048.0f * scaleFactor;
                        float sample2 = (data.oscilloscope.samples[nextSampleIndex] - 2048.0f) / 2048.0f * scaleFactor;

                        // Y pozíciók kiszámítása
                        int y1 = centerY - (int)(sample1 * effectiveH / 2);
                        int y2 = centerY - (int)(sample2 * effectiveH / 2);

                        // Korlátok között tartás
                        y1 = constrain(y1, 0, effectiveH - 1);
                        y2 = constrain(y2, 0, effectiveH - 1);

                        // Jel ellenőrzése
                        if (abs(y1 - centerY) > 2 || abs(y2 - centerY) > 2) {
                            hasVisibleSignal = true;
                        }

                        // Vonal rajzolása
                        sprite->drawLine(pixelX, y1, pixelX + 1, y2, TFT_GREEN);
                    }
                }
            }

            // Ha nincs látható jel, mutassunk egy figyelmeztetést
            if (!hasVisibleSignal && !drawTestSignal) {
                sprite->setCursor(10, effectiveH / 2 - 10);
                sprite->setTextColor(TFT_YELLOW);
                sprite->print("No Signal");
            }
        } else {
            // Ha nincs adat, mutassuk a középvonalat és figyelmeztetést
            sprite->drawFastHLine(0, centerY, width, TFT_YELLOW);
            sprite->setCursor(10, effectiveH / 2 - 10);
            sprite->setTextColor(TFT_RED);
            sprite->print("No Data");
        }

        // Sprite megjelenítése
        sprite->pushSprite(x, y);
    } else {
        // Fallback ha sprite nem elérhető
        renderMutedState();
    }
}

/**
 * @brief Waterfall megjelenítése
 */
void SpectrumVisualizationComponent::renderWaterfall(const SharedAudioData &data) {
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;

    // Sprite használata a villogás elkerülésére
    if (sprite && spriteCreated) {
        // Minden frame-ben elcsúsztatjuk a tartalmat egy sorral le
        sprite->scroll(0, 1);

        // Felül egy új sávot rajzolunk a jelenlegi spektrum alapján
        for (int pixelX = 0; pixelX < width; pixelX++) {
            // Spektrum bin index kiszámítása
            int binIndex = (pixelX * AudioProcessorConstants::LOW_RES_BINS) / width;
            if (binIndex < AudioProcessorConstants::LOW_RES_BINS) {
                float value = data.spectrum.lowResBins[binIndex];

                // Dinamikus zaj elnyomás - hangerő függvénye
                float noiseFloor = 0.01f; // Alapértelmezett zaj küszöb
                if (::config.data.currVolume <= 10) {
                    // Alacsony hangerő esetén erősebb zaj elnyomás
                    noiseFloor = 0.05f + (10 - ::config.data.currVolume) * 0.01f; // 0.05-0.15 tartomány
                }

                // Zajszint alatti jelek elnyomása
                if (value < noiseFloor) {
                    value = 0.0f;
                }

                float normalizedValue = data.spectrum.maxMagnitude > 0 ? value / data.spectrum.maxMagnitude : 0.0f;

                // Szín kiszámítása intenzitás alapján
                uint8_t intensity = (uint8_t)(normalizedValue * 15.0f); // 0-15 tartomány
                if (intensity > 15)
                    intensity = 15;

                uint16_t color = WATERFALL_COLORS[intensity];
                sprite->drawPixel(pixelX, 0, color);
            }
        }

        // Sprite megjelenítése
        sprite->pushSprite(x, y);
    } else {
        // Fallback ha sprite nem elérhető
        renderMutedState();
    }
}

/**
 * @brief Envelope megjelenítése
 */
void SpectrumVisualizationComponent::renderEnvelope(const SharedAudioData &data) {
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Envelope simítás - use spectrum or oscilloscope peak as fallback
    float targetValue = (data.mode == AudioVisualizationType::OSCILLOSCOPE) ? abs(data.oscilloscope.peak) : data.spectrum.maxMagnitude;
    envelopeLastSmoothedValue = envelopeLastSmoothedValue * 0.8f + targetValue * 0.2f;

    // Sprite használata a villogás elkerülésére
    if (sprite && spriteCreated) {
        sprite->fillSprite(TFT_BLACK);

        // Envelope szint megjelenítése sprite-ba
        int envelopeHeight = (int)(envelopeLastSmoothedValue * effectiveH);
        if (envelopeHeight > 0) {
            // Gradient háttér sprite-ba
            for (int i = 0; i < envelopeHeight; i++) {
                float ratio = (float)i / effectiveH;
                uint16_t color = getSpectrumColor(ratio);
                sprite->drawFastHLine(0, effectiveH - 1 - i, width, color);
            }
        }

        // Statisztikák megjelenítése sprite-ba
        sprite->setTextColor(TFT_WHITE, TFT_BLACK);
        sprite->setTextDatum(TL_DATUM);
        sprite->setFreeFont();
        sprite->setTextSize(1);

        char statText[64];
        snprintf(statText, sizeof(statText), "Mode: %d | Enabled: %s", (int)data.mode, data.enabled ? "Yes" : "No");
        sprite->drawString(statText, 2, 2);

        // Sprite megjelenítése
        sprite->pushSprite(x, y);
    } else {
        // Fallback ha sprite nem elérhető
        renderMutedState();
    }
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
 * @brief Gradient sáv rajzolása sprite-ba
 */
void SpectrumVisualizationComponent::drawGradientBarToSprite(int x, int y, int w, int h, float intensity) {
    if (!sprite || !spriteCreated)
        return;

    for (int i = 0; i < h; i++) {
        float ratio = (float)i / h;
        uint16_t color = getSpectrumColor(intensity * ratio);
        sprite->drawFastHLine(x, y + h - 1 - i, w, color);
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
    // Teljes terület törlése
    ::tft.fillRect(bounds.x - 1, bounds.y - 1, bounds.width + 2, bounds.height + 2, TFT_BLACK);

    // Kényszerített újrarajzolás jelzése
    needsForceRedraw = true;

    // Keret újrarajzolása
    drawFrame();

    // Tartalom újrarajzolása
    draw();
}

/**
 * @brief Módkijelző láthatóságának beállítása
 */
void SpectrumVisualizationComponent::setModeIndicatorVisible(bool visible) { modeIndicatorVisible = visible; }

/**
 * @brief Kényszerített újrarajzolás
 */
void SpectrumVisualizationComponent::forceRedrawOnNextFrame() { needsForceRedraw = true; }

/**
 * @brief RTTY hangolássegéd rajzolása
 */
void SpectrumVisualizationComponent::renderRTTYWaterfall(const SharedAudioData &data) {
    int x = bounds.x;
    int y = bounds.y;
    int width = bounds.width;
    int height = bounds.height;
    int effectiveH = height - indicatorFontHeight - 4;

    // Sprite használata a villogás elkerülésére
    if (sprite && spriteCreated) {
        // Waterfall alaprész
        sprite->scroll(0, 1);

        // Felül egy új sávot rajzolunk
        for (int pixelX = 0; pixelX < width; pixelX++) {
            int binIndex = (pixelX * AudioProcessorConstants::LOW_RES_BINS) / width;
            if (binIndex < AudioProcessorConstants::LOW_RES_BINS) {
                float value = data.spectrum.lowResBins[binIndex];

                // Dinamikus zaj elnyomás - hangerő függvénye
                float noiseFloor = 0.01f; // Alapértelmezett zaj küszöb
                if (::config.data.currVolume <= 10) {
                    // Alacsony hangerő esetén erősebb zaj elnyomás
                    noiseFloor = 0.05f + (10 - ::config.data.currVolume) * 0.01f; // 0.05-0.15 tartomány
                }

                // Zajszint alatti jelek elnyomása
                if (value < noiseFloor) {
                    value = 0.0f;
                }

                float normalizedValue = data.spectrum.maxMagnitude > 0 ? value / data.spectrum.maxMagnitude : 0.0f;

                uint8_t intensity = (uint8_t)(normalizedValue * 15.0f);
                if (intensity > 15)
                    intensity = 15;

                uint16_t color = WATERFALL_COLORS[intensity];
                sprite->drawPixel(pixelX, 0, color);
            }
        }

        // RTTY hangolás segítő vonalak - 2125 Hz (mark) és 2295 Hz (space)
        int markFreqPixel = (2125 * width) / (maxDisplayFrequencyHz);
        int spaceFreqPixel = (2295 * width) / (maxDisplayFrequencyHz);

        if (markFreqPixel >= 0 && markFreqPixel < width) {
            sprite->drawFastVLine(markFreqPixel, 0, effectiveH, TFT_GREEN); // Mark freq
        }
        if (spaceFreqPixel >= 0 && spaceFreqPixel < width) {
            sprite->drawFastVLine(spaceFreqPixel, 0, effectiveH, TFT_RED); // Space freq
        }

        // Középső frekvencia (2210 Hz)
        int centerFreqPixel = (2210 * width) / (maxDisplayFrequencyHz);
        if (centerFreqPixel >= 0 && centerFreqPixel < width) {
            sprite->drawFastVLine(centerFreqPixel, 0, effectiveH, TFT_YELLOW); // Center freq
        }

        // Sprite megjelenítése
        sprite->pushSprite(x, y);
    } else {
        renderMutedState();
    }
}

/**
 * @brief Config érték alapján DisplayMode konvertálás
 */
SpectrumVisualizationComponent::DisplayMode SpectrumVisualizationComponent::configValueToDisplayMode(uint8_t configValue) {
    switch (configValue) {
        case static_cast<uint8_t>(AudioComponentType::OFF):
            return DisplayMode::Off;
        case static_cast<uint8_t>(AudioComponentType::SPECTRUM_LOW_RES):
            return DisplayMode::SpectrumLowRes;
        case static_cast<uint8_t>(AudioComponentType::SPECTRUM_HIGH_RES):
            return DisplayMode::SpectrumHighRes;
        case static_cast<uint8_t>(AudioComponentType::OSCILLOSCOPE):
            return DisplayMode::Oscilloscope;
        case static_cast<uint8_t>(AudioComponentType::ENVELOPE):
            return DisplayMode::Envelope;
        case static_cast<uint8_t>(AudioComponentType::WATERFALL):
            return DisplayMode::Waterfall;
        case static_cast<uint8_t>(AudioComponentType::CW_WATERFALL):
            return DisplayMode::CWWaterfall;
        case static_cast<uint8_t>(AudioComponentType::RTTY_WATERFALL):
            return DisplayMode::RTTYWaterfall;
        default:
            return DisplayMode::Off; // Alapértelmezett Off
    }
}

/**
 * @brief DisplayMode alapján config érték konvertálás
 */
uint8_t SpectrumVisualizationComponent::displayModeToConfigValue(DisplayMode mode) {
    switch (mode) {
        case DisplayMode::Off:
            return static_cast<uint8_t>(AudioComponentType::OFF);
        case DisplayMode::SpectrumLowRes:
            return static_cast<uint8_t>(AudioComponentType::SPECTRUM_LOW_RES);
        case DisplayMode::SpectrumHighRes:
            return static_cast<uint8_t>(AudioComponentType::SPECTRUM_HIGH_RES);
        case DisplayMode::Oscilloscope:
            return static_cast<uint8_t>(AudioComponentType::OSCILLOSCOPE);
        case DisplayMode::Envelope:
            return static_cast<uint8_t>(AudioComponentType::ENVELOPE);
        case DisplayMode::Waterfall:
            return static_cast<uint8_t>(AudioComponentType::WATERFALL);
        case DisplayMode::CWWaterfall:
            return static_cast<uint8_t>(AudioComponentType::CW_WATERFALL);
        case DisplayMode::RTTYWaterfall:
            return static_cast<uint8_t>(AudioComponentType::RTTY_WATERFALL);
        default:
            return static_cast<uint8_t>(AudioComponentType::OFF); // Alapértelmezett Off
    }
}

/**
 * @brief Aktuális mód mentése config-ba
 */
void SpectrumVisualizationComponent::setCurrentModeToConfig() {
    uint8_t configValue = displayModeToConfigValue(currentMode);

    if (radioMode == RadioMode::AM) {
        ::config.data.audioModeAM = configValue;
    } else {
        ::config.data.audioModeFM = configValue;
    }
}

/**
 * @brief Mód betöltése config-ból
 */
void SpectrumVisualizationComponent::loadModeFromConfig() {
    uint8_t configValue = radioMode == RadioMode::AM ? ::config.data.audioModeAM : ::config.data.audioModeFM;
    DisplayMode newMode = configValueToDisplayMode(configValue);

    DEBUG("SpectrumVisualization: Loading mode %d from config (%s) -> DisplayMode %d\n", configValue, radioMode == RadioMode::AM ? "AM" : "FM", static_cast<int>(newMode));

    currentMode = newMode;

    // AudioProcessor mód beállítása
    AudioVisualizationType audioMode = AudioVisualizationType::OFF;
    switch (currentMode) {
        case DisplayMode::SpectrumLowRes:
        case DisplayMode::Waterfall:
        case DisplayMode::CWWaterfall:
        case DisplayMode::RTTYWaterfall:
            audioMode = AudioVisualizationType::SPECTRUM_LOW_RES;
            break;
        case DisplayMode::SpectrumHighRes:
            audioMode = AudioVisualizationType::SPECTRUM_HIGH_RES;
            break;
        case DisplayMode::Oscilloscope:
            audioMode = AudioVisualizationType::OSCILLOSCOPE;
            break;
        case DisplayMode::Envelope:
            audioMode = AudioVisualizationType::SPECTRUM_LOW_RES; // Envelope uses spectrum data
            break;
        default:
            audioMode = AudioVisualizationType::SPECTRUM_LOW_RES;
            break;
    }
    AudioProcessorCore1::setVisualizationMode(audioMode);

    // Kényszerített újrarajzolás
    needsForceRedraw = true;
}

/**
 * @brief Sprite létrehozásának biztosítása
 */
void SpectrumVisualizationComponent::ensureSpriteCreated() {
    if (!sprite) {
        int effectiveH = getEffectiveHeight();
        sprite = new TFT_eSprite(&::tft);
        if (sprite->createSprite(bounds.width, effectiveH)) {
            spriteCreated = true;
            DEBUG("SpectrumVisualization: Sprite létrehozva %dx%d\n", bounds.width, effectiveH);
        } else {
            delete sprite;
            sprite = nullptr;
            spriteCreated = false;
            DEBUG("SpectrumVisualization: Sprite létrehozása sikertelen!\n");
        }
    }
}
