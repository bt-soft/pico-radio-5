#include "SpectrumVisualizationComponent.h"
#include "AudioCore1Manager.h"
#include "Config.h"
#include "defines.h"
#include <cmath>
#include <cstring>
#include <vector>

// Színprofilok
namespace FftDisplayConstants {
const uint16_t colors0[16] = {0x0000, 0x000F, 0x001F, 0x081F, 0x0810, 0x0800, 0x0C00, 0x1C00, 0xFC00, 0xFDE0, 0xFFE0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; // Cold
const uint16_t colors1[16] = {0x0000, 0x1000, 0x2000, 0x4000, 0x8000, 0xC000, 0xF800, 0xF8A0, 0xF9C0, 0xFD20, 0xFFE0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; // Hot

constexpr uint16_t MODE_INDICATOR_VISIBLE_TIMEOUT_MS = 10 * 1000; // x másodperc láthatóság
constexpr uint8_t SPECTRUM_FPS = 20;                              // FPS limitálás konstans, ez még élvezhető fizualizációt ad, maradjon így 20 FPS-en

}; // namespace FftDisplayConstants

// ===== ÉRZÉKENYSÉGI / AMPLITÚDÓ SKÁLÁZÁSI KONSTANSOK =====
// Minden grafikon mód érzékenységét és amplitúdó skálázását itt lehet módosítani
// EGYSÉGES LOGIKA: nagyobb érték = nagyobb érzékenység (minden módnál)
namespace SensitivityConstants {
// Spektrum módok (LowRes és HighRes) - nagyobb érték = nagyobb érzékenység
constexpr float AMPLITUDE_SCALE = 0.8f; // Spektrum bar-ok amplitúdó skálázása

// Oszcilloszkóp mód - nagyobb érték = nagyobb érzékenység
constexpr float OSCI_SENSITIVITY_FACTOR = 25.0f; // Oszcilloszkóp jel erősítése

// Envelope mód - nagyobb érték = nagyobb amplitúdó
constexpr float ENVELOPE_INPUT_GAIN = 0.15f; // Envelope amplitúdó erősítése

// Waterfall mód - nagyobb érték = élénkebb színek
constexpr float WATERFALL_INPUT_SCALE = 8.0f; // Waterfall intenzitás skálázása

// CW/RTTY hangolási segéd - nagyobb érték = élénkebb színek
constexpr float TUNING_AID_INPUT_SCALE = 3.0f; // Hangolási segéd intenzitás skálázása
}; // namespace SensitivityConstants

// Analizátor konstansok
namespace AnalyzerConstants {
constexpr uint16_t ANALYZER_MIN_FREQ_HZ = 300;
}; // namespace AnalyzerConstants

/**
 * @brief Konstruktor
 */
SpectrumVisualizationComponent::SpectrumVisualizationComponent(int x, int y, int w, int h, RadioMode radioMode)
    : UIComponent(Rect(x, y, w, h)),                   //
      radioMode_(radioMode),                           //
      currentMode_(DisplayMode::Off),                  //
      lastRenderedMode_(DisplayMode::Off),             //
      modeIndicatorVisible_(false),                    //
      modeIndicatorDrawn_(false),                      //
      frequencyLabelsDrawn_(false),                    //
      modeIndicatorHideTime_(0),                       //
      lastTouchTime_(0),                               //
      lastFrameTime_(0),                               //
      envelopeLastSmoothedValue_(0.0f),                //
      frameHistoryIndex_(0),                           //
      frameHistoryFull_(false),                        //
      adaptiveGainFactor_(0.02f),                      //
      lastGainUpdateTime_(0),                          //
      sprite_(nullptr),                                //
      spriteCreated_(false),                           //
      indicatorFontHeight_(0),                         //
      currentTuningAidType_(TuningAidType::CW_TUNING), //
      currentTuningAidMinFreqHz_(0.0f),                //
      currentTuningAidMaxFreqHz_(0.0f),                //
      isMutedDrawn(false) {

    maxDisplayFrequencyHz_ = radioMode_ == RadioMode::AM ? SpectrumVisualizationComponent::MAX_DISPLAY_FREQUENCY_AM : SpectrumVisualizationComponent::MAX_DISPLAY_FREQUENCY_FM;

    // Peak detection buffer inicializálása
    memset(Rpeak_, 0, sizeof(Rpeak_));

    // Frame history buffer inicializálása
    for (int i = 0; i < FRAME_HISTORY_SIZE; i++) {
        frameMaxHistory_[i] = 0.0f;
    }

    // Waterfall buffer inicializálása
    if (bounds.height > 0 && bounds.width > 0) {
        wabuf.resize(bounds.height, std::vector<uint8_t>(bounds.width, 0));
    }

    // Sprite inicializálása
    sprite_ = new TFT_eSprite(&tft);

    // Sprite előkészítése a kezdeti módhoz
    manageSpriteForMode(currentMode_);

    // Indítsuk el a módok megjelenítését
    startShowModeIndicator();

    decoderManager = new DigitalDecoderManager(AudioProcessorConstants::DEFAULT_AM_SAMPLING_FREQUENCY, 2048);
    decoderManager->setMode(DigitalDecoderManager::Mode::CW);
    decoderManager->setCwParams(config.data.cwReceiverOffsetHz);
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

    if (decoderManager) {
        delete decoderManager;
        decoderManager = nullptr;
    }
}

/**
 * @brief Frame-alapú adaptív autogain frissítése
 * @param currentFrameMaxValue Jelenlegi frame maximális értéke
 */
void SpectrumVisualizationComponent::updateFrameBasedGain(float currentFrameMaxValue) {
    uint32_t now = millis();

    // Frame maximum érték hozzáadása a history bufferhez
    frameMaxHistory_[frameHistoryIndex_] = currentFrameMaxValue;
    frameHistoryIndex_ = (frameHistoryIndex_ + 1) % FRAME_HISTORY_SIZE;

    // Jelöljük, hogy már van elég adatunk
    if (frameHistoryIndex_ == 0) {
        frameHistoryFull_ = true;
    }

    // Rendszeres gain frissítés (gyakrabban, mint régen)
    if (now - lastGainUpdateTime_ > GAIN_UPDATE_INTERVAL_MS && frameHistoryFull_) {
        float averageFrameMax = getAverageFrameMax();

        if (averageFrameMax > MIN_SIGNAL_THRESHOLD) {
            int graphH = getGraphHeight();
            float targetMaxHeight = graphH * TARGET_MAX_UTILIZATION; // 75% a grafikon magasságából
            float idealGain = targetMaxHeight / averageFrameMax;

            // Simított átmenet az új gain faktorhoz
            adaptiveGainFactor_ = GAIN_SMOOTH_FACTOR * idealGain + (1.0f - GAIN_SMOOTH_FACTOR) * adaptiveGainFactor_;

            // Biztonsági korlátok - alacsonyabb minimum az érzékenyebb jelekhez
            adaptiveGainFactor_ = constrain(adaptiveGainFactor_, 0.001f, 5.0f);
        }

        lastGainUpdateTime_ = now;
    }
}

/**
 * @brief Átlagos frame maximum kiszámítása
 * @return Az utolsó FRAME_HISTORY_SIZE frame átlagos maximuma
 */
float SpectrumVisualizationComponent::getAverageFrameMax() const {
    if (!frameHistoryFull_) {
        // Ha még nincs elég adat, használjunk egy magas értéket,
        // hogy a gain alacsony maradjon a túlvezérlés elkerülésére
        return 5000.0f; // Feltételezzük, hogy ez egy tipikus magas érték
    }

    float sum = 0.0f;
    for (int i = 0; i < FRAME_HISTORY_SIZE; i++) {
        sum += frameMaxHistory_[i];
    }
    return sum / FRAME_HISTORY_SIZE;
}

/**
 * @brief Adaptív skálázási faktor lekérése
 * @param baseConstant Alap konstans érték
 * @return Adaptív skálázási faktor
 */
float SpectrumVisualizationComponent::getAdaptiveScale(float baseConstant) {
    // Auto gain módban az adaptív faktort használjuk
    if (isAutoGainMode()) {
        return baseConstant * adaptiveGainFactor_;
    }
    // Manual módban az eredeti konstanst használjuk
    return baseConstant;
}

/**
 * @brief Adaptív autogain reset
 */
void SpectrumVisualizationComponent::resetAdaptiveGain() {
    adaptiveGainFactor_ = 0.02f; // Alacsony kezdeti érték a túlvezérlés elkerülésére
    frameHistoryIndex_ = 0;
    frameHistoryFull_ = false;
    lastGainUpdateTime_ = millis();

    // Frame history buffer resetelése
    for (int i = 0; i < FRAME_HISTORY_SIZE; i++) {
        frameMaxHistory_[i] = 0.0f;
    }
}

/**
 * @brief Ellenőrzi, hogy auto gain módban vagyunk-e
 * @return True, ha auto gain mód aktív
 */
bool SpectrumVisualizationComponent::isAutoGainMode() {
    float currentConfig = (radioMode_ == RadioMode::AM) ? config.data.audioFftConfigAm : config.data.audioFftConfigFm;
    return (currentConfig == 0.0f); // 0.0f = Auto Gain
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

/**
 * @brief DisplayMode konvertálása config értékre
 */
uint8_t SpectrumVisualizationComponent::displayModeToConfigValue(DisplayMode mode) {
    //
    return static_cast<uint8_t>(mode);
}

/**
 * @brief Beállítja az aktuális audio módot a megfelelő rádió mód alapján.
 *
 */
void SpectrumVisualizationComponent::setCurrentModeToConfig() {

    // Config-ba mentjük az aktuális audio módot a megfelelő rádió mód alapján
    uint8_t modeValue = displayModeToConfigValue(currentMode_);

    if (radioMode_ == RadioMode::AM) {
        config.data.audioModeAM = modeValue;
    } else if (radioMode_ == RadioMode::FM) {
        config.data.audioModeFM = modeValue;
    }
}

/**
 * Waterfall színpaletta RGB565 formátumban
 */
const uint16_t SpectrumVisualizationComponent::WATERFALL_COLORS[16] = {0x0000, 0x000F, 0x001F, 0x081F, 0x0810, 0x0800, 0x0C00, 0x1C00, 0xFC00, 0xFDE0, 0xFFE0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

/**
 * @brief UIComponent draw implementáció
 */
void SpectrumVisualizationComponent::draw() {

    // FPS limitálás - az FPS értéke makróval állítható
    constexpr uint32_t FRAME_TIME_MS = 1000 / FftDisplayConstants::SPECTRUM_FPS;
    uint32_t currentTime = millis();
    if (currentTime - lastFrameTime_ < FRAME_TIME_MS) { // FPS limit
        return;
    }
    lastFrameTime_ = currentTime;

    // Ha Mute állapotban vagyunk
    if (rtv::muteStat) {
        if (!isMutedDrawn) {
            drawFrame();
            drawMutedMessage();
        }
        return;

    } else if (!rtv::muteStat && isMutedDrawn) {
        isMutedDrawn = false;
        needBorderDrawn = true; // Muted állapot megszűnt, rajzoljuk újra a keretet
    }

    // Ha nincs processzor vagy a dialog aktív, ne rajzoljunk újra (kivéve ha force redraw van)
    // Ellenőrizzük, hogy a core1 audio manager fut-e és van-e aktív dialógus
    if (!AudioCore1Manager::isRunning() || iscurrentScreenDialogActive()) {
        return;
    }

    if (needBorderDrawn) {
        drawFrame();             // Rajzoljuk meg a keretet, ha szükséges
        needBorderDrawn = false; // Reset the flag after drawing
    }

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
        case DisplayMode::RTTYWaterfall:
            renderCwOrRttyTuningAid();
            break;
    }

    // Mode indicator megjelenítése ha szükséges
    if (modeIndicatorVisible_ && !modeIndicatorDrawn_) {
        renderModeIndicator();
        modeIndicatorDrawn_ = true; // Mark as drawn
    }

    // A mode indicator időzített eltüntetése
    if (modeIndicatorVisible_ && millis() > modeIndicatorHideTime_) {
        modeIndicatorVisible_ = false;
        modeIndicatorDrawn_ = false;

        // Töröljük a területet ahol az indicator volt - KERET ALATT
        int indicatorH = 20;                       // fix magasság az indicator számára
        int indicatorY = bounds.y + bounds.height; // Közvetlenül a keret alatt
        tft.fillRect(bounds.x - 3, indicatorY, bounds.width + 3, indicatorH, TFT_BLACK);

        // Frekvencia feliratok kirajzolásának engedélyezése
        frequencyLabelsDrawn_ = true;
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
    tft.drawRect(bounds.x - 1, bounds.y - 2, bounds.width + 3, bounds.height + 2, TFT_DARKGREY);
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
                DEBUG("SpectrumVisualizationComponent: Sprite létrehozása sikertelen, mód: %s (szélesség:%d, grafikon magasság:%d)\n", static_cast<int>(modeToPrepareFor), bounds.width, graphH);
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
 * @brief FFT paraméterek beállítása az aktuális módhoz
 */
void SpectrumVisualizationComponent::setFftParametersForDisplayMode() {

    // Optimális FFT méret beállítása az új módhoz
    // Core1 AudioManager használatával FFT méret beállítása
    if (AudioCore1Manager::isRunning()) {

        { // FFT méret beállítása
            uint16_t optimalFftSize = getOptimalFftSizeForMode(currentMode_);
            uint16_t currentFftSize = getCore1FftSize();

            if (currentFftSize != optimalFftSize) {
                DEBUG("SpectrumVisualizationComponent: FFT méret változtatása: %d -> %d, mód: %s\n", currentFftSize, optimalFftSize, decodeModeToStr());

                if (AudioCore1Manager::setFftSize(optimalFftSize)) {
                    DEBUG("SpectrumVisualizationComponent: Az FFT méret sikeresen megváltozott: %d\n", optimalFftSize);
                } else {
                    DEBUG("SpectrumVisualizationComponent: Nem sikerült megváltoztatni az FFT méretet: %d\n", optimalFftSize);
                }
            } else {
                DEBUG("SpectrumVisualizationComponent: Az FFT méret már megfelelő: %d, mód: %s\n", currentFftSize, decodeModeToStr());
            }
        }

        // Oszcilloszkóp mód esetén engedélyezzük az oszcilloszkóp minták gyűjtését
        AudioCore1Manager::setCollectOsci(currentMode_ == DisplayMode::Oscilloscope);

        if (currentMode_ == DisplayMode::CWWaterfall) {
            // Alapértelmezett módok esetén a CW dekóder használata
            setTuningAidType(TuningAidType::CW_TUNING);
        } else if (currentMode_ == DisplayMode::RTTYWaterfall) {
            // RTTY waterfall esetén a CW dekóder nem szükséges
            setTuningAidType(TuningAidType::RTTY_TUNING);
        }
    }
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

    // FFT paraméterek beállítása az új módhoz
    setFftParametersForDisplayMode();

    // Mód indikátor indítása
    startShowModeIndicator();

    // Sprite előkészítése az új módhoz
    manageSpriteForMode(currentMode_);

    // Config mentése
    setCurrentModeToConfig();
}

/**
 * @brief Módok megjelenítésének indítása
 */
void SpectrumVisualizationComponent::startShowModeIndicator() {
    // Mode indicator megjelenítése 20 másodpercig
    modeIndicatorVisible_ = true;
    modeIndicatorDrawn_ = false; // Reset a flag-et hogy azonnal megjelenjen
    needBorderDrawn = true;      // Kényszerítjük a keret újrarajzolását

    // Indikátor eltüntetésének időzítése
    modeIndicatorHideTime_ = millis() + FftDisplayConstants::MODE_INDICATOR_VISIBLE_TIMEOUT_MS; // 20 másodpercig látható
}

/**
 * @brief AM/FM Mód betöltése config-ból
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

    // FFT paraméterek beállítása az új módhoz
    setFftParametersForDisplayMode();

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
            DEBUG("SpectrumVisualizationComponent::renderSpectrumLowRes - Sprite nincs létrehozva\n");
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

    // Ha nincs friss adat vagy nincs magnitude adat, ne rajzoljunk újra (megelőzzük a villogást)
    if (!dataAvailable || !magnitudeData || currentBinWidthHz == 0) {
        // Csak a sprite kirakása a korábbi tartalommal
        sprite_->pushSprite(bounds.x, bounds.y);
        return;
    }

    const int min_bin_idx_low_res = std::max(2, static_cast<int>(std::round(AnalyzerConstants::ANALYZER_MIN_FREQ_HZ / currentBinWidthHz)));
    const int max_bin_idx_low_res = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(maxDisplayFrequencyHz_ / currentBinWidthHz)));
    const int num_bins_in_low_res_range = std::max(1, max_bin_idx_low_res - min_bin_idx_low_res + 1);

    // Adaptív autogain használata
    float adaptiveScale = getAdaptiveScale(SensitivityConstants::AMPLITUDE_SCALE);

    double band_magnitudes[LOW_RES_BANDS] = {0.0};

    // magnitudeData már garantáltan nem nullptr itt
    for (int i = min_bin_idx_low_res; i <= max_bin_idx_low_res; i++) {
        uint8_t band_idx = getBandVal(i, min_bin_idx_low_res, num_bins_in_low_res_range, LOW_RES_BANDS);
        if (band_idx < LOW_RES_BANDS) {
            band_magnitudes[band_idx] = std::max(band_magnitudes[band_idx], magnitudeData[i]);
        }
    }

    // Legnagyobb érték megkeresése az adaptív autogain számára
    float maxMagnitude = 0.0f;
    for (int band_idx = 0; band_idx < LOW_RES_BANDS; band_idx++) {
        maxMagnitude = std::max(maxMagnitude, static_cast<float>(band_magnitudes[band_idx]));
    }

    // Sávok kirajzolása sprite-ra (adaptív autogain-nel)
    for (int band_idx = 0; band_idx < bands_to_display_on_screen; band_idx++) {
        int x_pos_for_bar = x_offset + bar_total_width_pixels_dynamic * band_idx;

        // Előbb töröljük az oszlop területét (fekete háttér)
        sprite_->fillRect(x_pos_for_bar, 0, dynamic_bar_width_pixels, graphH, TFT_BLACK);

        // Adaptív magnitúdó skálázás - egységes logika: nagyobb scale = nagyobb érzékenység
        double magnitude = band_magnitudes[band_idx];
        int dsize = static_cast<int>(magnitude * adaptiveScale);
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

    // Adaptív autogain frissítése
    updateFrameBasedGain(maxMagnitude);

    // Sprite kirakása a képernyőre
    sprite_->pushSprite(bounds.x, bounds.y);

    // Frekvencia feliratok rajzolása, ha még nem történt meg
    renderFrequencyLabels(AnalyzerConstants::ANALYZER_MIN_FREQ_HZ, maxDisplayFrequencyHz_);
}

/**
 * @brief High resolution spektrum renderelése (sprite-tal, javított amplitúdóval)
 */
void SpectrumVisualizationComponent::renderSpectrumHighRes() {
    // Audio feldolgozás Core1-en történik, AudioCore1Manager-en keresztül

    int graphH = getGraphHeight();
    if (!spriteCreated_ || bounds.width == 0 || graphH <= 0) {
        if (!spriteCreated_) {
            DEBUG("SpectrumVisualizationComponent::renderSpectrumHighRes - Sprite nincs létrehozva\n");
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

    // Ha nincs friss adat vagy nincs magnitude adat, ne rajzoljunk újra (megelőzzük a villogást)
    if (!dataAvailable || !magnitudeData || currentBinWidthHz == 0) {
        // Csak a sprite kirakása a korábbi tartalommal
        sprite_->pushSprite(bounds.x, bounds.y);
        return;
    }

    const int min_bin_idx_for_display = std::max(2, static_cast<int>(std::round(AnalyzerConstants::ANALYZER_MIN_FREQ_HZ / currentBinWidthHz)));
    const int max_bin_idx_for_display = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(maxDisplayFrequencyHz_ / currentBinWidthHz)));
    const int num_bins_in_display_range = std::max(1, max_bin_idx_for_display - min_bin_idx_for_display + 1);

    // Adaptív autogain használata
    float adaptiveScale = getAdaptiveScale(SensitivityConstants::AMPLITUDE_SCALE);
    float maxMagnitude = 0.0f;

    for (int screen_pixel_x = 0; screen_pixel_x < bounds.width; ++screen_pixel_x) {
        int fft_bin_index;
        if (bounds.width == 1) {
            fft_bin_index = min_bin_idx_for_display;
        } else {
            float ratio = static_cast<float>(screen_pixel_x) / (bounds.width - 1);
            fft_bin_index = min_bin_idx_for_display + static_cast<int>(std::round(ratio * (num_bins_in_display_range - 1)));
        }
        fft_bin_index = constrain(fft_bin_index, 0, static_cast<int>(actualFftSize / 2 - 1));

        double magnitude = magnitudeData[fft_bin_index];
        maxMagnitude = std::max(maxMagnitude, static_cast<float>(magnitude));

        // Előbb töröljük a pixel oszlopot (fekete vonal)
        sprite_->drawFastVLine(screen_pixel_x, 0, graphH, TFT_BLACK);

        // Amplitúdó skálázás - adaptív autogain-nel - egységes logika: nagyobb scale = nagyobb érzékenység
        int scaled_magnitude = static_cast<int>(magnitude * adaptiveScale);
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

    // Adaptív autogain frissítése
    updateFrameBasedGain(maxMagnitude);

    // Sprite kirakása a képernyőre
    sprite_->pushSprite(bounds.x, bounds.y);

    // Frekvencia feliratok rajzolása, ha még nem történt meg
    renderFrequencyLabels(AnalyzerConstants::ANALYZER_MIN_FREQ_HZ, maxDisplayFrequencyHz_);
}

/**
 * @brief Oszcilloszkóp renderelése
 */
void SpectrumVisualizationComponent::renderOscilloscope() {
    // Audio feldolgozás Core1-en történik, AudioCore1Manager-en keresztül

    int graphH = getGraphHeight();
    if (!spriteCreated_ || bounds.width == 0 || graphH <= 0) {
        if (!spriteCreated_) {
            DEBUG("SpectrumVisualizationComponent::renderOscilloscope - Sprite nincs létrehozva\n");
        }
        return;
    }

    // Core1 oszcilloszkóp adatok lekérése
    const int *osciData = nullptr;
    bool dataAvailable = getCore1OscilloscopeData(&osciData);

    if (!dataAvailable || !osciData) {
        // Ha nincs új adat, a korábbi tartalmat rajzoljuk ki újra a villogás elkerülése érdekében.
        sprite_->pushSprite(bounds.x, bounds.y);
        return;
    }

    // Sprite törlése - csak akkor, ha van új adat
    sprite_->fillSprite(TFT_BLACK);

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
            DEBUG("SpectrumVisualizationComponent::renderEnvelope - Sprite nincs létrehozva\n");
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
        currentBinWidthHz = (AudioProcessorConstants::MAX_SAMPLING_FREQUENCY / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    // 1. Adatok eltolása balra a wabuf-ban
    for (int r = 0; r < bounds.height; ++r) { // Teljes bounds.height
        for (int c = 0; c < bounds.width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    // Ennél az értéknél nem látszanak a tüskék...
    constexpr int ENVELOPE_BIN_NMUMBER = 40; // Az envelope-hoz használt bin szám

    const int min_bin_for_env = std::max(10, static_cast<int>(std::round(AnalyzerConstants::ANALYZER_MIN_FREQ_HZ / currentBinWidthHz)));
    const int max_bin_for_env = std::min(static_cast<int>(actualFftSize / ENVELOPE_BIN_NMUMBER - 1), static_cast<int>(std::round(maxDisplayFrequencyHz_ * 0.2f / currentBinWidthHz)));
    const int num_bins_in_env_range = std::max(1, max_bin_for_env - min_bin_for_env + 1);

    // Frame-alapú adaptív skálázás envelope-hoz
    float adaptiveScale = getAdaptiveScale(SensitivityConstants::ENVELOPE_INPUT_GAIN);

    // Konzervatív korlátok envelope-hez
    adaptiveScale = constrain(adaptiveScale, SensitivityConstants::ENVELOPE_INPUT_GAIN * 0.1f, SensitivityConstants::ENVELOPE_INPUT_GAIN * 10.0f); // 2. Új adatok betöltése
    // Az Envelope módhoz az magnitudeData értékeit használjuk csökkentett erősítéssel.
    float maxRawMagnitude = 0.0f;
    float maxGainedVal = 0.0f;

    // Minden sort feldolgozunk a teljes felbontásért
    for (int r = 0; r < bounds.height; ++r) {
        // 'r' (0 to bounds.height-1) leképezése FFT bin indexre a szűkített tartományon belül
        int fft_bin_index = min_bin_for_env + static_cast<int>(std::round(static_cast<float>(r) / std::max(1, (bounds.height - 1)) * (num_bins_in_env_range - 1)));
        fft_bin_index = constrain(fft_bin_index, min_bin_for_env, max_bin_for_env); // Finomabb gain alkalmazás envelope-hez
        double rawMagnitude = magnitudeData[fft_bin_index];

        // KRITIKUS: Infinity és NaN értékek szűrése!
        if (!isfinite(rawMagnitude) || rawMagnitude < 0.0) {
            rawMagnitude = 0.0;
        }

        // További védelem: túl nagy értékek limitálása
        if (rawMagnitude > 10000.0) {
            rawMagnitude = 10000.0;
        }

        double gained_val = rawMagnitude * adaptiveScale;

        // Debug info gyűjtése
        maxRawMagnitude = std::max(maxRawMagnitude, static_cast<float>(rawMagnitude));
        maxGainedVal = std::max(maxGainedVal, static_cast<float>(gained_val));

        wabuf[r][bounds.width - 1] = static_cast<uint8_t>(constrain(gained_val, 0.0, 255.0));
    }

    // 3. Sprite törlése és burkológörbe kirajzolása
    sprite_->fillSprite(TFT_BLACK); // Sprite törlése

    // Erőteljes simítás a tüskék ellen
    constexpr float ENVELOPE_SMOOTH_FACTOR = 0.05f;   // Sokkal erősebb simítás (volt 0.15f)
    constexpr float ENVELOPE_NOISE_THRESHOLD = 10.0f; // Magasabb zajküszöb a tüskék ellen (volt 2.0f)

    // Először rajzoljunk egy vékony központi vízszintes vonalat (alapvonal) - mindig látható
    int yCenter_on_sprite = graphH / 2;
    sprite_->drawFastHLine(0, yCenter_on_sprite, bounds.width, TFT_WHITE);

    for (int c = 0; c < bounds.width; ++c) {
        int sum_val_in_col = 0;
        int count_val_in_col = 0;
        bool column_has_signal = false;

        for (int r_wabuf = 0; r_wabuf < bounds.height; ++r_wabuf) { // Teljes bounds.height
            if (wabuf[r_wabuf][c] > ENVELOPE_NOISE_THRESHOLD) {
                column_has_signal = true;
                sum_val_in_col += wabuf[r_wabuf][c];
                count_val_in_col++;
            }
        }

        // A maximális amplitúdó simítása az oszlopban - átlag helyett maximum, de korlátozott
        float current_col_max_amplitude = 0.0f;
        if (count_val_in_col > 0) {
            current_col_max_amplitude = static_cast<float>(sum_val_in_col) / count_val_in_col;
            // NEM korlátozzuk itt - a rajzolásnál fogjuk kezelni a tüskéket
        }

        // Zajszűrés: kis amplitúdók elnyomása
        if (current_col_max_amplitude < ENVELOPE_NOISE_THRESHOLD) {
            current_col_max_amplitude = 0.0f;
        }

        // Erősebb simítás az oszlopok között - lassabb változás
        envelopeLastSmoothedValue_ = ENVELOPE_SMOOTH_FACTOR * envelopeLastSmoothedValue_ + (1.0f - ENVELOPE_SMOOTH_FACTOR) * current_col_max_amplitude;

        // További simítás: csak jelentős változásokat engedünk át
        if (abs(current_col_max_amplitude - envelopeLastSmoothedValue_) < ENVELOPE_NOISE_THRESHOLD) {
            current_col_max_amplitude = envelopeLastSmoothedValue_;
        } // Csak akkor rajzolunk burkológörbét, ha van számottevő jel
        if (column_has_signal || envelopeLastSmoothedValue_ > 0.5f) {
            // VÍZSZINTES NAGYÍTÁS: Csak a középső részt használjuk nagyobb felbontásért
            float displayValue = envelopeLastSmoothedValue_;

            // Intelligens tüske korlát megtartva
            if (displayValue > 150.0f) {
                displayValue = 150.0f + (displayValue - 150.0f) * 0.1f;
            }

            // EREDETI NAGYÍTÁS + vízszintes szétnyújtás
            // A teljes grafikon magasság 80%-át használjuk a jobb láthatóságért
            float y_offset_float = (displayValue / 100.0f) * (graphH * 0.8f); // Nagyobb skálázás, több részlet

            int y_offset_pixels = static_cast<int>(round(y_offset_float));
            y_offset_pixels = std::min(y_offset_pixels, graphH - 4); // Kis margó
            if (y_offset_pixels < 0)
                y_offset_pixels = 0;

            if (y_offset_pixels > 1) {
                // Szimmetrikus burkoló a középvonaltól - eredeti stílus
                int yUpper_on_sprite = yCenter_on_sprite - y_offset_pixels / 2;
                int yLower_on_sprite = yCenter_on_sprite + y_offset_pixels / 2;

                yUpper_on_sprite = constrain(yUpper_on_sprite, 2, graphH - 3);
                yLower_on_sprite = constrain(yLower_on_sprite, 2, graphH - 3);

                if (yUpper_on_sprite <= yLower_on_sprite) {
                    // Vastagabb vonal a jobb láthatóságért
                    sprite_->drawFastVLine(c, yUpper_on_sprite, yLower_on_sprite - yUpper_on_sprite + 1, TFT_WHITE);
                    // Második vonal a széleken a részletesebb megjelenítésért
                    if (y_offset_pixels > 4) {
                        sprite_->drawPixel(c, yUpper_on_sprite - 1, TFT_WHITE);
                        sprite_->drawPixel(c, yLower_on_sprite + 1, TFT_WHITE);
                    }
                }
            }
        }
    }

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
            DEBUG("SpectrumVisualizationComponent::renderWaterfall - Sprite nincs létrehozva\n");
        }
        return;
    }

    // Core1 spektrum adatok lekérése
    const double *magnitudeData = nullptr;
    uint16_t actualFftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
    float currentBinWidthHz = 0.0f;
    float currentAutoGain = 1.0f;

    bool dataAvailable = getCore1SpectrumData(&magnitudeData, &actualFftSize, &currentBinWidthHz, &currentAutoGain);

    // Ha nincs friss adat, ne frissítsük a waterfall buffert - megelőzzük a hamis mintákat
    if (!dataAvailable || !magnitudeData || currentBinWidthHz == 0) {
        // Csak a sprite kirakása a korábbi tartalommal
        sprite_->pushSprite(bounds.x, bounds.y);
        return;
    }

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

    // Adaptív autogain használata waterfall-hoz
    float adaptiveScale = getAdaptiveScale(SensitivityConstants::WATERFALL_INPUT_SCALE);
    float maxMagnitude = 0.0f;

    for (int r = 0; r < bounds.height; ++r) {
        // 'r' (0 to bounds.height-1) leképezése FFT bin indexre a szűkített tartományon belül
        int fft_bin_index = min_bin_for_wf + static_cast<int>(std::round(static_cast<float>(r) / std::max(1, (bounds.height - 1)) * (num_bins_in_wf_range - 1)));
        fft_bin_index = constrain(fft_bin_index, min_bin_for_wf, max_bin_for_wf);

        // Waterfall input scale - adaptív autogain-nel
        double rawMagnitude = magnitudeData[fft_bin_index];
        maxMagnitude = std::max(maxMagnitude, static_cast<float>(rawMagnitude));
        double scaledMagnitude = rawMagnitude * adaptiveScale;
        uint8_t finalValue = static_cast<uint8_t>(constrain(scaledMagnitude, 0.0, 255.0));

        wabuf[r][bounds.width - 1] = finalValue;
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

    // Adaptív autogain frissítése
    updateFrameBasedGain(maxMagnitude);

    // Sprite kirakása a képernyőre
    sprite_->pushSprite(bounds.x, bounds.y);
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
 * @brief Beállítja a hangolási segéd típusát (CW vagy RTTY).
 * @param type A beállítandó TuningAidType.
 * @note A setFftParametersForDisplayMode() hívja meg ezt a függvényt a hangolási segéd típusának beállítására.
 */
void SpectrumVisualizationComponent::setTuningAidType(TuningAidType type) {

    // CW: A config.data.cwReceiverOffsetHz +- 600 Hz körüli CW frekvencia a hangolássegéd sávszélessége
    constexpr float CW_TUNING_AID_SPAN_HZ = 600.0f;

    // RTTY: A minimum frekvencia: min(f_mark, f_space) - 200 Hz, a maximum frekvencia: max(f_mark, f_space) + 200 Hz.
    // Így a tuning aid spektrum sávszélessége a két RTTY frekvencia közötti távolság plusz kétszer 200 Hz.
    constexpr float RTTY_TUNING_AID_SPAN_HZ = 200.0f;

    bool typeChanged = (currentTuningAidType_ != type);
    currentTuningAidType_ = type;

    if (currentMode_ == DisplayMode::CWWaterfall || currentMode_ == DisplayMode::RTTYWaterfall) {
        uint16_t oldMinFreq = currentTuningAidMinFreqHz_;
        uint16_t oldMaxFreq = currentTuningAidMaxFreqHz_;

        if (currentTuningAidType_ == TuningAidType::CW_TUNING) {
            // CW: 600 Hz span a CW offset frekvencia körül
            uint16_t centerFreq = config.data.cwReceiverOffsetHz;
            currentTuningAidMinFreqHz_ = centerFreq - CW_TUNING_AID_SPAN_HZ / 2;
            currentTuningAidMaxFreqHz_ = centerFreq + CW_TUNING_AID_SPAN_HZ / 2;
        } else if (currentTuningAidType_ == TuningAidType::RTTY_TUNING) {
            // RTTY: Mark és Space frekvenciák közötti terület + margó
            uint16_t f_mark = config.data.rttyMarkFrequencyHz;
            uint16_t f_space = f_mark - config.data.rttyShiftHz;
            uint16_t min_freq = std::min(f_mark, f_space) - RTTY_TUNING_AID_SPAN_HZ;
            uint16_t max_freq = std::max(f_mark, f_space) + RTTY_TUNING_AID_SPAN_HZ;
            currentTuningAidMinFreqHz_ = min_freq;
            currentTuningAidMaxFreqHz_ = max_freq;
        } else {
            // OFF_DECODER: alapértelmezett tartomány
            currentTuningAidMinFreqHz_ = 0.0f;
            currentTuningAidMaxFreqHz_ = maxDisplayFrequencyHz_;
        }

        // Ha változott a frekvencia tartomány, invalidáljuk a buffert
        if (typeChanged || oldMinFreq != currentTuningAidMinFreqHz_ || oldMaxFreq != currentTuningAidMaxFreqHz_) {
            for (auto &row : wabuf) {
                std::fill(row.begin(), row.end(), 0);
            }
        }
    }
}

/**
 * @brief Hangolási segéd renderelése (CW/RTTY waterfall)
 */
void SpectrumVisualizationComponent::renderCwOrRttyTuningAid() {
    // Audio feldolgozás Core1-en történik, AudioCore1Manager-en keresztül

    int graphH = getGraphHeight();
    if (!spriteCreated_ || bounds.width == 0 || graphH <= 0 || wabuf.empty() || wabuf[0].empty()) {
        if (!spriteCreated_) {
            DEBUG("SpectrumVisualizationComponent::renderTuningAid - Sprite nincs létrehozva\n");
        }
        return;
    }

    // Core1 spektrum adatok lekérése
    const double *magnitudeData = nullptr;
    uint16_t actualFftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
    float currentBinWidthHz = 0.0f;
    float currentAutoGain = 1.0f;

    // Lekérjük az adatokat a Core1-ről
    bool dataAvailable = getCore1SpectrumData(&magnitudeData, &actualFftSize, &currentBinWidthHz, &currentAutoGain);
    if (!dataAvailable || !magnitudeData || currentBinWidthHz == 0) {
        // sprite_->pushSprite(bounds.x, bounds.y); // Sprite kirakása a képernyőre
        return;
    }

    // 1. Sprite scroll lefelé (1 pixel)
    sprite_->scroll(0, 1);

    // Waterfall paraméterek: tuning aid-hez a min-max frekvenciahatárok alapján
    const int min_bin_for_tuning = std::max(2, static_cast<int>(std::round(currentTuningAidMinFreqHz_ / currentBinWidthHz)));
    const int max_bin_for_tuning = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(currentTuningAidMaxFreqHz_ / currentBinWidthHz)));
    const int num_bins_in_tuning_range = std::max(1, max_bin_for_tuning - min_bin_for_tuning + 1);

    // Adaptív autogain használata waterfall-hoz
    float adaptiveScale = getAdaptiveScale(SensitivityConstants::WATERFALL_INPUT_SCALE);
    float maxMagnitude = 0.0f;

    // 2. Új adatok betöltése a legfelső sorba és csak azt rajzoljuk ki
    for (int c = 0; c < bounds.width; ++c) {
        float ratio_in_display_width = (bounds.width <= 1) ? 0.0f : (static_cast<float>(c) / (bounds.width - 1));
        int fft_bin_index = min_bin_for_tuning + static_cast<int>(std::round(ratio_in_display_width * (num_bins_in_tuning_range - 1)));
        fft_bin_index = constrain(fft_bin_index, min_bin_for_tuning, max_bin_for_tuning);
        double rawMagnitude = magnitudeData[fft_bin_index];
        maxMagnitude = std::max(maxMagnitude, static_cast<float>(rawMagnitude));
        double scaledMagnitude = rawMagnitude * adaptiveScale;
        uint8_t finalValue = static_cast<uint8_t>(constrain(scaledMagnitude, 0.0, 255.0));
        wabuf[0][c] = finalValue;

        // Csak a legfelső sort rajzoljuk ki (y=0)
        constexpr int WF_GRADIENT = 100;
        uint16_t color = valueToWaterfallColor(WF_GRADIENT * finalValue, 0.0f, 255.0f * WF_GRADIENT, 0);
        sprite_->drawPixel(c, 0, color);
    }

    // Adaptív autogain frissítése
    updateFrameBasedGain(maxMagnitude);

    // Színek
    constexpr uint16_t TUNING_AID_CW_TARGET_COLOR = TFT_GREEN;
    constexpr uint16_t TUNING_AID_RTTY_SPACE_COLOR = TFT_CYAN;
    constexpr uint16_t TUNING_AID_RTTY_MARK_COLOR = TFT_YELLOW;

    uint16_t min_freq_displayed = currentTuningAidMinFreqHz_;
    uint16_t max_freq_displayed = currentTuningAidMaxFreqHz_;
    uint16_t displayed_span_hz = max_freq_displayed - min_freq_displayed;

    // 4. Célfrekvencia vonalának kirajzolása a sprite-ra
    if (currentTuningAidType_ == TuningAidType::CW_TUNING || currentTuningAidType_ == TuningAidType::RTTY_TUNING) {

        // Hangolási csík kirajzolása
        if (displayed_span_hz > 0) {
            if (currentTuningAidType_ == TuningAidType::CW_TUNING) {
                // CW célfrekvencia középre kerül
                uint16_t line_x = bounds.width / 2;
                sprite_->drawFastVLine(line_x, 0, graphH, TUNING_AID_CW_TARGET_COLOR);

            } else if (currentTuningAidType_ == TuningAidType::RTTY_TUNING) {
                uint16_t f_mark = config.data.rttyMarkFrequencyHz;
                uint16_t f_space = f_mark - config.data.rttyShiftHz;

                // Space vonal
                if (f_space >= min_freq_displayed && f_space <= max_freq_displayed) {
                    float ratio_space = (static_cast<float>(f_space) - min_freq_displayed) / displayed_span_hz;
                    uint16_t line_x_space = static_cast<uint16_t>(std::round(ratio_space * (bounds.width - 1)));
                    line_x_space = constrain(line_x_space, 0, bounds.width - 1);
                    sprite_->drawFastVLine(line_x_space, 0, graphH, TUNING_AID_RTTY_SPACE_COLOR);
                }

                // Mark vonal
                if (f_mark >= min_freq_displayed && f_mark <= max_freq_displayed) {
                    float ratio_mark = (static_cast<float>(f_mark) - min_freq_displayed) / displayed_span_hz;
                    uint16_t line_x_mark = static_cast<uint16_t>(std::round(ratio_mark * (bounds.width - 1)));
                    line_x_mark = constrain(line_x_mark, 0, bounds.width - 1);
                    sprite_->drawFastVLine(line_x_mark, 0, graphH, TUNING_AID_RTTY_MARK_COLOR);
                }
            }

            // Frekvencia címkék kirajzolása a sprite-ba, így nem villog
            sprite_->setFreeFont();
            sprite_->setTextSize(1);
            sprite_->setTextDatum(BC_DATUM);

            if (currentTuningAidType_ == TuningAidType::CW_TUNING) {
                uint16_t line_x = bounds.width / 2;
                uint16_t label_y = graphH > 2 ? graphH - 2 : 0;
                sprite_->fillRect(line_x - 25, label_y - 8, 50, 10, TFT_BLACK);
                sprite_->setTextColor(TUNING_AID_CW_TARGET_COLOR, TFT_BLACK);
                sprite_->drawString(String(config.data.cwReceiverOffsetHz) + "Hz", line_x, label_y);

            } else if (currentTuningAidType_ == TuningAidType::RTTY_TUNING) {
                uint16_t f_mark = config.data.rttyMarkFrequencyHz;
                uint16_t f_space = f_mark - config.data.rttyShiftHz;
                // Space címke
                if (f_space >= min_freq_displayed && f_space <= max_freq_displayed) {
                    float ratio_space = (static_cast<float>(f_space) - min_freq_displayed) / displayed_span_hz;
                    uint16_t line_x_space = static_cast<uint16_t>(std::round(ratio_space * (bounds.width - 1)));
                    line_x_space = constrain(line_x_space, 0, bounds.width - 1);
                    uint16_t label_y = graphH;
                    sprite_->fillRect(line_x_space - 25, label_y - 8, 50, 10, TFT_BLACK);
                    sprite_->setTextColor(TUNING_AID_RTTY_SPACE_COLOR, TFT_BLACK);
                    sprite_->drawString(String(static_cast<uint16_t>(round(f_space))) + "Hz", line_x_space, label_y);
                }
                // Mark címke
                if (f_mark >= min_freq_displayed && f_mark <= max_freq_displayed) {
                    float ratio_mark = (static_cast<float>(f_mark) - min_freq_displayed) / displayed_span_hz;
                    uint16_t line_x_mark = static_cast<uint16_t>(std::round(ratio_mark * (bounds.width - 1)));
                    line_x_mark = constrain(line_x_mark, 0, bounds.width - 1);
                    uint16_t label_y = graphH;
                    sprite_->fillRect(line_x_mark - 25, label_y - 8, 50, 10, TFT_BLACK);
                    sprite_->setTextColor(TUNING_AID_RTTY_MARK_COLOR, TFT_BLACK);
                    sprite_->drawString(String(static_cast<uint16_t>(round(f_mark))) + "Hz", line_x_mark, label_y);
                }
            }
        }
    }

    // Sprite kirakása a képernyőre
    sprite_->pushSprite(bounds.x, bounds.y);

    // Adaptív autogain frissítése
    updateFrameBasedGain(maxMagnitude);

    // Frekvencia feliratok rajzolása, ha még nem történt meg
    renderFrequencyLabels(min_freq_displayed, max_freq_displayed);

    decoderManager->processBlock(magnitudeData, actualFftSize);
    std::string text = decoderManager->getDecodedText();
    DEBUG("SpectrumVisualizationComponent::renderTuningAid - Decoded text: %s\n", text.c_str());
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
 * @brief Core1 spektrum adatok lekérése
 */
bool SpectrumVisualizationComponent::getCore1SpectrumData(const double **outData, uint16_t *outSize, float *outBinWidth, float *outAutoGain) {
    return AudioCore1Manager::getSpectrumData(outData, outSize, outBinWidth, outAutoGain);
}

/**
 * @brief Core1 oszcilloszkóp adatok lekérése
 */
bool SpectrumVisualizationComponent::getCore1OscilloscopeData(const int **outData) {
    // Core1 oszcilloszkóp adatok lekérése
    return AudioCore1Manager::getOscilloscopeData(outData);
}

/**
 * @brief Muted üzenet kirajzolása
 */
void SpectrumVisualizationComponent::drawMutedMessage() {

    // Ha már kirajzoltuk, nem csinálunk semmit
    if (isMutedDrawn) {
        return;
    }

    // A terület közepének meghatározása
    int16_t x = bounds.x + bounds.width / 2;
    int16_t y = bounds.y + bounds.height / 2;
    tft.setFreeFont();
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(MC_DATUM); // Middle center
    tft.drawString("-- Muted --", x, y);

    isMutedDrawn = true;
}

/**
 * @brief Core1 FFT méret szálbiztos lekérése
 */
uint16_t SpectrumVisualizationComponent::getCore1FftSize() {
    uint16_t fftSize = 0;
    if (AudioCore1Manager::getFftSize(&fftSize)) {
        return fftSize;
    }

    // Ha nem sikerült lekérni, akkor egy default értéket adunk vissza
    return AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
}

/**
 * @brief Core1 bin szélesség szálbiztos lekérése
 */
float SpectrumVisualizationComponent::getCore1BinWidthHz() {

    float binWidthHz = 0.0f;

    uint16_t fftSize = 0;
    if (AudioCore1Manager::getFftCurrentBinWidth(&binWidthHz)) {
        return fftSize;
    }

    // Ha nincs friss adat, visszaadunk egy becsült értéket
    return AudioProcessorConstants::MAX_SAMPLING_FREQUENCY / AudioProcessorConstants::DEFAULT_FFT_SAMPLES; // Default estimate
}

/**
 * @brief Optimal FFT méret meghatározása a megjelenítési módhoz
 */
uint16_t SpectrumVisualizationComponent::getOptimalFftSizeForMode(DisplayMode mode) const {
    switch (mode) {
        case DisplayMode::CWWaterfall:
        case DisplayMode::RTTYWaterfall:
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
 * @brief Spektrum mód dekódolása szöveggé
 */
const char *SpectrumVisualizationComponent::decodeModeToStr() {

    const char *modeText = "";

    switch (currentMode_) {
        case DisplayMode::Off:
            modeText = "Off";
            break;
        case DisplayMode::SpectrumLowRes:
            modeText = "FFT lowres";
            break;
        case DisplayMode::SpectrumHighRes:
            modeText = "FFT highres";
            break;
        case DisplayMode::Oscilloscope:
            modeText = "Oscilloscope";
            break;
        case DisplayMode::Waterfall:
            modeText = "Waterfall";
            break;
        case DisplayMode::Envelope:
            modeText = "Envelope";
            break;
        case DisplayMode::CWWaterfall:
            modeText = "CW Waterfall";
            break;
        case DisplayMode::RTTYWaterfall:
            modeText = "RTTY Waterfall";
            break;
        default:
            modeText = "Unknown";
            break;
    }
    return modeText;
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

    // Mode szöveggé dekódolása

    String modeText = decodeModeToStr();
    if (currentMode_ != DisplayMode::Off) {
        modeText += isAutoGainMode() ? " (Auto" : " (Manu";
        modeText += " gain)";
    }

    // Clear mode indicator area explicitly before text drawing - KERET ALATT
    int indicatorY = bounds.y + bounds.height; // Közvetlenül a keret alatt kezdődik
    tft.fillRect(bounds.x - 4, indicatorY, bounds.width + 8, indicatorH, TFT_BLACK);

    // Draw text at component bottom + indicator area, center
    // Y coordinate will be the text baseline (bottom of the indicator area)
    tft.drawString(modeText, bounds.x + bounds.width / 2, indicatorY + indicatorH);
}

/**
 * @brief Frekvencia címkék renderelése a mode indikátor helyére
 */
void SpectrumVisualizationComponent::renderFrequencyLabels(uint16_t minDisplayFrequencyHz, uint16_t maxDisplayFrequencyHz) {

    if (!frequencyLabelsDrawn_) {
        return;
    }

    uint16_t indicatorH = 10;
    uint16_t indicatorY = bounds.y + bounds.height; // Közvetlenül a keret alatt kezdődik

    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);

    // Balra igazított min frekvencia
    tft.setTextDatum(BL_DATUM);
    tft.drawString(Utils::formatFrequencyString(minDisplayFrequencyHz), bounds.x, indicatorY + indicatorH);

    // Jobbra igazított max frekvencia
    tft.setTextDatum(BR_DATUM);
    tft.drawString(Utils::formatFrequencyString(maxDisplayFrequencyHz), bounds.x + bounds.width, indicatorY + indicatorH);

    frequencyLabelsDrawn_ = false;
}
