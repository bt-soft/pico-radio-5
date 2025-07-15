#include "SMeter.h"
#include "UIColorPalette.h" // TFT_COLOR_BACKGROUND definícióhoz

/**
 * Konstruktor.
 * @param bounds A komponens területe (pozíció és méret).
 * @param colors Opcionális színpaletta.
 */
SMeter::SMeter(const Rect &bounds, const ColorScheme &colors) : UIComponent(bounds, colors), prev_spoint_bars(SMeterConstants::INITIAL_PREV_SPOINT), prev_rssi_for_text(0xFF), prev_snr_for_text(0xFF) {
    // Inicializáljuk a textLayout struct-ot
    textLayout = {0, 0, 0, 0, 0, 0, 0, 0, false};
}

/**
 * RSSI érték konvertálása S-pont értékre (pixelben) - optimalizált lookup táblával.
 * @param rssi Bemenő RSSI érték (0-127 dBuV).
 * @param isFMMode Igaz, ha FM módban vagyunk, hamis AM/SSB/CW esetén.
 * @return A jelerősség pixelben (0-MeterBarMaxPixelValue).
 */
uint8_t SMeter::rssiConverter(uint8_t rssi, bool isFMMode) {
    using namespace SMeterConstants;

    // Válasszuk ki a megfelelő lookup táblát
    const RssiRange *table = isFMMode ? FM_RSSI_TABLE : AM_RSSI_TABLE;
    const size_t table_size = isFMMode ? FM_RSSI_TABLE_SIZE : AM_RSSI_TABLE_SIZE;

    // Keresés a lookup táblában
    for (size_t i = 0; i < table_size; i++) {
        if (rssi >= table[i].min_rssi && rssi <= table[i].max_rssi) {
            // Számítás: base_spoint + (rssi - min_rssi) * multiplier
            int spoint_calc = table[i].base_spoint;
            if (table[i].multiplier > 0) {
                spoint_calc += (rssi - table[i].min_rssi) * table[i].multiplier;
            }

            // Biztosítjuk, hogy az érték a megengedett tartományban maradjon
            if (spoint_calc < 0)
                spoint_calc = 0;
            if (spoint_calc > METER_BAR_MAX_PIXEL_VALUE)
                spoint_calc = METER_BAR_MAX_PIXEL_VALUE;

            return static_cast<uint8_t>(spoint_calc);
        }
    }

    // Alapértelmezett érték, ha nem találtunk egyezést
    return isFMMode ? 36 : 0;
}

/**
 * S-Meter grafikus sávjainak kirajzolása a mért RSSI alapján.
 * @param rssi Aktuális RSSI érték.
 * @param isFMMode Igaz, ha FM módban vagyunk.
 */
