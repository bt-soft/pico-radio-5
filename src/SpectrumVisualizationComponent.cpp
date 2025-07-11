#include "SpectrumVisualizationComponent.h"
#include "AudioCore1Manager.h"
#include "Config.h"
#include "defines.h"
#include <cmath>
#include <vector>

// Színprofilok
namespace FftDisplayConstants {
const uint16_t colors0[16] = {0x0000, 0x000F, 0x001F, 0x081F, 0x0810, 0x0800, 0x0C00, 0x1C00, 0xFC00, 0xFDE0, 0xFFE0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; // Cold
const uint16_t colors1[16] = {0x0000, 0x1000, 0x2000, 0x4000, 0x8000, 0xC000, 0xF800, 0xF8A0, 0xF9C0, 0xFD20, 0xFFE0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; // Hot
}; // namespace FftDisplayConstants

// ===== ÉRZÉKENYSÉGI / AMPLITÚDÓ SKÁLÁZÁSI KONSTANSOK =====
// Minden grafikon mód érzékenységét és amplitúdó skálázását itt lehet módosítani
namespace SensitivityConstants {
// Spektrum módok (LowRes és HighRes) - magasabb érték = alacsonyabb érzékenység
constexpr float AMPLITUDE_SCALE = 120.0f; // Spektrum bar-ok amplitúdó skálázása

// Oszcilloszkóp mód - magasabb érték = nagyobb érzékenység
constexpr float OSCI_SENSITIVITY_FACTOR = 25.0f; // Oszcilloszkóp jel erősítése

// Envelope mód - alacsonyabb érték = kisebb amplitúdó
constexpr float ENVELOPE_INPUT_GAIN = 0.01f; // Envelope amplitúdó erősítése

// Waterfall mód - alacsonyabb érték = halványabb színek
constexpr float WATERFALL_INPUT_SCALE = 0.03f; // Waterfall intenzitás skálázása

// CW/RTTY hangolási segéd - alacsonyabb érték = halványabb színek
constexpr float TUNING_AID_INPUT_SCALE = 0.03f; // Hangolási segéd intenzitás skálázása
}; // namespace SensitivityConstants

// Analizátor konstansok
namespace AnalyzerConstants {
constexpr float ANALYZER_MIN_FREQ_HZ = 300.0f;
constexpr float ANALYZER_MAX_FREQ_HZ = 15000.0f;
constexpr uint16_t WATERFALL_TOP_Y = 20;        // A vízesés diagram tetejének Y koordinátája
constexpr uint16_t ANALYZER_BOTTOM_MARGIN = 20; // Alsó margó a skálának és a vízesésnek
}; // namespace AnalyzerConstants

/**
 * Waterfall színpaletta RGB565 formátumban
 */
const uint16_t SpectrumVisualizationComponent::WATERFALL_COLORS[16] = {0x0000, 0x000F, 0x001F, 0x081F, 0x0810, 0x0800, 0x0C00, 0x1C00, 0xFC00, 0xFDE0, 0xFFE0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

/**
 * @brief Konstruktor
 */
SpectrumVisualizationComponent::SpectrumVisualizationComponent(int x, int y, int w, int h, RadioMode radioMode)
    : UIComponent(Rect(x, y, w, h)), radioMode_(radioMode), currentMode_(DisplayMode::Off), lastRenderedMode_(DisplayMode::Off), modeIndicatorVisible_(false), modeIndicatorDrawn_(false), frequencyLabelsDrawn_(false),
      modeIndicatorHideTime_(0), lastTouchTime_(0), lastFrameTime_(0), maxDisplayFrequencyHz_(radioMode == RadioMode::AM ? MAX_DISPLAY_FREQUENCY_AM : MAX_DISPLAY_FREQUENCY_FM), envelopeLastSmoothedValue_(0.0f),
      sprite_(nullptr), spriteCreated_(false), indicatorFontHeight_(0), currentYAnalyzer_(0), pAudioProcessor_(nullptr), currentTuningAidType_(TuningAidType::CW_TUNING), currentTuningAidMinFreqHz_(0.0f),
      currentTuningAidMaxFreqHz_(0.0f) {

    // Core1 AudioManager használata helyett a helyi AudioProcessor
    // Az AudioProcessor most már a core1-en fut az AudioCore1Manager-en keresztül
    pAudioProcessor_ = nullptr; // Már nem használjuk a helyi AudioProcessor-t

    DEBUG("SpectrumVisualizationComponent: Inicializálva core1 audio feldolgozással.\n");

    // Peak detection buffer inicializálása
    memset(Rpeak_, 0, sizeof(Rpeak_));

    // Waterfall buffer inicializálása
    if (bounds.height > 0 && bounds.width > 0) {
        wabuf.resize(bounds.height, std::vector<uint8_t>(bounds.width, 0));
    }

    // Sprite inicializálása
    sprite_ = new TFT_eSprite(&tft);

    // Sprite előkészítése a kezdeti módhoz
    manageSpriteForMode(currentMode_);
}

/**
 * @brief Destruktor
 */
SpectrumVisualizationComponent::~SpectrumVisualizationComponent() {
    if (sprite_) {
        sprite_->deleteSprite();
        delete sprite_;
        sprite_ = nullptr;
    }

    // AudioProcessor már nem itt van felszabadítva, a core1-en fut
    pAudioProcessor_ = nullptr;
}

/**
 * @brief UIComponent draw implementáció
 */
void SpectrumVisualizationComponent::draw() {

    // FPS limitálás - maximum ~30 FPS a villogás csökkentéséhez
    uint32_t currentTime = millis();
    if (currentTime - lastFrameTime_ < 33) { // 33ms = ~30 FPS
        return;
    }
    lastFrameTime_ = currentTime;

    // Ha nincs processzor vagy a dialog aktív, ne rajzoljunk újra (kivéve ha force redraw van)
    // Ellenőrizzük, hogy a core1 audio manager fut-e és van-e aktív dialógus
    if (!AudioCore1Manager::isRunning() || iscurrentScreenDialogActive()) {
        return;
    }

    if (needBorderDrawn) {
        drawFrame();             // Rajzoljuk meg a keretet, ha szükséges
        needBorderDrawn = false; // Reset the flag after drawing
    }

    // Audio adatok már a core1-en feldolgozás alatt vannak
    // Itt csak lekérjük a feldolgozott adatokat

    // Biztonsági ellenőrzés: FM módban CW/RTTY módok nem engedélyezettek
    if (radioMode_ == RadioMode::FM && (currentMode_ == DisplayMode::CWWaterfall || currentMode_ == DisplayMode::RTTYWaterfall)) {
        currentMode_ = DisplayMode::Waterfall; // Automatikus váltás Waterfall módra
    }

    // Renderelés módjának megfelelően
    switch (currentMode_) {
        case DisplayMode::Off:
            renderOffMode();
            break;
        case DisplayMode::SpectrumLowRes:
            renderSpectrumLowRes();
            break;
        case DisplayMode::SpectrumHighRes:
            renderSpectrumHighRes();
            break;
        case DisplayMode::Oscilloscope:
            renderOscilloscope();
            break;
        case DisplayMode::Envelope:
            renderEnvelope();
            break;
        case DisplayMode::Waterfall:
            renderWaterfall();
            break;
        case DisplayMode::CWWaterfall:
            renderCWWaterfall();
            break;
        case DisplayMode::RTTYWaterfall:
            renderRTTYWaterfall();
            break;
    }

    // Mode indicator megjelenítése ha szükséges
    if (modeIndicatorVisible_ && millis() > modeIndicatorHideTime_) {
        modeIndicatorVisible_ = false;
        modeIndicatorDrawn_ = false; // Reset flag when hiding
        // Töröljük a területet ahol az indicator volt - KERET ALATT
        int indicatorH = 20;                       // fix magasság az indicator számára
        int indicatorY = bounds.y + bounds.height; // Közvetlenül a keret alatt
        tft.fillRect(bounds.x, indicatorY, bounds.width, indicatorH, TFT_BLACK);
    }

    // Only draw mode indicator once when it becomes visible
    if (modeIndicatorVisible_ && !modeIndicatorDrawn_) {
        renderModeIndicator();
        modeIndicatorDrawn_ = true; // Mark as drawn
    }

    lastRenderedMode_ = currentMode_;
}

/**
 * @brief Touch kezelés
 */
bool SpectrumVisualizationComponent::handleTouch(const TouchEvent &touch) {
    if (touch.pressed && isPointInside(touch.x, touch.y)) {
        lastTouchTime_ = millis();
        cycleThroughModes();
        return true;
    }
    return false;
}

/**
 * @brief Keret rajzolása
 */
void SpectrumVisualizationComponent::drawFrame() {

    // Belső terület kitöltése feketével - MINDEN border-től 1 pixellel beljebb
    // A külső keret bounds.x-1, bounds.y-1 pozícióban van, ezért a fillRect
    // bounds.x, bounds.y koordinátától kezdődik, de minden oldalon 1 pixellel kevesebb
    tft.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, TFT_BLACK);

    // Teljes külső keret rajzolása (1 pixel vastag, minden oldalon)
    tft.drawRect(bounds.x - 1, bounds.y - 2, bounds.width + 3, bounds.height + 2, TFT_WHITE);
}

/**
 * @brief Kezeli a sprite létrehozását/törlését a megadott módhoz
 * @param modeToPrepareFor Az a mód, amelyhez a sprite-ot elő kell készíteni.
 */
void SpectrumVisualizationComponent::manageSpriteForMode(DisplayMode modeToPrepareFor) {

    if (spriteCreated_) { // Ha létezik sprite egy korábbi módból
        sprite_->deleteSprite();
        spriteCreated_ = false;
    }

    // Sprite használata MINDEN módhoz (kivéve Off)
    if (modeToPrepareFor != DisplayMode::Off) {
        int graphH = getGraphHeight();
        if (bounds.width > 0 && graphH > 0) {
            sprite_->setColorDepth(16); // RGB565
            spriteCreated_ = sprite_->createSprite(bounds.width, graphH);
            if (spriteCreated_) {
                sprite_->fillSprite(TFT_BLACK); // Kezdeti törlés
            } else {
                DEBUG("SpectrumVisualizationComponent: Sprite creation failed for mode %d (w:%d, graphH:%d)\n", static_cast<int>(modeToPrepareFor), bounds.width, graphH);
            }
        }
    }

    // Teljes terület törlése mód váltáskor az előző grafikon eltávolításához
    if (modeToPrepareFor != lastRenderedMode_) {

        // Csak a belső területet töröljük, de az alsó bordert meghagyjuk
        tft.fillRect(bounds.x, bounds.y, bounds.width, bounds.height - 1, TFT_BLACK);

        // Frekvencia feliratok területének törlése - CSAK a component szélességében
        tft.fillRect(bounds.x, bounds.y + bounds.height + 1, bounds.width, 15, TFT_BLACK);

        // Sprite is törlése ha létezett
        if (spriteCreated_) {
            sprite_->fillSprite(TFT_BLACK);
        }
        // Flag resetelése mód váltáskor
        frequencyLabelsDrawn_ = false;

        // Envelope reset mód váltáskor
        if (modeToPrepareFor == DisplayMode::Envelope) {
            envelopeLastSmoothedValue_ = 0.0f; // Simított érték nullázása
            // Wabuf buffer teljes törlése hogy tiszta vonallal kezdődjön az envelope
            for (auto &row : wabuf) {
                std::fill(row.begin(), row.end(), 0);
            }
        }
    }
}

/**
 * @brief Grafikon magasságának számítása (teljes keret magasság)
 */
int SpectrumVisualizationComponent::getGraphHeight() const {
    return bounds.height - 1; // 1 pixel eltávolítása az alsó border megőrzéséhez
}

/**
 * @brief Indicator magasságának számítása
 */
int SpectrumVisualizationComponent::getIndicatorHeight() const { return modeIndicatorVisible_ ? 10 : 0; }

/**
 * @brief Hatékony magasság számítása (grafikon + indicator)
 */
int SpectrumVisualizationComponent::getEffectiveHeight() const {
    return bounds.height + getIndicatorHeight(); // Keret + indicator alatta
}

/**
 * @brief Módok közötti váltás
 */
void SpectrumVisualizationComponent::cycleThroughModes() {

    int nextMode = static_cast<int>(currentMode_) + 1;

    // FM módban kihagyjuk a CW és RTTY hangolási segéd módokat
    if (radioMode_ == RadioMode::FM) {
        if (nextMode == static_cast<int>(DisplayMode::CWWaterfall)) {
            nextMode = static_cast<int>(DisplayMode::Off); // Ugrás az Off módra
        } else if (nextMode == static_cast<int>(DisplayMode::RTTYWaterfall)) {
            nextMode = static_cast<int>(DisplayMode::Off); // Ugrás az Off módra
        } else if (nextMode > static_cast<int>(DisplayMode::Waterfall)) {
            nextMode = static_cast<int>(DisplayMode::Off);
        }
    } else {
        // AM módban minden mód elérhető
        if (nextMode > static_cast<int>(DisplayMode::RTTYWaterfall)) {
            nextMode = static_cast<int>(DisplayMode::Off);
        }
    }

    // Előző mód megőrzése a tisztításhoz
    lastRenderedMode_ = currentMode_;
    currentMode_ = static_cast<DisplayMode>(nextMode);

    // Optimális FFT méret beállítása az új módhoz
    // Core1 AudioManager használatával FFT méret beállítása
    if (AudioCore1Manager::isRunning()) {
        uint16_t optimalFftSize = getOptimalFftSizeForMode(currentMode_);
        uint16_t currentFftSize = getCore1FftSize();

        if (currentFftSize != optimalFftSize) {
            DEBUG("SpectrumVisualizationComponent: Changing FFT size from %d to %d for mode %d\n", currentFftSize, optimalFftSize, static_cast<int>(currentMode_));

            if (AudioCore1Manager::setFftSize(optimalFftSize)) {
                DEBUG("SpectrumVisualizationComponent: FFT size successfully changed to %d\n", optimalFftSize);
            } else {
                DEBUG("SpectrumVisualizationComponent: Failed to change FFT size to %d\n", optimalFftSize);
            }
        }
    }

    // Mode indicator megjelenítése 20 másodpercig
    modeIndicatorVisible_ = true;
    modeIndicatorDrawn_ = false;               // Reset a flag-et hogy azonnal megjelenjen
    needBorderDrawn = true;                    // Kényszerítjük a keret újrarajzolását
    modeIndicatorHideTime_ = millis() + 20000; // 20 másodpercig látható

    // Sprite előkészítése az új módhoz
    manageSpriteForMode(currentMode_);

    // Config mentése
    setCurrentModeToConfig();
}

/**
 * @brief Aktuális mód szöveges neve
 */
/**
 * @brief Kezdeti mód beállítása
 */
void SpectrumVisualizationComponent::setInitialMode(DisplayMode mode) {
    // FM módban CW/RTTY módok nem engedélyezettek
    if (radioMode_ == RadioMode::FM && (mode == DisplayMode::CWWaterfall || mode == DisplayMode::RTTYWaterfall)) {
        mode = DisplayMode::Waterfall; // Alapértelmezés FM módban
    }

    currentMode_ = mode;
    lastRenderedMode_ = DisplayMode::Off; // Kényszerítjük az újrarajzolást

    // Optimális FFT méret beállítása Core1-en
    if (AudioCore1Manager::isRunning()) {
        uint16_t optimalFftSize = getOptimalFftSizeForMode(currentMode_);
        uint16_t currentFftSize = getCore1FftSize();

        if (currentFftSize != optimalFftSize) {
            DEBUG("SpectrumVisualizationComponent: Setting initial FFT size to %d for mode %d\n", optimalFftSize, static_cast<int>(currentMode_));

            if (!AudioCore1Manager::setFftSize(optimalFftSize)) {
                DEBUG("SpectrumVisualizationComponent: Failed to set initial FFT size to %d\n", optimalFftSize);
            }
        }
    }

    // Sprite előkészítése az új módhoz
    manageSpriteForMode(currentMode_);
}

/**
 * @brief Mód betöltése config-ból
 */
void SpectrumVisualizationComponent::loadModeFromConfig() {
    // Config-ból betöltjük az aktuális rádió módnak megfelelő audio módot
    uint8_t configValue = (radioMode_ == RadioMode::AM) ? config.data.audioModeAM : config.data.audioModeFM;
    DisplayMode configMode = configValueToDisplayMode(configValue);

    // FM módban CW/RTTY módok nem engedélyezettek
    if (radioMode_ == RadioMode::FM && (configMode == DisplayMode::CWWaterfall || configMode == DisplayMode::RTTYWaterfall)) {
        configMode = DisplayMode::Waterfall; // Alapértelmezés FM módban
    }

    currentMode_ = configMode;

    // Optimális FFT méret beállítása Core1-en
    if (AudioCore1Manager::isRunning()) {
        uint16_t optimalFftSize = getOptimalFftSizeForMode(currentMode_);
        uint16_t currentFftSize = getCore1FftSize();

        if (currentFftSize != optimalFftSize) {
            DEBUG("SpectrumVisualizationComponent: Loading FFT size %d from config for mode %d\n", optimalFftSize, static_cast<int>(currentMode_));

            if (!AudioCore1Manager::setFftSize(optimalFftSize)) {
                DEBUG("SpectrumVisualizationComponent: Failed to load FFT size %d from config\n", optimalFftSize);
            }
        }
    }

    // Sprite előkészítése az új módhoz
    manageSpriteForMode(currentMode_);

    needBorderDrawn = true; // Kényszerítjük a keret újrarajzolását
}

/**
 * @brief Módkijelző láthatóságának beállítása
 */
void SpectrumVisualizationComponent::setModeIndicatorVisible(bool visible) {
    modeIndicatorVisible_ = visible;
    modeIndicatorDrawn_ = false; // Reset draw flag when visibility changes
    if (visible) {
        modeIndicatorHideTime_ = millis() + 20000; // 20 másodperc
    }
}

/**
 * @brief Ellenőrzi, hogy egy megjelenítési mód elérhető-e az aktuális rádió módban
 */
bool SpectrumVisualizationComponent::isModeAvailable(DisplayMode mode) const {
    // FM módban CW és RTTY hangolási segéd módok nem elérhetők
    if (radioMode_ == RadioMode::FM && (mode == DisplayMode::CWWaterfall || mode == DisplayMode::RTTYWaterfall)) {
        return false;
    }

    // Minden más mód minden rádió módban elérhető
    return true;
}

/**
 * @brief Off mód renderelése
 */
void SpectrumVisualizationComponent::renderOffMode() {

    if (lastRenderedMode_ == currentMode_) {
        return;
    }

    // Terület törlése, de alsó border meghagyása
    tft.fillRect(bounds.x, bounds.y, bounds.width, bounds.height - 1, TFT_BLACK);

    // OFF szöveg középre igazítása, nagyobb betűmérettel
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3);         // Még nagyobb betűméret
    tft.setTextDatum(MC_DATUM); // Középre igazítás (Middle Center)
    int textX = bounds.x + bounds.width / 2;
    int textY = bounds.y + (bounds.height - 1) / 2; // Pontos középre igazítás, figyelembe véve az alsó border-t
    tft.drawString("OFF", textX, textY);
}

/**
 * @brief Low resolution spektrum renderelése
 */
/**
 * @brief Low resolution spektrum renderelése (sprite-tal, javított amplitúdóval)
 */
void SpectrumVisualizationComponent::renderSpectrumLowRes() {
    // Audio feldolgozás Core1-en történik, AudioCore1Manager-en keresztül

    int graphH = getGraphHeight();
    if (!spriteCreated_ || bounds.width == 0 || graphH <= 0) {
        if (!spriteCreated_) {
            DEBUG("SpectrumVisualizationComponent::renderSpectrumLowRes - Sprite not created\n");
        }
        return;
    }

    // Ne töröljük a teljes sprite-ot minden frame-ben - csak az oszlopokat rajzoljuk újra

    int actual_low_res_peak_max_height = graphH - 1;
    constexpr int bar_gap_pixels = 1;
    constexpr int LOW_RES_BANDS = 24;
    int bands_to_display_on_screen = LOW_RES_BANDS;

    if (bounds.width < (bands_to_display_on_screen + (bands_to_display_on_screen - 1) * bar_gap_pixels)) {
        bands_to_display_on_screen = (bounds.width + bar_gap_pixels) / (1 + bar_gap_pixels);
    }
    if (bands_to_display_on_screen <= 0)
        bands_to_display_on_screen = 1;

    int dynamic_bar_width_pixels = 1;
    if (bands_to_display_on_screen > 0) {
        dynamic_bar_width_pixels = (bounds.width - (std::max(0, bands_to_display_on_screen - 1) * bar_gap_pixels)) / bands_to_display_on_screen;
    }
    if (dynamic_bar_width_pixels < 1)
        dynamic_bar_width_pixels = 1;

    int bar_total_width_pixels_dynamic = dynamic_bar_width_pixels + bar_gap_pixels;
    int total_drawn_width = (bands_to_display_on_screen * dynamic_bar_width_pixels) + (std::max(0, bands_to_display_on_screen - 1) * bar_gap_pixels);
    int x_offset = (bounds.width - total_drawn_width) / 2;

    // Lassabb peak ereszkedés: csak minden 3. hívásnál csökkentjük
    static uint8_t peak_fall_counter = 0;
    peak_fall_counter = (peak_fall_counter + 1) % 3;

    for (int band_idx = 0; band_idx < bands_to_display_on_screen; band_idx++) {
        if (peak_fall_counter == 0) {
            if (Rpeak_[band_idx] >= 1)
                Rpeak_[band_idx] -= 1;
        }
    }

    // Core1 spektrum adatok lekérése
    const double *magnitudeData = nullptr;
    uint16_t actualFftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
    float currentBinWidthHz = 0.0f;
    float currentAutoGain = 1.0f;

    bool dataAvailable = getCore1SpectrumData(&magnitudeData, &actualFftSize, &currentBinWidthHz, &currentAutoGain);

    if (!dataAvailable || currentBinWidthHz == 0) {
        // Ha nincs friss adat, használjuk a default értékeket
        currentBinWidthHz = (30000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
    }

    const int min_bin_idx_low_res = std::max(2, static_cast<int>(std::round(AnalyzerConstants::ANALYZER_MIN_FREQ_HZ / currentBinWidthHz)));
    const int max_bin_idx_low_res = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(maxDisplayFrequencyHz_ / currentBinWidthHz)));
    const int num_bins_in_low_res_range = std::max(1, max_bin_idx_low_res - min_bin_idx_low_res + 1);

