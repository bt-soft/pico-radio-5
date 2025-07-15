#ifndef __SMETER_H
#define __SMETER_H

#include <algorithm> // std::min miatt

#include "UIComponent.h" // UIComponent alaposztály
#include "defines.h"     // Színekhez (TFT_BLACK, stb.)

namespace SMeterConstants {
constexpr int16_t SMETER_WIDTH = 240; // S-Meter teljes szélessége
// Skála méretei és pozíciója
constexpr uint8_t SCALE_WIDTH = 236;
constexpr uint8_t SCALE_HEIGHT = 46;
constexpr uint8_t SCALE_START_X_OFFSET = 2;
constexpr uint8_t SCALE_START_Y_OFFSET = 6;
constexpr uint8_t SCALE_END_X_OFFSET = SCALE_START_X_OFFSET + SCALE_WIDTH;
constexpr uint8_t SCALE_END_Y_OFFSET = SCALE_START_Y_OFFSET + SCALE_HEIGHT;

// S-Pont skála rajzolása
constexpr uint8_t SPOINT_START_X = 15;
constexpr uint8_t SPOINT_Y = 24;
constexpr uint8_t SPOINT_TICK_WIDTH = 2;
constexpr uint8_t SPOINT_TICK_HEIGHT = 8;
constexpr uint8_t SPOINT_NUMBER_Y = 13;
constexpr uint8_t SPOINT_SPACING = 12;
constexpr uint8_t SPOINT_COUNT = 10; // 0-9

// Plusz skála rajzolása
constexpr uint8_t PLUS_SCALE_START_X = 123;
constexpr uint8_t PLUS_SCALE_Y = 24;
constexpr uint8_t PLUS_SCALE_TICK_WIDTH = 3;
constexpr uint8_t PLUS_SCALE_TICK_HEIGHT = 8;
constexpr uint8_t PLUS_SCALE_NUMBER_Y = 13;
constexpr uint8_t PLUS_SCALE_SPACING = 16;
constexpr uint8_t PLUS_SCALE_COUNT = 6; // +10-től +60-ig

// Skála sávok rajzolása
constexpr uint8_t SBAR_Y = 32;
constexpr uint8_t SBAR_HEIGHT = 3;
constexpr uint8_t SBAR_SPOINT_WIDTH = 112;
constexpr uint8_t SBAR_PLUS_START_X = 127;
constexpr uint8_t SBAR_PLUS_WIDTH = 100;

// Mérősáv rajzolása
constexpr uint8_t METER_BAR_Y = 38;
constexpr uint8_t METER_BAR_HEIGHT = 6;

constexpr uint8_t METER_BAR_RED_START_X = 15; // Piros S0 sáv kezdete
constexpr uint8_t METER_BAR_RED_WIDTH = 15;   // Piros S0 sáv szélessége

// Az S1 (első narancs) az S0 (piros) után 2px réssel kezdődik
constexpr uint8_t METER_BAR_ORANGE_START_X = METER_BAR_RED_START_X + METER_BAR_RED_WIDTH + 2; // 15 + 15 + 2 = 32
constexpr uint8_t METER_BAR_ORANGE_WIDTH = 10;                                                // Narancs S1-S8 sávok szélessége
constexpr uint8_t METER_BAR_ORANGE_SPACING = 12;                                              // Narancs sávok kezdőpontjai közötti távolság (10px sáv + 2px rés)

// Az S9+10dB (első zöld) az S8 (utolsó narancs) után 2px réssel kezdődik
// S8 vége: METER_BAR_ORANGE_START_X + (7 * METER_BAR_ORANGE_SPACING) + METER_BAR_ORANGE_WIDTH = 32 + 84 + 10 = 126
constexpr uint8_t METER_BAR_GREEN_START_X = METER_BAR_ORANGE_START_X + ((8 - 1) * METER_BAR_ORANGE_SPACING) + METER_BAR_ORANGE_WIDTH + 2; // 126 + 2 = 128
constexpr uint8_t METER_BAR_GREEN_WIDTH = 14;                                                                                             // Zöld S9+dB sávok szélessége
constexpr uint8_t METER_BAR_GREEN_SPACING = 16;                                                                                           // Zöld sávok kezdőpontjai közötti távolság (14px sáv + 2px rés)

constexpr uint8_t METER_BAR_FINAL_ORANGE_START_X =
    METER_BAR_GREEN_START_X + ((6 - 1) * METER_BAR_GREEN_SPACING) + METER_BAR_GREEN_WIDTH + 2; // Utolsó narancs sáv (S9+60dB felett)
                                                                                               // S9+60dB (6. zöld sáv) vége: 128 + (5*16) + 14 = 128 + 80 + 14 = 222. Utána 2px rés. -> 222 + 2 = 224
constexpr uint8_t METER_BAR_FINAL_ORANGE_WIDTH = 3;                                            // Ennek a sávnak a szélessége

constexpr uint8_t METER_BAR_MAX_PIXEL_VALUE = 208;                    // A teljes mérősáv hossza pixelben, az rssiConverter max kimenete
constexpr uint8_t METER_BAR_SPOINT_LIMIT = 9;                         // S-pontok száma (S0-S8), azaz 9 sáv (1 piros, 8 narancs)
constexpr uint8_t METER_BAR_TOTAL_LIMIT = METER_BAR_SPOINT_LIMIT + 6; // Összes sáv (S0-S8 és 6db S9+dB), azaz 9 + 6 = 15 sáv

// Szöveges címkék rajzolása
constexpr uint8_t RSSI_LABEL_X_OFFSET = 10;
constexpr uint8_t SIGNAL_LABEL_Y_OFFSET_IN_FM = 60; // FM módban a felirat Y pozíciója (ha máshova kerülne)

// Kezdeti állapot a prev_spoint-hoz
constexpr uint8_t INITIAL_PREV_SPOINT = 0xFF; // Érvénytelen érték, hogy az első frissítés biztosan megtörténjen

// RSSI konverziós optimalizáció - lookup táblák
// FM mód RSSI -> S-pont konverziós tartományok
struct RssiRange {
    uint8_t min_rssi;
    uint8_t max_rssi;
    uint8_t base_spoint;
    uint8_t multiplier;
};

// FM módhoz optimalizált lookup tábla
constexpr RssiRange FM_RSSI_TABLE[] = {
    {0, 0, 36, 0},                          // rssi < 1
    {1, 2, 60, 0},                          // S6
    {3, 8, 84, 2},                          // S7: 84 + (rssi-2)*2
    {9, 14, 96, 2},                         // S8: 96 + (rssi-8)*2
    {15, 24, 108, 2},                       // S9: 108 + (rssi-14)*2
    {25, 34, 124, 2},                       // S9+10dB: 124 + (rssi-24)*2
    {35, 44, 140, 2},                       // S9+20dB: 140 + (rssi-34)*2
    {45, 54, 156, 2},                       // S9+30dB: 156 + (rssi-44)*2
    {55, 64, 172, 2},                       // S9+40dB: 172 + (rssi-54)*2
    {65, 74, 188, 2},                       // S9+50dB: 188 + (rssi-64)*2
    {75, 76, 204, 0},                       // S9+60dB
    {77, 255, METER_BAR_MAX_PIXEL_VALUE, 0} // Max érték
};

// AM/SSB/CW módhoz optimalizált lookup tábla
constexpr RssiRange AM_RSSI_TABLE[] = {
    {0, 1, 12, 0},                          // S0
    {2, 2, 24, 0},                          // S1
    {3, 3, 36, 0},                          // S2
    {4, 4, 48, 0},                          // S3
    {5, 10, 48, 2},                         // S4: 48 + (rssi-4)*2
    {11, 16, 60, 2},                        // S5: 60 + (rssi-10)*2
    {17, 22, 72, 2},                        // S6: 72 + (rssi-16)*2
    {23, 28, 84, 2},                        // S7: 84 + (rssi-22)*2
    {29, 34, 96, 2},                        // S8: 96 + (rssi-28)*2
    {35, 44, 108, 2},                       // S9: 108 + (rssi-34)*2
    {45, 54, 124, 2},                       // S9+10dB: 124 + (rssi-44)*2
    {55, 64, 140, 2},                       // S9+20dB: 140 + (rssi-54)*2
    {65, 74, 156, 2},                       // S9+30dB: 156 + (rssi-64)*2
    {75, 84, 172, 2},                       // S9+40dB: 172 + (rssi-74)*2
    {85, 94, 188, 2},                       // S9+50dB: 188 + (rssi-84)*2
    {95, 95, 204, 0},                       // S9+60dB
    {96, 255, METER_BAR_MAX_PIXEL_VALUE, 0} // Max érték
};

constexpr size_t FM_RSSI_TABLE_SIZE = sizeof(FM_RSSI_TABLE) / sizeof(FM_RSSI_TABLE[0]);
constexpr size_t AM_RSSI_TABLE_SIZE = sizeof(AM_RSSI_TABLE) / sizeof(AM_RSSI_TABLE[0]);
} // namespace SMeterConstants