void SMeter::drawMeterBars(uint8_t rssi, bool isFMMode) {
    using namespace SMeterConstants;
    uint8_t spoint = rssiConverter(rssi, isFMMode); // Jelerősség pixelben

    // Optimalizáció: ne rajzoljunk sávokat feleslegesen
    if (spoint == prev_spoint_bars) {
        return;
    }

    prev_spoint_bars = spoint;
    int tik = 0;      // 'tik': aktuálisan rajzolt sáv indexe (S0, S1, ..., S9+10dB, ...)
    int met = spoint; // 'met': hátralévő "jelerősség energia" pixelben, amit még ki kell rajzolni

    // Az utolsó színes sáv abszolút X koordinátája a kijelzőn.
    // Kezdetben a piros sáv (S0) elejére mutat. Ha spoint=0, ez marad.
    int end_of_colored_x_abs = bounds.x + METER_BAR_RED_START_X;

    // Piros (S0) és narancs (S1-S8) sávok rajzolása
    // Ciklus amíg van 'met' (energia) ÉS még az S-pont tartományon (S0-S8) belül vagyunk.
    while (met > 0 && tik < METER_BAR_SPOINT_LIMIT) {
        if (tik == 0) {                                                            // Első sáv: S0 (piros)
            int draw_width = std::min(met, static_cast<int>(METER_BAR_RED_WIDTH)); // Max. a sáv szélessége, vagy amennyi 'met' van
            if (draw_width > 0) {
                ::tft.fillRect(bounds.x + METER_BAR_RED_START_X, bounds.y + METER_BAR_Y, draw_width, METER_BAR_HEIGHT, TFT_RED);
                end_of_colored_x_abs = bounds.x + METER_BAR_RED_START_X + draw_width; // Frissítjük a színes rész végét
            }
            met -= METER_BAR_RED_WIDTH; // Teljes S0 sáv "költségét" levonjuk a 'met'-ből

        } else { // Következő sávok: S1-S8 (narancs)

            // X pozíció: METER_BAR_ORANGE_START_X + (aktuális narancs sáv indexe) * (narancs sáv szélessége + rés)
            int current_bar_x = bounds.x + METER_BAR_ORANGE_START_X + ((tik - 1) * METER_BAR_ORANGE_SPACING);
            int draw_width = std::min(met, static_cast<int>(METER_BAR_ORANGE_WIDTH));
            if (draw_width > 0) {
                ::tft.fillRect(current_bar_x, bounds.y + METER_BAR_Y, draw_width, METER_BAR_HEIGHT, TFT_ORANGE);
                end_of_colored_x_abs = current_bar_x + draw_width;
            }
            met -= METER_BAR_ORANGE_WIDTH; // Teljes narancs sáv "költségét" levonjuk
        }
        tik++; // Lépünk a következő sávra
    }

    // Zöld (S9+10dB - S9+60dB) sávok rajzolása
    // Ciklus amíg van 'met' ÉS még az S9+dB tartományon belül vagyunk.
    while (met > 0 && tik < METER_BAR_TOTAL_LIMIT) {
        // X pozíció: METER_BAR_GREEN_START_X + (aktuális zöld sáv indexe az S9+dB tartományon belül) * (zöld sáv szélessége + rés)
        int current_bar_x = bounds.x + METER_BAR_GREEN_START_X + ((tik - METER_BAR_SPOINT_LIMIT) * METER_BAR_GREEN_SPACING);
        int draw_width = std::min(met, static_cast<int>(METER_BAR_GREEN_WIDTH));
        if (draw_width > 0) {
            ::tft.fillRect(current_bar_x, bounds.y + METER_BAR_Y, draw_width, METER_BAR_HEIGHT, TFT_GREEN);
            end_of_colored_x_abs = current_bar_x + draw_width;
        }
        met -= METER_BAR_GREEN_WIDTH; // Teljes zöld sáv "költségét" levonjuk
        tik++;                        // Lépünk a következő sávra
    }

    // Utolsó, S9+60dB feletti narancs sáv rajzolása
    // Ha elértük az összes S és S9+dB sáv végét (tik == MeterBarTotalLimit) ÉS még mindig van 'met' (energia).
    if (tik == METER_BAR_TOTAL_LIMIT && met > 0) {
        int draw_width = std::min(met, static_cast<int>(METER_BAR_FINAL_ORANGE_WIDTH));
        if (draw_width > 0) {
            ::tft.fillRect(bounds.x + METER_BAR_FINAL_ORANGE_START_X, bounds.y + METER_BAR_Y, draw_width, METER_BAR_HEIGHT, TFT_ORANGE);
            end_of_colored_x_abs = bounds.x + METER_BAR_FINAL_ORANGE_START_X + draw_width;
        }
        // met -= METER_BAR_FINAL_ORANGE_WIDTH; // Itt már nem kell csökkenteni, mert ez az utolsó lehetséges színes sáv.
    }

    // A mérősáv teljes definiált végének X koordinátája (ahol a fekete kitöltésnek véget kell érnie).
    int meter_display_area_end_x_abs = bounds.x + METER_BAR_RED_START_X + METER_BAR_MAX_PIXEL_VALUE;

    // Biztosítjuk, hogy a kirajzolt színes rész ne lógjon túl a definiált maximális értéken.
    if (end_of_colored_x_abs > meter_display_area_end_x_abs) {
        end_of_colored_x_abs = meter_display_area_end_x_abs;
    }
    // Ha spoint=0 volt, akkor semmi sem rajzolódott, end_of_colored_x_abs a skála elején maradt.
    if (spoint == 0) {
        end_of_colored_x_abs = bounds.x + METER_BAR_RED_START_X;
    }

    // Fekete kitöltés: az utolsó színes sáv végétől a skála definiált végéig.
    // Csak akkor rajzolunk feketét, ha a színes sáv nem érte el a skála végét.
    if (end_of_colored_x_abs < meter_display_area_end_x_abs) {
        ::tft.fillRect(end_of_colored_x_abs, bounds.y + METER_BAR_Y, meter_display_area_end_x_abs - end_of_colored_x_abs, METER_BAR_HEIGHT, TFT_BLACK);
    }
}

/**
 * TFT alapállapot beállítása szöveg rajzolásához.
 * @param color Szöveg színe
 * @param background Háttér színe
 */
void SMeter::setupTextTFT(uint16_t color, uint16_t background) {
    ::tft.setFreeFont();
    ::tft.setTextSize(1);
    ::tft.setTextColor(color, background);
}

