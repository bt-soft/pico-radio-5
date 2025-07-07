#include "AudioDisplayComponent.h"
#include "Config.h"
#include "defines.h"

extern Config config; // Globális config elérése

AudioDisplayComponent::AudioDisplayComponent(int16_t x, int16_t y, int16_t width, int16_t height, AudioAnalyzer &analyzer)
    : UIComponent(Rect(x, y, width, height)), audioAnalyzer(analyzer), currentMode(AudioDisplayMode::SPECTRUM_LOW_RES), sprite(nullptr), spriteCreated(false), modeDisplayStartTime(0), waterfallCurrentLine(0) {
    // Waterfall adatok inicializálása
    memset(waterfallData, 0, sizeof(waterfallData));

    // CW/RTTY helper inicializálása
    initCWRTTYHelper();

    // Utolsó audio adat inicializálása
    memset(&lastAudioData, 0, sizeof(lastAudioData));

    // Mód felirat megjelenítésének kezdete
    modeDisplayStartTime = millis();

    // Kezdeti rajzolás biztosítása
    markForRedraw();

    DEBUG("AudioDisplayComponent created at (%d,%d) size %dx%d, mode: %s\n", x, y, width, height, getModeString(currentMode));
}

AudioDisplayComponent::~AudioDisplayComponent() { destroySprite(); }

void AudioDisplayComponent::draw() {
    static uint32_t lastDebugTime = 0;
    uint32_t currentTime = millis();

    if (currentMode == AudioDisplayMode::OFF) {
        return; // Kikapcsolt állapotban nem rajzolunk semmit
    }

    // Debug output minden 2 másodpercben
    if (currentTime - lastDebugTime > 2000) {
        DEBUG("AudioDisplayComponent::draw() called, mode: %s\n", getModeString(currentMode));
        lastDebugTime = currentTime;
    }

    // TFT referencia lekérése
    extern TFT_eSPI tft;

    // Sprite létrehozása, ha még nincs
    if (!spriteCreated) {
        createSprite(tft);
    }

    if (!sprite) {
        return; // Sprite létrehozás sikertelen
    }

    // Legfrissebb audio adatok lekérése
    AudioData audioData;
    bool hasNewData = audioAnalyzer.getLatestData(audioData); // Non-blocking

    // Debug output minden 2 másodpercben
    if (currentTime - lastDebugTime > 2000) {
        DEBUG("hasNewData: %s, audioData.lowRes[0]: %u, lastAudioData.lowRes[0]: %u\n", hasNewData ? "YES" : "NO", audioData.lowResSpectrum[0], lastAudioData.lowResSpectrum[0]);
    }

    if (hasNewData) {
        lastAudioData = audioData;

        // Debug output audio adatokról minden 2 másodpercben
        if (currentTime - lastDebugTime > 2000) {
            DEBUG("Audio data updated - isMuted: %s, lowRes[0]: %u, lowRes[7]: %u\n", audioData.isMuted ? "YES" : "NO", audioData.lowResSpectrum[0], audioData.lowResSpectrum[7]);
        }

        // Waterfall adatok frissítése, ha szükséges
        if (currentMode == AudioDisplayMode::WATERFALL || currentMode == AudioDisplayMode::WATERFALL_CW_RTTY) {
            updateWaterfallData();
        }
    } else {
        // Nincs új adat, debug output
        if (currentTime - lastDebugTime > 2000) {
            DEBUG("No new audio data available\n");
        }

        // TESZT: Ideiglenesen kikapcsolva - nézzük meg a valós adatokat
        // Ha nincs új adat, akkor is frissítsük a kijelzést animált adatokkal
        /*
        for (int i = 0; i < AudioData::LOW_RES_BINS; i++) {
            lastAudioData.lowResSpectrum[i] = (uint16_t)(2048 + 1000 * sin(millis() * 0.002 + i * 0.8));
        }
        */
    }

    // Sprite tisztítása
    sprite->fillSprite(TFT_BLACK);

    // Keret rajzolása a komponens körül (vizuális teszt)
    sprite->drawRect(0, 0, bounds.width, bounds.height, TFT_WHITE);

    // Módtól függő rajzolás
    switch (currentMode) {
        case AudioDisplayMode::SPECTRUM_LOW_RES:
            drawLowResSpectrum();
            break;
        case AudioDisplayMode::SPECTRUM_HIGH_RES:
            drawHighResSpectrum();
            break;
        case AudioDisplayMode::OSCILLOSCOPE:
            drawOscilloscope();
            break;
        case AudioDisplayMode::ENVELOPE:
            drawEnvelope();
            break;
        case AudioDisplayMode::WATERFALL:
            drawWaterfall();
            break;
        case AudioDisplayMode::WATERFALL_CW_RTTY:
            drawWaterfallCWRTTY();
            break;
        default:
            break;
    }

    // MUTED felirat, ha a rádió némított
    if (lastAudioData.isMuted) {
        drawMutedLabel();
    }

    // Mód felirat megjelenítése (20mp-ig)
    if (currentTime - modeDisplayStartTime < MODE_DISPLAY_DURATION) {
        drawModeLabel();
    }

    // Sprite a képernyőre rajzolása
    sprite->pushSprite(bounds.x, bounds.y);

    // Redraw scheduling optimalizálása - gyakoribb frissítés a teszt miatt
    static uint32_t lastRedrawSchedule = 0;
    if (hasNewData || (currentTime - lastRedrawSchedule > 50)) { // 20 FPS
        markForRedraw();
        lastRedrawSchedule = currentTime;
    }
}

