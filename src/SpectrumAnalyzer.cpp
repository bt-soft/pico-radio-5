#include "SpectrumAnalyzer.h"
#include "Config.h"
#include <algorithm>
#include <cstring>

// ===================================================================
// Konstruktor és inicializálás
// ===================================================================

/**
 * @brief SpectrumAnalyzer konstruktor - Spektrum analizátor létrehozása
 * @param bounds Komponens határai
 */
SpectrumAnalyzer::SpectrumAnalyzer(const Rect &bounds) : UIComponent(bounds), enabled(true), lastUpdateTime(0), barCount(0), barWidth(0), sprite(nullptr), spriteCreated(false) {

    // Alapértelmezett színek beállítása - TFT_eSPI beépített konstansokkal
    colorLow = TFT_GREEN;
    colorMid = TFT_YELLOW;
    colorHigh = TFT_RED;
    colorPeak = TFT_WHITE;
    colorBackground = TFT_BLACK;

    // Adatok inicializálása
    memset(currentLevels, 0, sizeof(currentLevels));
    memset(peakLevels, 0, sizeof(peakLevels));
    memset(peakHoldTime, 0, sizeof(peakHoldTime));

    // Bar layout számítása
    calculateBarLayout();

    // Sprite létrehozása villogás ellen
    createSprite();

    DEBUG("SpectrumAnalyzer: Létrehozva %dx%d méretben, %d bar\n", bounds.width, bounds.height, barCount);
}

// ===================================================================
// Bar layout számítás
// ===================================================================

/**
 * @brief Bar-ok számításának és méretezésének inicializálása
 */
void SpectrumAnalyzer::calculateBarLayout() {
    if (bounds.width < MIN_BAR_WIDTH) {
        barCount = 0;
        barWidth = 0;
        return;
    }

    // Maximális bar szám a low-res spektrum alapján
    uint16_t maxBars = AudioProcessorConstants::LOW_RES_BINS;

    // Számítsuk ki, hány bar fér el a rendelkezésre álló szélességben
    uint16_t availableWidth = bounds.width;
    uint16_t totalSpacing = 0;

    // Próbáljuk meg elférni az összes bar-t
    for (uint16_t testBarCount = maxBars; testBarCount > 0; testBarCount--) {
        totalSpacing = (testBarCount - 1) * BAR_SPACING;
        uint16_t testBarWidth = (availableWidth - totalSpacing) / testBarCount;

        if (testBarWidth >= MIN_BAR_WIDTH) {
            barCount = testBarCount;
            barWidth = testBarWidth;
            break;
        }
    }

    // Ha még mindig nincs megoldás, használjunk minimális bar szélességet
    if (barCount == 0) {
        barCount = 1;
        barWidth = MIN_BAR_WIDTH;
    }

    DEBUG("SpectrumAnalyzer: Bar layout - count: %d, width: %d, spacing: %d\n", barCount, barWidth, BAR_SPACING);
}

// ===================================================================
// UIComponent interface megvalósítás
// ===================================================================

/**
 * @brief Komponens frissítése - Spektrum adatok és peak hold
 */
void SpectrumAnalyzer::update() {
    if (!enabled)
        return;

    uint32_t currentTime = millis();
    uint32_t deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;

    // Spektrum adatok frissítése
    updateSpectrumData();

    // Peak hold frissítése
    updatePeakHold(deltaTime);
}

/**
 * @brief Komponens rajzolása
 */
void SpectrumAnalyzer::draw() {
    if (!enabled || barCount == 0) {
        // Ha letiltott, rajzoljunk fekete téglalapot közvetlenül
        TFT_eSPI &display = ::tft;
        display.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, colorBackground);
        return;
    }

    // Ha nincs sprite, visszatérünk
    if (!spriteCreated || sprite == nullptr) {
        return;
    }

    // Sprite háttér törlése
    sprite->fillSprite(colorBackground);

    // Bar-ok rajzolása sprite-ba
    uint16_t currentX = 0; // Sprite-ban relatív koordináták

    for (uint16_t i = 0; i < barCount && i < AudioProcessorConstants::LOW_RES_BINS; i++) {
        // Normalizált szintek számítása (0.0-1.0)
        float normalizedLevel = (currentLevels[i] - DB_MIN) / DB_RANGE;
        float normalizedPeak = (peakLevels[i] - DB_MIN) / DB_RANGE;

        // Határok ellenőrzése
        normalizedLevel = std::max(0.0f, std::min(1.0f, normalizedLevel));
        normalizedPeak = std::max(0.0f, std::min(1.0f, normalizedPeak));

        // Bar rajzolása sprite-ba
        drawBarToSprite(i, currentX, normalizedLevel, normalizedPeak);

        // Következő bar pozíciója
        currentX += barWidth + BAR_SPACING;
    }

    // Sprite megjelenítése a képernyőn
    sprite->pushSprite(bounds.x, bounds.y);
}