/**
 * S-Meter skála kirajzolása (a statikus részek: vonalak, számok).
 * Ezt általában egyszer kell meghívni a képernyő inicializálásakor.
 */
void SMeter::drawSmeterScale() {

    using namespace SMeterConstants;

    // Ha már inicializáltuk a pozíciókat, ne rajzoljuk újra (opcionális optimalizáció)
    if (textLayout.initialized) {
        return; // Már inicializált, nem kell újra kirajzolni
    }

    // A skála teljes területének törlése feketével (beleértve a szöveg helyét is)
    ::tft.fillRect(bounds.x + SCALE_START_X_OFFSET, bounds.y + SCALE_START_Y_OFFSET, SCALE_WIDTH, SCALE_HEIGHT + 10, TFT_BLACK);

    setupTextTFT(TFT_WHITE, TFT_BLACK); // Szövegszín: fehér, Háttér: fekete

    // S-pont skála vonalak és számok (0-9)
    for (int i = 0; i < SPOINT_COUNT; i++) {
        ::tft.fillRect(bounds.x + SPOINT_START_X + (i * SPOINT_SPACING), bounds.y + SPOINT_Y, SPOINT_TICK_WIDTH, SPOINT_TICK_HEIGHT, TFT_WHITE);
        // setCursor + print használata a konzisztencia érdekében
        int textX = bounds.x + SPOINT_START_X + (i * SPOINT_SPACING) - 3; // Központosítás
        int textY = bounds.y + SPOINT_NUMBER_Y;
        ::tft.setCursor(textX, textY);
        ::tft.print(i);
    }
    // S9+dB skála vonalak és számok (+10, +20, ..., +60)
    for (int i = 1; i <= PLUS_SCALE_COUNT; i++) {
        ::tft.fillRect(bounds.x + PLUS_SCALE_START_X + (i * PLUS_SCALE_SPACING), bounds.y + PLUS_SCALE_Y, PLUS_SCALE_TICK_WIDTH, PLUS_SCALE_TICK_HEIGHT, TFT_RED);
        if (i % 2 == 0) {                                                             // Csak minden másodiknál írjuk ki a "+számot" (pl. +20, +40, +60)
            int textX = bounds.x + PLUS_SCALE_START_X + (i * PLUS_SCALE_SPACING) - 8; // Központosítás
            int textY = bounds.y + PLUS_SCALE_NUMBER_Y;
            ::tft.setCursor(textX, textY);
            ::tft.print("+");
            ::tft.print(i * 10);
        }
    }

    // Skála alatti vízszintes sávok
    ::tft.fillRect(bounds.x + SPOINT_START_X, bounds.y + SBAR_Y, SBAR_SPOINT_WIDTH, SBAR_HEIGHT, TFT_WHITE); // S0-S9 sáv
    ::tft.fillRect(bounds.x + SBAR_PLUS_START_X, bounds.y + SBAR_Y, SBAR_PLUS_WIDTH, SBAR_HEIGHT, TFT_RED);  // S9+dB sáv

    // Statikus RSSI és SNR feliratok kirajzolása - komplett TFT állapot újrabeállítása
    textLayout.text_y_pos = bounds.y + SCALE_END_Y_OFFSET + 2;
    uint16_t current_x_calc = bounds.x + RSSI_LABEL_X_OFFSET; // Teljes TFT állapot tisztítása és újrabeállítása a címkékhez
    setupTextTFT(TFT_GREEN, colors.background);               // Feliratok színe - UIComponent színpaletta használata
    textLayout.text_h = ::tft.fontHeight();                   // Szöveg magassága a törléshez

    // RSSI Felirat - setCursor + print használata
    const char *rssi_label_text = "RSSI: ";
    ::tft.setCursor(current_x_calc, textLayout.text_y_pos);
    ::tft.print(rssi_label_text);
    textLayout.rssi_label_x_pos = current_x_calc;
    textLayout.rssi_value_x_pos = current_x_calc + ::tft.textWidth(rssi_label_text);
    textLayout.rssi_value_max_w = ::tft.textWidth("XXX dBuV");                       // Max lehetséges szélesség
    current_x_calc = textLayout.rssi_value_x_pos + textLayout.rssi_value_max_w + 10; // 10px rés

    // SNR Felirat - setCursor + print használata
    const char *snr_label_text = "SNR: ";
    ::tft.setCursor(current_x_calc, textLayout.text_y_pos);
    ::tft.print(snr_label_text);
    textLayout.snr_label_x_pos = current_x_calc;
    textLayout.snr_value_x_pos = current_x_calc + ::tft.textWidth(snr_label_text);
    textLayout.snr_value_max_w = ::tft.textWidth("XXX dB"); // Max lehetséges szélesség

    // Jelöljük, hogy a skála már inicializálva van
    textLayout.initialized = true;
}