bool AudioDisplayComponent::handleTouch(const TouchEvent &event) {
    if (!event.pressed) {
        return false; // Csak press eseményeket kezelünk
    }

    // Touch pozíció ellenőrzése
    if (event.x >= bounds.x && event.x < bounds.x + bounds.width && event.y >= bounds.y && event.y < bounds.y + bounds.height) {

        // Következő módra váltás
        nextMode();
        return true;
    }

    return false;
}

void AudioDisplayComponent::setDisplayMode(AudioDisplayMode mode) {
    if (currentMode != mode) {
        currentMode = mode;
        modeDisplayStartTime = millis(); // Mód felirat megjelenítésének kezdete

        DEBUG("AudioDisplayComponent: Mode changed to %s\n", getModeString(mode));
    }
}

void AudioDisplayComponent::nextMode() {
    int nextModeInt = (int)currentMode + 1;
    if (nextModeInt >= (int)AudioDisplayMode::MODE_COUNT) {
        nextModeInt = 0; // Vissza az elejére
    }

    setDisplayMode((AudioDisplayMode)nextModeInt);
}

void AudioDisplayComponent::createSprite(TFT_eSPI &tft) {
    if (spriteCreated) {
        return;
    }

    sprite = new TFT_eSprite(&tft);
    if (sprite && sprite->createSprite(bounds.width, bounds.height)) {
        spriteCreated = true;
        DEBUG("AudioDisplayComponent: Sprite created (%dx%d)\n", bounds.width, bounds.height);
    } else {
        delete sprite;
        sprite = nullptr;
        DEBUG("AudioDisplayComponent: Failed to create sprite\n");
    }
}

void AudioDisplayComponent::destroySprite() {
    if (sprite) {
        sprite->deleteSprite();
        delete sprite;
        sprite = nullptr;
        spriteCreated = false;
    }
}

void AudioDisplayComponent::drawLowResSpectrum() {
    if (!sprite)
        return;

    const int barWidth = bounds.width / AudioData::LOW_RES_BINS;
    const int maxBarHeight = bounds.height - 20; // Kis hely a peak vonalaknak

    // Debug: első oszlop értékének ellenőrzése
    static uint32_t lastSpectrumDebug = 0;
    uint32_t now = millis();
    if (now - lastSpectrumDebug > 5000) {
        DEBUG("Spectrum - barWidth: %d, maxBarHeight: %d, firstBar: %u->%d\n", barWidth, maxBarHeight, lastAudioData.lowResSpectrum[0], map(lastAudioData.lowResSpectrum[0], 0, 4095, 0, maxBarHeight));
        lastSpectrumDebug = now;
    }
    for (int i = 0; i < AudioData::LOW_RES_BINS; i++) {
        int x_pos = i * barWidth;

        // Spektrum oszlop magassága (0-4095 -> 0-maxBarHeight)
        uint16_t spectrumValue = lastAudioData.lowResSpectrum[i];
        int barHeight = map(spectrumValue, 0, 4095, 0, maxBarHeight);
        int y_pos = bounds.height - barHeight;

        // Peak hold magassága
        int peakHeight = map(lastAudioData.peakHold[i], 0, 4095, 0, maxBarHeight);
        int peak_y = bounds.height - peakHeight;

        // Spektrum oszlop rajzolása (gradiens színekkel)
        for (int y = y_pos; y < bounds.height; y++) {
            uint16_t color;
            int intensity = map(y, bounds.height, y_pos, 0, 255);

            if (intensity < 85) {
                color = sprite->color565(0, intensity * 3, 0); // Zöld
            } else if (intensity < 170) {
                color = sprite->color565((intensity - 85) * 3, 255, 0); // Sárga
            } else {
                color = sprite->color565(255, 255 - (intensity - 170) * 3, 0); // Piros
            }

            sprite->drawFastHLine(x_pos, y, barWidth - 1, color);
        }

        // Peak hold vonal rajzolása
        if (peakHeight > 5) {
            sprite->drawFastHLine(x_pos, peak_y, barWidth - 1, TFT_WHITE);
        }
    }
}