// ===================================================================
// Spektrum analizátor specifikus funkciók
// ===================================================================

/**
 * @brief Engedélyezi/letiltja a komponenst
 * @param enabled true = engedélyezett, false = letiltott
 */
void SpectrumAnalyzer::setEnabled(bool enabled) {
    this->enabled = enabled;
    if (!enabled) {
        // Reset peak értékek
        memset(peakLevels, 0, sizeof(peakLevels));
        memset(peakHoldTime, 0, sizeof(peakHoldTime));
    }
}

/**
 * @brief Színpaletta beállítása
 */
void SpectrumAnalyzer::setColors(uint16_t lowColor, uint16_t midColor, uint16_t highColor, uint16_t peakColor) {
    this->colorLow = lowColor;
    this->colorMid = midColor;
    this->colorHigh = highColor;
    this->colorPeak = peakColor;
}

// ===================================================================
// Belső segédfüggvények
// ===================================================================

/**
 * @brief dB érték magasságra konvertálása
 */
uint16_t SpectrumAnalyzer::dbToHeight(float dbValue) const {
    float normalized = (dbValue - DB_MIN) / DB_RANGE;
    normalized = std::max(0.0f, std::min(1.0f, normalized));
    return (uint16_t)(normalized * bounds.height);
}

/**
 * @brief Szín interpoláció szint alapján
 */
uint16_t SpectrumAnalyzer::getColorForLevel(float level) const {
    if (level < 0.33f) {
        // Alacsony szint - zöld
        return colorLow;
    } else if (level < 0.66f) {
        // Közepes szint - sárga
        return colorMid;
    } else {
        // Magas szint - piros
        return colorHigh;
    }
}

/**
 * @brief Spektrum adatok lekérése az AudioProcessor-ból
 */
bool SpectrumAnalyzer::updateSpectrumData() {
    // Ellenőrizzük, hogy van-e új adat
    if (!g_sharedAudioData.dataReady) {
        return false;
    }

    // Mutex védelem alatt másoljuk az adatokat
    if (mutex_try_enter(&g_sharedAudioData.dataMutex, nullptr)) {

        // Low-res spektrum adatok másolása
        for (uint16_t i = 0; i < AudioProcessorConstants::LOW_RES_BINS && i < barCount; i++) {
            currentLevels[i] = g_sharedAudioData.spectrum.lowResBins[i];
        }

        mutex_exit(&g_sharedAudioData.dataMutex);
        return true;
    }

    return false; // Nem sikerült mutex lock
}

/**
 * @brief Peak értékek frissítése és csökkenése
 */
void SpectrumAnalyzer::updatePeakHold(uint32_t deltaTime) {
    uint32_t currentTime = millis();

    for (uint16_t i = 0; i < barCount && i < AudioProcessorConstants::LOW_RES_BINS; i++) {

        // Ha az aktuális szint nagyobb mint a peak, frissítsük a peak-et
        if (currentLevels[i] > peakLevels[i]) {
            peakLevels[i] = currentLevels[i];
            peakHoldTime[i] = currentTime;
        } else {
            // Peak csökkenés idő alapján
            if (currentTime - peakHoldTime[i] > PEAK_HOLD_TIME) {
                peakLevels[i] *= PEAK_DECAY_RATE;

                // Minimális szint alatt nullázás
                if (peakLevels[i] < DB_MIN + 5.0f) {
                    peakLevels[i] = DB_MIN;
                }
            }
        }
    }
}

/**
 * @brief Egy bar megrajzolása
 */