    double band_magnitudes[LOW_RES_BANDS] = {0.0};

    if (!magnitudeData) {
        memset(band_magnitudes, 0, sizeof(band_magnitudes));
    } else {
        for (int i = min_bin_idx_low_res; i <= max_bin_idx_low_res; i++) {
            uint8_t band_idx = getBandVal(i, min_bin_idx_low_res, num_bins_in_low_res_range, LOW_RES_BANDS);
            if (band_idx < LOW_RES_BANDS) {
                band_magnitudes[band_idx] = std::max(band_magnitudes[band_idx], magnitudeData[i]);
            }
        }
    }

    // Sávok kirajzolása sprite-ra (javított amplitúdóval)
    for (int band_idx = 0; band_idx < bands_to_display_on_screen; band_idx++) {
        int x_pos_for_bar = x_offset + bar_total_width_pixels_dynamic * band_idx;

        // Előbb töröljük az oszlop területét (fekete háttér)
        sprite_->fillRect(x_pos_for_bar, 0, dynamic_bar_width_pixels, graphH, TFT_BLACK);

        // Javított magnitúdó skálázás
        double magnitude = band_magnitudes[band_idx];
        int dsize = static_cast<int>(magnitude / SensitivityConstants::AMPLITUDE_SCALE);
        dsize = constrain(dsize, 0, actual_low_res_peak_max_height);

        if (dsize > Rpeak_[band_idx] && band_idx < MAX_SPECTRUM_BANDS) {
            Rpeak_[band_idx] = dsize;
        }

        // Bar kirajzolása sprite-ra
        if (dsize > 0) {
            int y_start_bar = graphH - dsize;
            int bar_h_visual = dsize;
            if (y_start_bar < 0) {
                bar_h_visual -= (0 - y_start_bar);
                y_start_bar = 0;
            }
            if (bar_h_visual > 0) {
                sprite_->fillRect(x_pos_for_bar, y_start_bar, dynamic_bar_width_pixels, bar_h_visual, TFT_GREEN); // Zöld bar
            }
        }

        // Peak (csúcs) kirajzolása sprite-ra
        int peak = Rpeak_[band_idx];
        if (peak > 3) {
            int y_peak = graphH - peak;
            sprite_->fillRect(x_pos_for_bar, y_peak, dynamic_bar_width_pixels, 2, TFT_CYAN); // 2 pixel magas cyan csík
        }
    }