void AudioDisplayComponent::drawHighResSpectrum() {
    if (!sprite)
        return;

    const int maxBarHeight = bounds.height - 10;
    const int barsToShow = min(bounds.width, (int)AudioData::SPECTRUM_BINS - 1);

    for (int i = 0; i < barsToShow; i++) {
        int spectrumIndex = map(i, 0, barsToShow - 1, 0, AudioData::SPECTRUM_BINS - 2);

        // Spektrum oszlop magassága
        int barHeight = map(lastAudioData.spectrumData[spectrumIndex], 0, 4095, 0, maxBarHeight);
        int y_pos = bounds.height - barHeight;

        // Egyszerű oszlop színezés
        uint16_t color;
        if (barHeight < maxBarHeight / 3) {
            color = TFT_GREEN;
        } else if (barHeight < 2 * maxBarHeight / 3) {
            color = TFT_YELLOW;
        } else {
            color = TFT_RED;
        }

        sprite->drawFastVLine(i, y_pos, barHeight, color);
    }
}

void AudioDisplayComponent::drawOscilloscope() {
    if (!sprite)
        return;
    const int midY = bounds.height / 2;
    const int maxAmplitude = bounds.height / 2 - 5;

    // Trigger keresése (egyszerű pozitív átmenet)
    int triggerIndex = 0;
    for (int i = 1; i < AudioData::FFT_SIZE - bounds.width; i++) {
        if (lastAudioData.rawSamples[i - 1] < 2048 && lastAudioData.rawSamples[i] >= 2048) {
            triggerIndex = i;
            break;
        }
    }

    // Oszcilloszkóp görbe rajzolása
    for (int x = 0; x < bounds.width - 1; x++) {
        if (triggerIndex + x >= AudioData::FFT_SIZE)
            break;

        // Aktuális és következő minta
        int sample1 = lastAudioData.rawSamples[triggerIndex + x];
        int sample2 = lastAudioData.rawSamples[triggerIndex + x + 1];

        // Konverzió screen koordinátákra
        int y1 = midY - map(sample1, 0, 4095, -maxAmplitude, maxAmplitude);
        int y2 = midY - map(sample2, 0, 4095, -maxAmplitude, maxAmplitude);

        sprite->drawLine(x, y1, x + 1, y2, TFT_GREEN);
    }

    // Középső vonal
    sprite->drawFastHLine(0, midY, bounds.width, TFT_DARKGREY);
}

void AudioDisplayComponent::drawEnvelope() {
    if (!sprite)
        return;

    // Burkológörbe számítása (egyszerű módon)
    static uint16_t envelopeHistory[256]; // Korábbi értékek tárolása
    static int historyIndex = 0;

    // Átlagos magnitúdó számítása
    uint32_t sum = 0;
    for (int i = 0; i < AudioData::LOW_RES_BINS; i++) {
        sum += lastAudioData.lowResSpectrum[i];
    }
    uint16_t avgMagnitude = sum / AudioData::LOW_RES_BINS;

    // Burkológörbe frissítése
    envelopeHistory[historyIndex] = avgMagnitude;
    historyIndex = (historyIndex + 1) % 256;

    // Burkológörbe rajzolása
    const int maxEnvelopeHeight = bounds.height - 10;
    for (int x = 0; x < bounds.width - 1; x++) {
        if (x >= 256)
            break;

        int idx1 = (historyIndex + x) % 256;
        int idx2 = (historyIndex + x + 1) % 256;

        int y1 = bounds.height - map(envelopeHistory[idx1], 0, 4095, 0, maxEnvelopeHeight);
        int y2 = bounds.height - map(envelopeHistory[idx2], 0, 4095, 0, maxEnvelopeHeight);

        sprite->drawLine(x, y1, x + 1, y2, TFT_CYAN);
    }
}

void AudioDisplayComponent::drawWaterfall() {
    if (!sprite)
        return; // Waterfall rajzolása (minden sor egy időpillanat spektruma)
    for (int y = 0; y < min(bounds.height, WATERFALL_LINES); y++) {
        int dataLine = (waterfallCurrentLine - y + WATERFALL_LINES) % WATERFALL_LINES;

        for (int x = 0; x < bounds.width; x++) {
            if (x >= AudioData::SPECTRUM_BINS)
                break;

            uint16_t magnitude = waterfallData[dataLine][x];
            uint16_t color = getMagnitudeColor(magnitude);

            sprite->drawPixel(x, y, color);
        }
    }
}