void SpectrumAnalyzer::drawBar(TFT_eSPI &display, uint16_t barIndex, uint16_t x, float level, float peakLevel) {
    if (barWidth == 0)
        return;

    // Bar magasságok számítása
    uint16_t barHeight = (uint16_t)(level * bounds.height);
    uint16_t peakHeight = (uint16_t)(peakLevel * bounds.height);

    // Bar Y pozíciók (alulról felfelé rajzolunk)
    uint16_t barBottom = bounds.y + bounds.height;
    uint16_t barTop = barBottom - barHeight;
    uint16_t peakY = barBottom - peakHeight;

    // Fő bar rajzolása színátmenettel
    if (barHeight > 0) {
        uint16_t barColor = getColorForLevel(level);
        display.fillRect(x, barTop, barWidth, barHeight, barColor);
    }

    // Peak vonal rajzolása (2 pixel magas)
    if (peakHeight > 2 && peakLevel > 0.05f) { // Csak ha látható
        display.fillRect(x, peakY - 1, barWidth, 2, colorPeak);
    }

// Debug információ (opcionális)
#ifdef DEBUG_SPECTRUM_ANALYZER_VERBOSE
    if (barIndex % 4 == 0) { // Csak minden 4. bar-nál
        DEBUG("Bar %d: level=%.1f, peak=%.1f, height=%d, peakH=%d\n", barIndex, level, peakLevel, barHeight, peakHeight);
    }
#endif
}

/**
 * @brief Egy bar rajzolása sprite-ba
 * @param barIndex Bar indexe (0-tól kezdve)
 * @param x Sprite-beli X pozíció
 * @param level Normalizált szint (0.0-1.0)
 * @param peakLevel Normalizált peak szint (0.0-1.0)
 */
void SpectrumAnalyzer::drawBarToSprite(uint16_t barIndex, uint16_t x, float level, float peakLevel) {
    if (barWidth == 0 || sprite == nullptr)
        return;

    // Bar magasságok számítása
    uint16_t barHeight = (uint16_t)(level * bounds.height);
    uint16_t peakHeight = (uint16_t)(peakLevel * bounds.height);

    // Bar Y pozíciók (alulról felfelé rajzolunk, sprite-ban relatív koordináták)
    uint16_t barBottom = bounds.height;
    uint16_t barTop = barBottom - barHeight;
    uint16_t peakY = barBottom - peakHeight;

    // Fő bar rajzolása színátmenettel
    if (barHeight > 0) {
        uint16_t barColor = getColorForLevel(level);
        sprite->fillRect(x, barTop, barWidth, barHeight, barColor);
    }

    // Peak vonal rajzolása (2 pixel magas)
    if (peakHeight > 2 && peakLevel > 0.05f) { // Csak ha látható
        sprite->fillRect(x, peakY - 1, barWidth, 2, colorPeak);
    }
}

/**
 * @brief SpectrumAnalyzer destruktor - Sprite felszabadítása
 */
SpectrumAnalyzer::~SpectrumAnalyzer() { deleteSprite(); }

// ===================================================================
// Sprite kezelés
// ===================================================================

/**
 * @brief Sprite létrehozása villogás ellen
 */
void SpectrumAnalyzer::createSprite() {
    if (spriteCreated || bounds.width == 0 || bounds.height == 0) {
        return;
    }

    sprite = new TFT_eSprite(&::tft);
    // 16 bites színmód a memória takarítás érdekében
    sprite->setColorDepth(16);

    if (sprite->createSprite(bounds.width, bounds.height)) {
        spriteCreated = true;
        sprite->fillSprite(colorBackground);
        DEBUG("SpectrumAnalyzer: Sprite létrehozva %dx%d méretben (16 bit)\n", bounds.width, bounds.height);
    } else {
        delete sprite;
        sprite = nullptr;
        DEBUG("SpectrumAnalyzer: Sprite létrehozása sikertelen!\n");
    }
}

/**
 * @brief Sprite törlése
 */
void SpectrumAnalyzer::deleteSprite() {
    if (spriteCreated && sprite != nullptr) {
        sprite->deleteSprite();
        delete sprite;
        sprite = nullptr;
        spriteCreated = false;
        DEBUG("SpectrumAnalyzer: Sprite törölve\n");
    }
}

// ===================================================================
// UIComponent interface megvalósítás
// ===================================================================