/**
 * S-Meter érték és RSSI/SNR szöveg megjelenítése.
 * @param rssi Aktuális RSSI érték (0–127 dBμV).
 * @param snr Aktuális SNR érték (0–127 dB).
 * @param isFMMode Igaz, ha FM módban vagyunk, hamis egyébként (AM/SSB/CW).
 */
void SMeter::showRSSI(uint8_t rssi, uint8_t snr, bool isFMMode) {

    // 1. Dinamikus S-Meter sávok kirajzolása az aktuális RSSI alapján
    drawMeterBars(rssi, isFMMode); // Ez már tartalmazza a prev_spoint_bars optimalizációt

    // 2. Ellenőrizzük, hogy a pozíciók inicializálva vannak-e
    if (!textLayout.initialized) {
        // Ha a pozíciók még nincsenek beállítva, inicializáljuk a skálát
        drawSmeterScale();
    }

    // 3. RSSI és SNR értékek szöveges kiírása, csak ha változott az értékük
    bool rssi_changed = (rssi != prev_rssi_for_text);
    bool snr_changed = (snr != prev_snr_for_text);
    if (!rssi_changed && !snr_changed)
        return; // Ha semmi sem változott, kilépünk

    // Font és egyéb beállítások az értékekhez - setupTextTFT használata
    setupTextTFT(TFT_WHITE, colors.background); // Értékek színe: fehér, háttér: UIComponent színpaletta

    if (rssi_changed) {
        char rssi_str_buff[12]; // "XXX dBuV" + null
        snprintf(rssi_str_buff, sizeof(rssi_str_buff), "%3d dBuV", rssi);
        // Régi érték területének törlése
        ::tft.fillRect(textLayout.rssi_value_x_pos, textLayout.text_y_pos, textLayout.rssi_value_max_w, textLayout.text_h, colors.background);
        // Új érték kirajzolása - setCursor + print használata
        ::tft.setCursor(textLayout.rssi_value_x_pos, textLayout.text_y_pos);
        ::tft.print(rssi_str_buff);
    }
    if (snr_changed) {
        char snr_str_buff[10]; // "XXX dB" vagy "  ---" + null

        // AM/SSB/CW módban gyakran nincs értelmes SNR érték (chip korlát)
        // Csak FM módban vagy ha van jelentős SNR érték, jelenítjük meg
        if (isFMMode || snr > 5) {
            snprintf(snr_str_buff, sizeof(snr_str_buff), "%3d dB", snr);
        } else {
            snprintf(snr_str_buff, sizeof(snr_str_buff), "  ---");
        }

        // Régi érték területének törlése
        ::tft.fillRect(textLayout.snr_value_x_pos, textLayout.text_y_pos, textLayout.snr_value_max_w, textLayout.text_h, colors.background);
        // Új érték kirajzolása - setCursor + print használata
        ::tft.setCursor(textLayout.snr_value_x_pos, textLayout.text_y_pos);
        ::tft.print(snr_str_buff);
    }

    // Elmentjük az aktuális numerikus értékeket a következő összehasonlításhoz
    if (rssi_changed) {
        prev_rssi_for_text = rssi;
    }
    if (snr_changed) {
        prev_snr_for_text = snr;
    }
}

/**
 * @brief Kirajzolja a komponenst (UIComponent override)
 *
 * Ez a metódus implementálja az UIComponent draw() interfészét.
 * Inicializáláskor rajzolja ki a teljes skálát.
 */
void SMeter::draw() {
#ifdef __DEBUG
    // Debug: Piros keret rajzolás a komponens határainak vizualizálásához
    ::tft.drawRect(bounds.x, bounds.y, bounds.width, bounds.height, TFT_RED);
#endif

    if (needsRedraw) {
        drawSmeterScale();
        needsRedraw = false;
    }
}

/**
 * @brief Újrarajzolásra jelölés - reset-eli az initialized flag-et
 * @details Dialóg bezárása vagy képernyő törlése után szükséges a statikus skála újrarajzolása.
 * Ez a metódus biztosítja, hogy a drawSmeterScale() újra kirajzolja a statikus elemeket.
 */
void SMeter::markForRedraw(bool markChildren) {
    // Szülő implementáció meghívása (beállítja a needsRedraw flag-et)
    UIComponent::markForRedraw(markChildren);

    // SMeter specifikus: reset-eljük az initialized flag-et
    // Ez kikényszeríti a statikus skála újrarajzolását
    textLayout.initialized = false;
}