    // Sprite kirakása a képernyőre
    sprite_->pushSprite(bounds.x, bounds.y);
}

/**
 * @brief High resolution spektrum renderelése (sprite-tal, javított amplitúdóval)
 */
void SpectrumVisualizationComponent::renderSpectrumHighRes() {
    // Audio feldolgozás Core1-en történik, AudioCore1Manager-en keresztül

    int graphH = getGraphHeight();
    if (!spriteCreated_ || bounds.width == 0 || graphH <= 0) {
        if (!spriteCreated_) {
            DEBUG("SpectrumVisualizationComponent::renderSpectrumHighRes - Sprite not created\n");
        }
        return;
    }

    // Ne töröljük a teljes sprite-ot minden frame-ben - csak a vonalakat rajzoljuk újra

    // Core1 spektrum adatok lekérése
    const double *magnitudeData = nullptr;
    uint16_t actualFftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
    float currentBinWidthHz = 0.0f;
    float currentAutoGain = 1.0f;

    bool dataAvailable = getCore1SpectrumData(&magnitudeData, &actualFftSize, &currentBinWidthHz, &currentAutoGain);

    if (!dataAvailable || currentBinWidthHz == 0) {
        currentBinWidthHz = (30000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
    }
    const int min_bin_idx_for_display = std::max(2, static_cast<int>(std::round(AnalyzerConstants::ANALYZER_MIN_FREQ_HZ / currentBinWidthHz)));
    const int max_bin_idx_for_display = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(maxDisplayFrequencyHz_ / currentBinWidthHz)));
    const int num_bins_in_display_range = std::max(1, max_bin_idx_for_display - min_bin_idx_for_display + 1);

    if (!dataAvailable || !magnitudeData) {
        sprite_->pushSprite(bounds.x, bounds.y - 1);
        return;
    }

    for (int screen_pixel_x = 0; screen_pixel_x < bounds.width; ++screen_pixel_x) {
        int fft_bin_index;
        if (bounds.width == 1) {
            fft_bin_index = min_bin_idx_for_display;
        } else {
            float ratio = static_cast<float>(screen_pixel_x) / (bounds.width - 1);
            fft_bin_index = min_bin_idx_for_display + static_cast<int>(std::round(ratio * (num_bins_in_display_range - 1)));
        }
        fft_bin_index = constrain(fft_bin_index, 0, static_cast<int>(actualFftSize / 2 - 1));

        // Előbb töröljük a pixel oszlopot (fekete vonal)
        sprite_->drawFastVLine(screen_pixel_x, 0, graphH, TFT_BLACK);

        double magnitude = magnitudeData[fft_bin_index];
        // Amplitúdó skálázás
        int scaled_magnitude = static_cast<int>(magnitude / SensitivityConstants::AMPLITUDE_SCALE);
        scaled_magnitude = constrain(scaled_magnitude, 0, graphH - 1);

        if (scaled_magnitude > 0) {
            int y_bar_start = graphH - 1 - scaled_magnitude;
            int bar_actual_height = scaled_magnitude + 1;
            if (y_bar_start < 0) {
                bar_actual_height -= (0 - y_bar_start);
                y_bar_start = 0;
            }
            if (bar_actual_height > 0) {
                sprite_->drawFastVLine(screen_pixel_x, y_bar_start, bar_actual_height, TFT_SKYBLUE);
            }
        }
    }

    // Sprite kirakása a képernyőre
    sprite_->pushSprite(bounds.x, bounds.y);
}