/**
 * SMeter osztály az S-Meter kezelésére - UIComponent alapon
 */
class SMeter : public UIComponent {
  private:
    uint8_t prev_spoint_bars;   // Előző S-pont érték a grafikus sávokhoz
    uint8_t prev_rssi_for_text; // Előző RSSI érték a szöveges kiíráshoz
    uint8_t prev_snr_for_text;  // Előző SNR érték a szöveges kiíráshoz// Inicializálási flag és pozíciók egyetlen struct-ban
    struct TextLayout {
        uint16_t rssi_label_x_pos;
        uint16_t rssi_value_x_pos;
        uint16_t rssi_value_max_w;
        uint16_t snr_label_x_pos;
        uint16_t snr_value_x_pos;
        uint16_t snr_value_max_w;
        uint16_t text_y_pos;
        uint8_t text_h;
        bool initialized;
    } textLayout;

    /**
     * RSSI érték konvertálása S-pont értékre (pixelben) - optimalizált lookup táblával.
     * @param rssi Bemenő RSSI érték (0-127 dBuV).
     * @param isFMMode Igaz, ha FM módban vagyunk, hamis AM/SSB/CW esetén.
     * @return A jelerősség pixelben (0-MeterBarMaxPixelValue).
     */
    uint8_t rssiConverter(uint8_t rssi, bool isFMMode);