void AudioDisplayComponent::drawWaterfallCWRTTY() {
    if (!sprite)
        return;

    // Alapvetően ugyanaz, mint a sima waterfall, de hangoló vonalakkal
    drawWaterfall(); // CW/RTTY hangoló vonalak rajzolása
    int centerX1 = map(cwRttyHelper.centerBin1, 0, AudioData::SPECTRUM_BINS, 0, bounds.width);
    sprite->drawFastVLine(centerX1, 0, bounds.height, TFT_RED);

    if (cwRttyHelper.isRTTY) {
        int centerX2 = map(cwRttyHelper.centerBin2, 0, AudioData::SPECTRUM_BINS, 0, bounds.width);
        sprite->drawFastVLine(centerX2, 0, bounds.height, TFT_RED);

        // Sáv jelölése
        int bandStart = min(centerX1, centerX2);
        int bandEnd = max(centerX1, centerX2);
        sprite->drawRect(bandStart, bounds.height - 10, bandEnd - bandStart, 8, TFT_YELLOW);
    }
}

void AudioDisplayComponent::drawModeLabel() {
    if (!sprite)
        return;

    const char *modeStr = getModeString(currentMode);

    // Szöveg pozíció (középre igazítva)
    sprite->setTextColor(TFT_WHITE, TFT_BLACK);
    sprite->setTextDatum(MC_DATUM);
    sprite->drawString(modeStr, bounds.width / 2, bounds.height / 2);
}

void AudioDisplayComponent::drawMutedLabel() {
    if (!sprite)
        return;

    sprite->setTextColor(TFT_RED, TFT_BLACK);
    sprite->setTextDatum(MC_DATUM);
    sprite->drawString("MUTED", bounds.width / 2, bounds.height / 2);
}

const char *AudioDisplayComponent::getModeString(AudioDisplayMode mode) const {
    switch (mode) {
        case AudioDisplayMode::OFF:
            return "OFF";
        case AudioDisplayMode::SPECTRUM_LOW_RES:
            return "SPECTRUM LOW";
        case AudioDisplayMode::SPECTRUM_HIGH_RES:
            return "SPECTRUM HIGH";
        case AudioDisplayMode::OSCILLOSCOPE:
            return "OSCILLOSCOPE";
        case AudioDisplayMode::ENVELOPE:
            return "ENVELOPE";
        case AudioDisplayMode::WATERFALL:
            return "WATERFALL";
        case AudioDisplayMode::WATERFALL_CW_RTTY:
            return "CW/RTTY";
        default:
            return "UNKNOWN";
    }
}

int AudioDisplayComponent::frequencyToBin(float frequency) const { return (int)(frequency * AudioData::FFT_SIZE / AudioData::SAMPLE_RATE); }

void AudioDisplayComponent::initCWRTTYHelper() {
    // CW mód alapértelmezett beállítása
    cwRttyHelper.targetFreq1 = CW_DECODER_DEFAULT_FREQUENCY;
    cwRttyHelper.targetFreq2 = 0;
    cwRttyHelper.centerBin1 = frequencyToBin(cwRttyHelper.targetFreq1);
    cwRttyHelper.centerBin2 = 0;
    cwRttyHelper.isRTTY = false;

    // TODO: Config-ból olvasni a RTTY beállításokat
    // cwRttyHelper.isRTTY = true;
    // cwRttyHelper.targetFreq1 = RTTY_DEFAULT_MARKER_FREQUENCY;
    // cwRttyHelper.targetFreq2 = RTTY_DEFAULT_SPACE_FREQUENCY;
}

void AudioDisplayComponent::updateWaterfallData() {
    // Következő waterfall sor
    waterfallCurrentLine = (waterfallCurrentLine + 1) % WATERFALL_LINES;

    // Spektrum adatok másolása a waterfall pufferbe
    for (int i = 0; i < AudioData::SPECTRUM_BINS && i < bounds.width; i++) {
        waterfallData[waterfallCurrentLine][i] = lastAudioData.spectrumData[i];
    }
}

uint16_t AudioDisplayComponent::getMagnitudeColor(uint16_t magnitude) const {
    // Heatmap színezés: fekete -> kék -> zöld -> sárga -> piros -> fehér
    int level = map(magnitude, 0, 4095, 0, 255);

    if (level < 51) {
        // Fekete -> kék
        return sprite->color565(0, 0, level * 5);
    } else if (level < 102) {
        // Kék -> zöld
        return sprite->color565(0, (level - 51) * 5, 255);
    } else if (level < 153) {
        // Zöld -> sárga
        return sprite->color565((level - 102) * 5, 255, 255 - (level - 102) * 5);
    } else if (level < 204) {
        // Sárga -> piros
        return sprite->color565(255, 255 - (level - 153) * 5, 0);
    } else {
        // Piros -> fehér
        int white = (level - 204) * 5;
        return sprite->color565(255, white, white);
    }
}