/**
 * @brief Oszcilloszkóp renderelése
 */
void SpectrumVisualizationComponent::renderOscilloscope() {
    // Audio feldolgozás Core1-en történik, AudioCore1Manager-en keresztül

    int graphH = getGraphHeight();
    if (!spriteCreated_ || bounds.width == 0 || graphH <= 0) {
        if (!spriteCreated_) {
            DEBUG("SpectrumVisualizationComponent::renderOscilloscope - Sprite not created\n");
        }
        return;
    }

    // Sprite törlése
    sprite_->fillSprite(TFT_BLACK);

    // Core1 oszcilloszkóp adatok lekérése
    const int *osciData = nullptr;
    bool dataAvailable = getCore1OscilloscopeData(&osciData);

    if (!dataAvailable || !osciData) {
        sprite_->pushSprite(bounds.x, bounds.y);
        return;
    }

    // DC komponens számítása
    double sum_samples = 0.0;
    const int MAX_INTERNAL_WIDTH = AudioProcessorConstants::MAX_INTERNAL_WIDTH; // 320
    for (int k = 0; k < MAX_INTERNAL_WIDTH; ++k) {
        sum_samples += osciData[k];
    }
    double dc_offset_correction = (MAX_INTERNAL_WIDTH > 0 && osciData) ? sum_samples / MAX_INTERNAL_WIDTH : 2048.0;

    int actual_osci_samples_to_draw = bounds.width;
    // --- Érzékenységi faktor meghatározása ---
    // Az oszcilloszkóp mindig a manuális érzékenységi faktort használja,
    // mivel az osciSamples nyers ADC értékeket tartalmaz, függetlenül az FFT erősítési módjától.
    float current_sensitivity_factor = SensitivityConstants::OSCI_SENSITIVITY_FACTOR;

    int prev_x = -1, prev_y = -1, prev_sample_idx = -1;

    for (int i = 0; i < actual_osci_samples_to_draw; i++) {
        int num_available_samples = MAX_INTERNAL_WIDTH;
        if (num_available_samples == 0)
            continue; // Ha nincs minta, ne csináljunk semmit

        // Minták leképezése a rendelkezésre álló MAX_INTERNAL_WIDTH-ből a tényleges 'width'-re
        int sample_idx;
        if (actual_osci_samples_to_draw == 1) {
            sample_idx = 0;
        } else {
            sample_idx = (i * (num_available_samples - 1)) / (actual_osci_samples_to_draw - 1);
        }
        sample_idx = constrain(sample_idx, 0, num_available_samples - 1);

        int raw_sample = osciData[sample_idx];
        // ADC érték (0-4095) átalakítása a KISZÁMÍTOTT DC KÖZÉPPONTHOZ képest,
        // majd skálázás az OSCI_SENSITIVITY_FACTOR-ral és a grafikon magasságára
        double sample_deviation = (static_cast<double>(raw_sample) - dc_offset_correction);
        double gain_adjusted_deviation = sample_deviation * current_sensitivity_factor;

        // Skálázás a grafikon felére (mivel a jel a középvonal körül ingadozik)
        double scaled_y_deflection = gain_adjusted_deviation * (static_cast<double>(graphH) / 2.0 - 1.0) / 2048.0; // A 2048.0 itt a maximális elméleti ADC eltérésre skáláz

        int y_pos = graphH / 2 - static_cast<int>(round(scaled_y_deflection));
        y_pos = constrain(y_pos, 0, graphH - 1); // Korlátozás a grafikon területére
        int x_pos = i;

        if (prev_x != -1 && i > 0 && sample_idx != prev_sample_idx) {
            // Csak akkor rajzolunk vonalat, ha ténylegesen másik mintához jutottunk
            sprite_->drawLine(prev_x, prev_y, x_pos, y_pos, TFT_GREEN);
        } else if (prev_x == -1) {
            sprite_->drawPixel(x_pos, y_pos, TFT_GREEN); // Első pont kirajzolása
        }
        prev_x = x_pos;
        prev_y = y_pos;
        prev_sample_idx = sample_idx;
    }

    // Sprite kirakása a képernyőre
    sprite_->pushSprite(bounds.x, bounds.y);
}

/**
 * @brief Envelope renderelése
 */