    /**
     * S-Meter grafikus sávjainak kirajzolása a mért RSSI alapján.
     * @param rssi Aktuális RSSI érték.
     * @param isFMMode Igaz, ha FM módban vagyunk.
     */
    void drawMeterBars(uint8_t rssi, bool isFMMode);

    /**
     * TFT alapállapot beállítása szöveg rajzolásához.
     * @param color Szöveg színe
     * @param background Háttér színe
     */
    void setupTextTFT(uint16_t color, uint16_t background);

  public:
    /**
     * Konstruktor.
     * @param tft Referencia a TFT kijelző objektumra.
     * @param bounds A komponens területe (pozíció és méret).
     * @param colors Opcionális színpaletta (alapértelmezett használata).
     */
    SMeter(const Rect &bounds, const ColorScheme &colors = ColorScheme::defaultScheme());

    /**
     * S-Meter skála kirajzolása (a statikus részek: vonalak, számok).
     * Ezt általában egyszer kell meghívni a képernyő inicializálásakor.
     */
    void drawSmeterScale();

    /**
     * S-Meter érték és RSSI/SNR szöveg megjelenítése.
     * @param rssi Aktuális RSSI érték (0–127 dBμV).
     * @param snr Aktuális SNR érték (0–127 dB).
     * @param isFMMode Igaz, ha FM módban vagyunk, hamis egyébként (AM/SSB/CW).
     */
    void showRSSI(uint8_t rssi, uint8_t snr, bool isFMMode); // === UIComponent felülírt metódusok ===
    /**
     * @brief Rajzolja a komponenst (UIComponent override)
     */
    virtual void draw() override;

    /**
     * @brief Újrarajzolásra jelölés - reset-eli az initialized flag-et
     * @details Dialóg bezárása vagy képernyő törlése után szükséges a statikus skála újrarajzolása
     */
    virtual void markForRedraw(bool markChildren = false) override;

    /**
     * @brief Nem kezeli az érintést (UIComponent override)
     * @return false, mert az SMeter nem interaktív
     */
    virtual bool handleTouch(const TouchEvent &event) override { return false; }

  protected:
    /**
     * @brief Vizuális lenyomott visszajelzés letiltása
     * @return false - ez a komponens nem ad vizuális visszajelzést lenyomásra
     */
    virtual bool allowsVisualPressedFeedback() const override { return false; }
};

#endif // __SMETER_H