void SpectrumVisualizationComponent::renderEnvelope() {
    // Audio feldolgozás Core1-en történik, AudioCore1Manager-en keresztül

    int graphH = getGraphHeight();
    if (!spriteCreated_ || bounds.width == 0 || graphH <= 0 || wabuf.empty() || wabuf[0].empty()) {
        if (!spriteCreated_) {
            DEBUG("SpectrumVisualizationComponent::renderEnvelope - Sprite not created\n");
        }
        return;
    }

    // Core1 spektrum adatok lekérése
    const double *magnitudeData = nullptr;
    uint16_t actualFftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
    float currentBinWidthHz = 0.0f;
    float currentAutoGain = 1.0f;

    bool dataAvailable = getCore1SpectrumData(&magnitudeData, &actualFftSize, &currentBinWidthHz, &currentAutoGain);

    if (!dataAvailable || currentBinWidthHz == 0)
        currentBinWidthHz = (30000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    // 1. Adatok eltolása balra a wabuf-ban
    for (int r = 0; r < bounds.height; ++r) { // Teljes bounds.height
        for (int c = 0; c < bounds.width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    const int min_bin_for_env = std::max(2, static_cast<int>(std::round(AnalyzerConstants::ANALYZER_MIN_FREQ_HZ / currentBinWidthHz)));
    const int max_bin_for_env = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(maxDisplayFrequencyHz_ / currentBinWidthHz)));
    const int num_bins_in_env_range = std::max(1, max_bin_for_env - min_bin_for_env + 1);

    // 2. Új adatok betöltése
    // Az Envelope módhoz az magnitudeData értékeit használjuk csökkentett erősítéssel.
    for (int r = 0; r < bounds.height; ++r) { // Teljes bounds.height
        // 'r' (0 to bounds.height-1) leképezése FFT bin indexre a szűkített tartományon belül
        int fft_bin_index = min_bin_for_env + static_cast<int>(std::round(static_cast<float>(r) / std::max(1, (bounds.height - 1)) * (num_bins_in_env_range - 1)));
        fft_bin_index = constrain(fft_bin_index, min_bin_for_env, max_bin_for_env);

        // Az AudioProcessor->getMagnitudeData()[fft_bin_index] már tartalmazza a csillapított értéket.
        // Alkalmazzuk a csökkentett ENVELOPE_INPUT_GAIN-t.
        double gained_val = magnitudeData[fft_bin_index] * SensitivityConstants::ENVELOPE_INPUT_GAIN;
        wabuf[r][bounds.width - 1] = static_cast<uint8_t>(constrain(gained_val, 0.0, 255.0)); // 0-255 közé korlátozzuk a wabuf számára
    }

    // 3. Sprite törlése és burkológörbe kirajzolása
    sprite_->fillSprite(TFT_BLACK); // Sprite törlése

    constexpr float ENVELOPE_SMOOTH_FACTOR = 0.25f;

    // Először rajzoljunk egy vékony központi vízszintes vonalat (alapvonal) - mindig látható
    int yCenter_on_sprite = graphH / 2;
    sprite_->drawFastHLine(0, yCenter_on_sprite, bounds.width, TFT_WHITE);

    for (int c = 0; c < bounds.width; ++c) {
        int max_val_in_col = 0;
        bool column_has_signal = false;

        for (int r_wabuf = 0; r_wabuf < bounds.height; ++r_wabuf) { // Teljes bounds.height
            if (wabuf[r_wabuf][c] > 0)
                column_has_signal = true;
            if (wabuf[r_wabuf][c] > max_val_in_col) {
                max_val_in_col = wabuf[r_wabuf][c];
            }
        }

        // A maximális amplitúdó simítása az oszlopban
        float current_col_max_amplitude = static_cast<float>(max_val_in_col);
        // A tagváltozót használjuk a simításhoz az oszlopok között
        envelopeLastSmoothedValue_ = ENVELOPE_SMOOTH_FACTOR * envelopeLastSmoothedValue_ + (1.0f - ENVELOPE_SMOOTH_FACTOR) * current_col_max_amplitude;

        // Csak akkor rajzolunk burkológörbét, ha van számottevő jel
        if (column_has_signal || envelopeLastSmoothedValue_ > 0.5f) {
            // A simított amplitúdó skálázása a grafikon magasságára (0-255 -> 0-graphH)
            float y_offset_float = (envelopeLastSmoothedValue_ / 255.0f) * (graphH / 2 - 1); // 0-255 amplitúdó a grafikon feléig

            int y_offset_pixels = static_cast<int>(round(y_offset_float));
            y_offset_pixels = std::min(y_offset_pixels, graphH / 2 - 1);
            if (y_offset_pixels < 0)
                y_offset_pixels = 0;

            if (y_offset_pixels > 0) { // Csak ha van érdemi eltérés a középvonaltól
                int yUpper_on_sprite = yCenter_on_sprite - y_offset_pixels;
                int yLower_on_sprite = yCenter_on_sprite + y_offset_pixels;

                yUpper_on_sprite = constrain(yUpper_on_sprite, 0, graphH - 1);
                yLower_on_sprite = constrain(yLower_on_sprite, 0, graphH - 1);

                if (yUpper_on_sprite <= yLower_on_sprite) {
                    sprite_->drawFastVLine(c, yUpper_on_sprite, yLower_on_sprite - yUpper_on_sprite + 1, TFT_WHITE);
                }
            }
        }
    }

    // Sprite kirakása a képernyőre
    sprite_->pushSprite(bounds.x, bounds.y);
}

/**
 * @brief Waterfall renderelése
 */
void SpectrumVisualizationComponent::renderWaterfall() {
    // Audio feldolgozás Core1-en történik, AudioCore1Manager-en keresztül

    int graphH = getGraphHeight();
    if (!spriteCreated_ || bounds.width == 0 || graphH <= 0 || wabuf.empty() || wabuf[0].empty()) {
        if (!spriteCreated_) {
            DEBUG("SpectrumVisualizationComponent::renderWaterfall - Sprite not created\n");
        }
        return;
    }

    // Core1 spektrum adatok lekérése
    const double *magnitudeData = nullptr;
    uint16_t actualFftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
    float currentBinWidthHz = 0.0f;
    float currentAutoGain = 1.0f;

    bool dataAvailable = getCore1SpectrumData(&magnitudeData, &actualFftSize, &currentBinWidthHz, &currentAutoGain);

    if (!dataAvailable || currentBinWidthHz == 0)
        currentBinWidthHz = (30000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    // 1. Adatok eltolása balra a wabuf-ban (ez továbbra is szükséges a wabuf frissítéséhez)
    for (int r = 0; r < bounds.height; ++r) { // A teljes bounds.height magasságon iterálunk a wabuf miatt
        for (int c = 0; c < bounds.width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    const int min_bin_for_wf = std::max(2, static_cast<int>(std::round(AnalyzerConstants::ANALYZER_MIN_FREQ_HZ / currentBinWidthHz)));
    const int max_bin_for_wf = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(maxDisplayFrequencyHz_ / currentBinWidthHz)));
    const int num_bins_in_wf_range = std::max(1, max_bin_for_wf - min_bin_for_wf + 1);

    // 2. Új adatok betöltése a wabuf jobb szélére (a wabuf továbbra is bounds.height magas)
    for (int r = 0; r < bounds.height; ++r) {
        // 'r' (0 to bounds.height-1) leképezése FFT bin indexre a szűkített tartományon belül
        int fft_bin_index = min_bin_for_wf + static_cast<int>(std::round(static_cast<float>(r) / std::max(1, (bounds.height - 1)) * (num_bins_in_wf_range - 1)));
        fft_bin_index = constrain(fft_bin_index, min_bin_for_wf, max_bin_for_wf);

        // Waterfall input scale
        wabuf[r][bounds.width - 1] = static_cast<uint8_t>(constrain(magnitudeData[fft_bin_index] * SensitivityConstants::WATERFALL_INPUT_SCALE, 0.0, 255.0));
    }

    // 3. Sprite görgetése és új oszlop kirajzolása
    sprite_->scroll(-1, 0); // Tartalom görgetése 1 pixellel balra

    // Az új (jobb szélső) oszlop kirajzolása a sprite-ra
    // A sprite graphH magas, a wabuf bounds.height magas.
    for (int r_wabuf = 0; r_wabuf < bounds.height; ++r_wabuf) {
        // r_wabuf (0..bounds.height-1) leképezése y_on_sprite-ra (0..graphH-1)
        // A vízesés "fentről lefelé" jelenik meg a képernyőn, de a wabuf sorai a frekvenciákat jelentik (alulról felfelé).
        // Tehát a wabuf r-edik sorát a sprite (graphH - 1 - r_scaled) pozíciójára kell rajzolni.
        int screen_y_relative_inverted = (r_wabuf * (graphH - 1)) / std::max(1, (bounds.height - 1));
        int y_on_sprite = (graphH - 1 - screen_y_relative_inverted); // Y koordináta a sprite-on belül

        if (y_on_sprite >= 0 && y_on_sprite < graphH) { // Biztosítjuk, hogy a sprite-on belül rajzolunk
            constexpr int WF_GRADIENT = 100;
            uint16_t color = valueToWaterfallColor(WF_GRADIENT * wabuf[r_wabuf][bounds.width - 1], 0.0f, 255.0f * WF_GRADIENT, 0); // Az új oszlop adata
            sprite_->drawPixel(bounds.width - 1, y_on_sprite, color);                                                              // Rajzolás a sprite jobb szélére
        }
    }

    // Sprite kirakása a képernyőre
    sprite_->pushSprite(bounds.x, bounds.y);
}

/**
 * @brief Mode indicator renderelése
 */
void SpectrumVisualizationComponent::renderModeIndicator() {
    if (!modeIndicatorVisible_)
        return;

    int indicatorH = getIndicatorHeight();
    if (indicatorH < 8)
        return;

    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Background black, this clears the previous
    tft.setTextDatum(BC_DATUM);              // Bottom-center alignment

    String modeText = "";
    switch (currentMode_) {
        case DisplayMode::Off:
            modeText = "Off";
            break;
        case DisplayMode::SpectrumLowRes:
            modeText = "FFT LowRes";
            break;
        case DisplayMode::SpectrumHighRes:
            modeText = "FFT HighRes";
            break;
        case DisplayMode::Oscilloscope:
            modeText = "Scope";
            break;
        case DisplayMode::Waterfall:
            modeText = "WaterFall";
            break;
        case DisplayMode::Envelope:
            modeText = "Envelope";
            break;
        case DisplayMode::CWWaterfall:
            modeText = "CW WaterFall";
            break;
        case DisplayMode::RTTYWaterfall:
            modeText = "RTTY WaterFall";
            break;
        default:
            modeText = "Unknown";
            break;
    }

    // Clear mode indicator area explicitly before text drawing - KERET ALATT
    int indicatorY = bounds.y + bounds.height; // Közvetlenül a keret alatt kezdődik
    tft.fillRect(bounds.x, indicatorY, bounds.width, indicatorH, TFT_BLACK);

    // Draw text at component bottom + indicator area, center
    // Y coordinate will be the text baseline (bottom of the indicator area)
    tft.drawString(modeText, bounds.x + bounds.width / 2, indicatorY + indicatorH);
}

/**
 * @brief Gradient oszlop rajzolása
 */
void SpectrumVisualizationComponent::drawGradientBar(int x, int y, int w, int h, float intensity) {
    uint16_t color = getSpectrumColor(intensity);
    tft.fillRect(x, y, w, h, color);
}

/**
 * @brief Spektrum szín meghatározása
 */
uint16_t SpectrumVisualizationComponent::getSpectrumColor(float intensity) {
    // Egyszerű színskála: kék -> zöld -> sárga -> piros
    if (intensity < 0.25f) {
        return interpolateColor(TFT_BLUE, TFT_CYAN, intensity * 4.0f);
    } else if (intensity < 0.5f) {
        return interpolateColor(TFT_CYAN, TFT_GREEN, (intensity - 0.25f) * 4.0f);
    } else if (intensity < 0.75f) {
        return interpolateColor(TFT_GREEN, TFT_YELLOW, (intensity - 0.5f) * 4.0f);
    } else {
        return interpolateColor(TFT_YELLOW, TFT_RED, (intensity - 0.75f) * 4.0f);
    }
}

/**
 * @brief Waterfall szín meghatározása
 */
uint16_t SpectrumVisualizationComponent::valueToWaterfallColor(float val, float min_val, float max_val, byte colorProfileIndex) {
    const uint16_t *colors = (colorProfileIndex == 0) ? FftDisplayConstants::colors0 : FftDisplayConstants::colors1;
    byte color_size = 16;

    if (val < min_val)
        val = min_val;
    if (val > max_val)
        val = max_val;

    int index = (int)((val - min_val) * (color_size - 1) / (max_val - min_val));
    if (index < 0)
        index = 0;
    if (index >= color_size)
        index = color_size - 1;

    return colors[index];
}

/**
 * @brief Színek interpolációja
 */
uint16_t SpectrumVisualizationComponent::interpolateColor(uint16_t color1, uint16_t color2, float ratio) {
    if (ratio <= 0.0f)
        return color1;
    if (ratio >= 1.0f)
        return color2;

    // RGB565 felbontása
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;

    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;

    // Interpoláció
    uint8_t r = r1 + (uint8_t)((r2 - r1) * ratio);
    uint8_t g = g1 + (uint8_t)((g2 - g1) * ratio);
    uint8_t b = b1 + (uint8_t)((b2 - b1) * ratio);

    // RGB565 összeállítása
    return (r << 11) | (g << 5) | b;
}

/**
 * @brief Config értékek konvertálása
 */
SpectrumVisualizationComponent::DisplayMode SpectrumVisualizationComponent::configValueToDisplayMode(uint8_t configValue) {
    if (configValue <= static_cast<uint8_t>(DisplayMode::RTTYWaterfall)) {
        return static_cast<DisplayMode>(configValue);
    }
    return DisplayMode::Off;
}

uint8_t SpectrumVisualizationComponent::displayModeToConfigValue(DisplayMode mode) { return static_cast<uint8_t>(mode); }

void SpectrumVisualizationComponent::setCurrentModeToConfig() {
    // Config-ba mentjük az aktuális audio módot a megfelelő rádió mód alapján
    uint8_t modeValue = displayModeToConfigValue(currentMode_);

    if (radioMode_ == RadioMode::AM) {
        config.data.audioModeAM = modeValue;
    } else if (radioMode_ == RadioMode::FM) {
        config.data.audioModeFM = modeValue;
    }

    // TODO: Itt kellene menteni az EEPROM-ba a config változásokat
    // config.save();
}

/**
 * @brief Beállítja a hangolási segéd típusát (CW vagy RTTY).
 * @param type A beállítandó TuningAidType.
 */
void SpectrumVisualizationComponent::setTuningAidType(TuningAidType type) {

    constexpr float CW_TUNING_AID_SPAN_HZ = 600.0f; // A config.data.cwReceiverOffsetHz +- 600 Hz körüli CW frekvencia a hangolássegéd sávszélessége
    constexpr float RTTY_TUNING_AID_SPAN_HZ = 200.0f;

    bool typeChanged = (currentTuningAidType_ != type);
    currentTuningAidType_ = type;

    if (currentMode_ == DisplayMode::CWWaterfall || currentMode_ == DisplayMode::RTTYWaterfall) {
        float oldMinFreq = currentTuningAidMinFreqHz_;
        float oldMaxFreq = currentTuningAidMaxFreqHz_;

        if (currentTuningAidType_ == TuningAidType::CW_TUNING) {

            // CW: 600 Hz span a CW offset frekvencia körül
            float centerFreq = config.data.cwReceiverOffsetHz;
            currentTuningAidMinFreqHz_ = centerFreq - CW_TUNING_AID_SPAN_HZ / 2.0f;
            currentTuningAidMaxFreqHz_ = centerFreq + CW_TUNING_AID_SPAN_HZ / 2.0f;

        } else if (currentTuningAidType_ == TuningAidType::RTTY_TUNING) {
            // RTTY: Mark és Space frekvenciák közötti terület + margó
            float f_mark = config.data.rttyMarkFrequencyHz;
            float f_space = f_mark - config.data.rttyShiftHz;

            // Bal/jobb margó
            float min_freq = std::min(f_mark, f_space) - RTTY_TUNING_AID_SPAN_HZ;
            float max_freq = std::max(f_mark, f_space) + RTTY_TUNING_AID_SPAN_HZ;

            currentTuningAidMinFreqHz_ = min_freq;
            currentTuningAidMaxFreqHz_ = max_freq;

        } else {
            // OFF_DECODER: alapértelmezett tartomány
            currentTuningAidMinFreqHz_ = 0.0f;
            currentTuningAidMaxFreqHz_ = maxDisplayFrequencyHz_;
        }

        // Ha változott a frekvencia tartomány, invalidáljuk a buffert
        if (typeChanged || oldMinFreq != currentTuningAidMinFreqHz_ || oldMaxFreq != currentTuningAidMaxFreqHz_) {
            // Waterfall buffer törlése
            for (auto &row : wabuf) {
                std::fill(row.begin(), row.end(), 0);
            }
        }
    }
}

/**
 * @brief CW Waterfall renderelése
 */
void SpectrumVisualizationComponent::renderCWWaterfall() {
    setTuningAidType(TuningAidType::CW_TUNING);
    renderTuningAid();
}

/**
 * @brief RTTY Waterfall renderelése
 */
void SpectrumVisualizationComponent::renderRTTYWaterfall() {
    setTuningAidType(TuningAidType::RTTY_TUNING);
    renderTuningAid();
}

/**
 * @brief Hangolási segéd renderelése (CW/RTTY waterfall)
 */
void SpectrumVisualizationComponent::renderTuningAid() {
    // Audio feldolgozás Core1-en történik, AudioCore1Manager-en keresztül

    int graphH = getGraphHeight();
    if (!spriteCreated_ || bounds.width == 0 || graphH <= 0 || wabuf.empty() || wabuf[0].empty()) {
        if (!spriteCreated_) {
            DEBUG("SpectrumVisualizationComponent::renderTuningAid - Sprite not created\n");
        }
        return;
    }

    // Core1 spektrum adatok lekérése
    const double *magnitudeData = nullptr;
    uint16_t actualFftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
    float currentBinWidthHz = 0.0f;
    float currentAutoGain = 1.0f;

    bool dataAvailable = getCore1SpectrumData(&magnitudeData, &actualFftSize, &currentBinWidthHz, &currentAutoGain);

    if (!dataAvailable || currentBinWidthHz == 0)
        currentBinWidthHz = (30000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    // 1. Adatok eltolása "lefelé" a wabuf-ban (időbeli léptetés)
    // Csak a grafikon magasságáig (graphH) használjuk a wabuf sorait.
    // A wabuf mérete (bounds.height x bounds.width), de itt csak graphH sort használunk fel a vízeséshez.
    for (int r = graphH - 1; r > 0; --r) {       // Utolsó sortól a másodikig
        for (int c = 0; c < bounds.width; ++c) { // Minden oszlop (frekvencia bin)
            wabuf[r][c] = wabuf[r - 1][c];
        }
    }

    const int min_fft_bin_for_tuning = std::max(1, static_cast<int>(std::round(currentTuningAidMinFreqHz_ / currentBinWidthHz)));
    const int max_fft_bin_for_tuning = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(currentTuningAidMaxFreqHz_ / currentBinWidthHz)));
    const int num_bins_in_tuning_range = std::max(1, max_fft_bin_for_tuning - min_fft_bin_for_tuning + 1);

    // 2. Új adatok betöltése a wabuf tetejére (első sor)
    for (int c = 0; c < bounds.width; ++c) {
        // Képernyő pixel X koordinátájának (c) leképezése FFT bin indexre
        float ratio_in_display_width = (bounds.width <= 1) ? 0.0f : (static_cast<float>(c) / (bounds.width - 1));
        int fft_bin_index = min_fft_bin_for_tuning + static_cast<int>(std::round(ratio_in_display_width * (num_bins_in_tuning_range - 1)));
        fft_bin_index = constrain(fft_bin_index, min_fft_bin_for_tuning, max_fft_bin_for_tuning);
        fft_bin_index = constrain(fft_bin_index, 2, static_cast<int>(actualFftSize / 2 - 1));

        // Hangolási segéd input scale (csökkentett érték a túlvezéreltség ellen)
        wabuf[0][c] = static_cast<uint8_t>(constrain(magnitudeData[fft_bin_index] * SensitivityConstants::TUNING_AID_INPUT_SCALE, 0.0, 255.0));
    }

    // 3. Sprite törlése és waterfall kirajzolása
    sprite_->fillSprite(TFT_BLACK); // Sprite törlése

    // Waterfall kirajzolása csak a graphH magasságig
    for (int r = 0; r < graphH; ++r) {
        for (int c = 0; c < bounds.width; ++c) {
            constexpr int WF_GRADIENT = 100;
            uint16_t color = valueToWaterfallColor(WF_GRADIENT * wabuf[r][c], 0.0f, 255.0f * WF_GRADIENT, 0);
            sprite_->drawPixel(c, r, color);
        }
    }

    // 4. Célfrekvencia vonalának kirajzolása a sprite-ra
    if (currentTuningAidType_ != TuningAidType::OFF_DECODER) {
        // Színek
        constexpr uint16_t TUNING_AID_TARGET_LINE_COLOR = TFT_GREEN;
        constexpr uint16_t TUNING_AID_RTTY_SPACE_LINE_COLOR = TFT_CYAN;
        constexpr uint16_t TUNING_AID_RTTY_MARK_LINE_COLOR = TFT_MAGENTA;

        float min_freq_displayed = currentTuningAidMinFreqHz_;
        float max_freq_displayed = currentTuningAidMaxFreqHz_;
        float displayed_span_hz = max_freq_displayed - min_freq_displayed;

        if (displayed_span_hz > 0) {
            if (currentTuningAidType_ == TuningAidType::CW_TUNING) {
                // CW célfrekvencia középre kerül
                int line_x = bounds.width / 2;
                sprite_->drawFastVLine(line_x, 0, graphH, TUNING_AID_TARGET_LINE_COLOR);

            } else if (currentTuningAidType_ == TuningAidType::RTTY_TUNING) {
                float f_mark = config.data.rttyMarkFrequencyHz;
                float f_space = f_mark - config.data.rttyShiftHz;

                // Space vonal
                if (f_space >= min_freq_displayed && f_space <= max_freq_displayed) {
                    float ratio_space = (f_space - min_freq_displayed) / displayed_span_hz;
                    int line_x_space = static_cast<int>(std::round(ratio_space * (bounds.width - 1)));
                    line_x_space = constrain(line_x_space, 0, bounds.width - 1);
                    sprite_->drawFastVLine(line_x_space, 0, graphH, TUNING_AID_RTTY_SPACE_LINE_COLOR);
                }

                // Mark vonal
                if (f_mark >= min_freq_displayed && f_mark <= max_freq_displayed) {
                    float ratio_mark = (f_mark - min_freq_displayed) / displayed_span_hz;
                    int line_x_mark = static_cast<int>(std::round(ratio_mark * (bounds.width - 1)));
                    line_x_mark = constrain(line_x_mark, 0, bounds.width - 1);
                    sprite_->drawFastVLine(line_x_mark, 0, graphH, TUNING_AID_RTTY_MARK_LINE_COLOR);
                }
            }
        }
    }

    // Sprite kirakása a képernyőre
    sprite_->pushSprite(bounds.x, bounds.y);

    // Frekvencia címkék kirajzolása közvetlenül a TFT-re (sprite kívül) - minden rendereléskor
    if (currentTuningAidType_ != TuningAidType::OFF_DECODER) {
        float min_freq_displayed = currentTuningAidMinFreqHz_;
        float max_freq_displayed = currentTuningAidMaxFreqHz_;
        float displayed_span_hz = max_freq_displayed - min_freq_displayed;

        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextDatum(BC_DATUM);

        if (displayed_span_hz > 0) {
            if (currentTuningAidType_ == TuningAidType::CW_TUNING) {
                // CW frekvencia címke - törli a területet és biztosan látható pozícióban
                int line_x = bounds.x + bounds.width / 2;
                int label_y = bounds.y + bounds.height + 5;

                // Terület törlése a felirat körül
                tft.fillRect(line_x - 25, label_y - 8, 50, 10, TFT_BLACK);

                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.drawString(String(static_cast<int>(config.data.cwReceiverOffsetHz)) + "Hz", line_x, label_y);

            } else if (currentTuningAidType_ == TuningAidType::RTTY_TUNING) {
                float f_mark = config.data.rttyMarkFrequencyHz;
                float f_space = f_mark - config.data.rttyShiftHz;

                // Space címke
                if (f_space >= min_freq_displayed && f_space <= max_freq_displayed) {
                    float ratio_space = (f_space - min_freq_displayed) / displayed_span_hz;
                    int line_x_space = bounds.x + static_cast<int>(std::round(ratio_space * (bounds.width - 1)));
                    line_x_space = constrain(line_x_space, bounds.x, bounds.x + bounds.width - 1);
                    int label_y = bounds.y + bounds.height + 5;

                    // Terület törlése a felirat körül
                    tft.fillRect(line_x_space - 25, label_y - 8, 50, 10, TFT_BLACK);

                    tft.setTextColor(TFT_CYAN, TFT_BLACK);
                    tft.drawString(String(static_cast<int>(round(f_space))) + "Hz", line_x_space, label_y);
                }

                // Mark címke
                if (f_mark >= min_freq_displayed && f_mark <= max_freq_displayed) {
                    float ratio_mark = (f_mark - min_freq_displayed) / displayed_span_hz;
                    int line_x_mark = bounds.x + static_cast<int>(std::round(ratio_mark * (bounds.width - 1)));
                    line_x_mark = constrain(line_x_mark, bounds.x, bounds.x + bounds.width - 1);
                    int label_y = bounds.y + bounds.height + 5;

                    // Terület törlése a felirat körül
                    tft.fillRect(line_x_mark - 25, label_y - 8, 50, 10, TFT_BLACK);

                    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
                    tft.drawString(String(static_cast<int>(round(f_mark))) + "Hz", line_x_mark, label_y);
                }
            }
        }
    }
}

/**
 * @brief Meghatározza, hogy egy adott FFT bin melyik alacsony felbontású sávhoz tartozik.
 * @param fft_bin_index Az FFT bin indexe.
 * @param min_bin_low_res A LowRes spektrumhoz figyelembe vett legalacsonyabb FFT bin index.
 * @param num_bins_low_res_range A LowRes spektrumhoz figyelembe vett FFT bin-ek száma.
 * @param total_bands A frekvenciasávok száma.
 * @return A sáv indexe (0-tól total_bands-1-ig).
 */
uint8_t SpectrumVisualizationComponent::getBandVal(int fft_bin_index, int min_bin_low_res, int num_bins_low_res_range, int total_bands) {
    if (fft_bin_index < min_bin_low_res || num_bins_low_res_range <= 0) {
        return 0; // Vagy egy érvénytelen sáv index, ha szükséges
    }
    // Kiszámítjuk a relatív indexet a megadott tartományon belül
    int relative_bin_index = fft_bin_index - min_bin_low_res;
    if (relative_bin_index < 0)
        return 0; // Elvileg nem fordulhat elő, ha fft_bin_index >= min_bin_low_res

    // Leképezzük a relatív bin indexet (0-tól num_bins_low_res_range-1-ig) a total_bands sávokra
    return constrain(relative_bin_index * total_bands / num_bins_low_res_range, 0, total_bands - 1);
}

/**
 * @brief Kirajzol egyetlen oszlopot/sávot (bar-t) az alacsony felbontású spektrumhoz.
 * @param band_idx A frekvenciasáv indexe, amelyhez az oszlop tartozik.
 * @param magnitude A sáv magnitúdója (double).
 * @param actual_start_x_on_screen A spektrum rajzolásának kezdő X koordinátája a képernyőn.
 * @param peak_max_height_for_mode A sáv maximális magassága az adott módban.
 * @param current_bar_width_pixels Az aktuális sávszélesség pixelekben.
 */
void SpectrumVisualizationComponent::drawSpectrumBar(int band_idx, double magnitude, int actual_start_x_on_screen, int peak_max_height_for_mode, int current_bar_width_pixels) {
    int graphH = bounds.height;
    if (graphH <= 0)
        return;

    int dsize = static_cast<int>(magnitude / SensitivityConstants::AMPLITUDE_SCALE);
    dsize = constrain(dsize, 0, peak_max_height_for_mode);
    constexpr int bar_gap_pixels = 1;
    int bar_total_width_pixels_dynamic = current_bar_width_pixels + bar_gap_pixels;
    int xPos = actual_start_x_on_screen + bar_total_width_pixels_dynamic * band_idx;

    if (xPos + current_bar_width_pixels > bounds.x + bounds.width || xPos < bounds.x)
        return;

    if (dsize > 0) {
        int y_start_bar = bounds.y + graphH - dsize;
        int bar_h_visual = dsize;
        if (y_start_bar < bounds.y) {
            bar_h_visual -= (bounds.y - y_start_bar);
            y_start_bar = bounds.y;
        }
        if (bar_h_visual > 0) {
            tft.fillRect(xPos, y_start_bar, current_bar_width_pixels, bar_h_visual, TFT_GREEN); // Zöld bar
        }
    }

    if (dsize > Rpeak_[band_idx] && band_idx < MAX_SPECTRUM_BANDS) {
        Rpeak_[band_idx] = dsize;
    }
    // Ha a peak érték 0-ra csökkent, töröljük (biztosítjuk, hogy ne jelenjen meg)
    if (band_idx < MAX_SPECTRUM_BANDS && Rpeak_[band_idx] < 1) {
        Rpeak_[band_idx] = 0;
    }
}

/**
 * @brief Optimal FFT méret meghatározása a megjelenítési módhoz
 */
uint16_t SpectrumVisualizationComponent::getOptimalFftSizeForMode(DisplayMode mode) const {
    switch (mode) {
        case DisplayMode::Waterfall:
        case DisplayMode::CWWaterfall:
        case DisplayMode::RTTYWaterfall:
            return 2048; // 2x jobb felbontás a hangolási segédhez

        case DisplayMode::SpectrumHighRes:
            return 1024; // Maximum felbontás a spektrum analizáláshoz

        case DisplayMode::SpectrumLowRes:
        case DisplayMode::Oscilloscope:
        case DisplayMode::Envelope:
        case DisplayMode::Off:
        default:
            return 512; // Alapértelmezett - gyorsabb
    }
}

/**
 * @brief Core1 spektrum adatok lekérése
 */
bool SpectrumVisualizationComponent::getCore1SpectrumData(const double **outData, uint16_t *outSize, float *outBinWidth, float *outAutoGain) {
    return AudioCore1Manager::getSpectrumData(outData, outSize, outBinWidth, outAutoGain);
}

/**
 * @brief Core1 oszcilloszkóp adatok lekérése
 */
bool SpectrumVisualizationComponent::getCore1OscilloscopeData(const int **outData) { return AudioCore1Manager::getOscilloscopeData(outData); }

/**
 * @brief Core1 bin szélesség lekérése (ha nincs friss adat, cached érték)
 */
float SpectrumVisualizationComponent::getCore1BinWidthHz() {
    const double *dummyData;
    uint16_t dummySize;
    float binWidth = 0.0f;
    float dummyGain;

    // Csak a bin width-et kérdezzük le, ha van friss adat
    if (AudioCore1Manager::getSpectrumData(&dummyData, &dummySize, &binWidth, &dummyGain)) {
        return binWidth;
    }

    // Ha nincs friss adat, visszaadunk egy becsült értéket
    return AudioProcessorConstants::DEFAULT_SAMPLING_FREQUENCY / 512.0f; // Default estimate
}

/**
 * @brief Core1 FFT méret lekérése (ha nincs friss adat, cached érték)
 */
uint16_t SpectrumVisualizationComponent::getCore1FftSize() {
    const double *dummyData;
    uint16_t fftSize = 512; // Default
    float dummyBinWidth, dummyGain;

    // Csak az FFT méretet kérdezzük le, ha van friss adat
    if (AudioCore1Manager::getSpectrumData(&dummyData, &fftSize, &dummyBinWidth, &dummyGain)) {
        return fftSize;
    }

    // Ha nincs friss adat, visszaadunk egy alapértelmezett értéket
    return AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
}
