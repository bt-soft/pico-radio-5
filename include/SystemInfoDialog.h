#pragma once

#include "MessageDialog.h"
#include "PicoMemoryInfo.h"
#include "StationData.h"
#include "defines.h"

/**
 * @brief Rendszer információ dialógus - MessageDialog alapú implementáció
 * @details Átfogó rendszer információkat jelenít meg angolul kategóriákba szervezve:
 * - Program információk (név, verzió, szerző, fordítás dátuma)
 * - Memória használat (Flash, RAM, EEPROM) valós időben
 * - Hardware információk (MCU, kijelző specifikációk)
 * - Si4735 rádió modul információk (jelenleg helykitöltő)
 *
 * Főbb jellemzők:
 * - 4 különböző oldal navigációs gombokkal
 * - Previous/Next lapozás lehetősége
 * - Mini font használata minden gombon
 * - OK gomb automatikus kezelése a MessageDialog örökléssel
 * - Valós idejű memória állapot lekérés
 */
class SystemInfoDialog : public MessageDialog {

  public:
    // Konstansok

    /**
     * @brief Az összes oldal száma a dialógusban
     * @details 4 oldal van összesen: Program (0), Memory (1), Hardware (2), Radio (3)
     */
    static constexpr int TOTAL_PAGES = 4;

    /**
     * @brief SystemInfoDialog konstruktor
     * @param parentScreen Szülő képernyő, amely ezt a dialógust birtokolja
     * @param tft TFT kijelző referencia a rajzoláshoz
     * @param bounds Dialógus területének koordinátái és méretei
     * @param cs Színséma a dialógus megjelenítéséhez (opcionális, alapértelmezett a defaultScheme)
     */
    SystemInfoDialog(UIScreen *parentScreen, const Rect &bounds, const ColorScheme &cs = ColorScheme::defaultScheme());

    /**
     * @brief Virtuális destruktor
     * @details Az alapértelmezett destruktor elegendő, mivel a shared_ptr automatikusan
     * felszabadítja a navigációs gombokat.
     */
    virtual ~SystemInfoDialog() = default;

  protected:
    /**
     * @brief A dialógus teljes újrarajzolása felüldefiniálva
     * @details Felülírja a MessageDialog::drawSelf() metódust, hogy custom tartalmat
     * rajzoljon ki az üzenet helyett. Kezeli az oldalszám megjelenítését, a szöveges
     * tartalom formázását és az összes gomb újrarajzolását.
     */
    virtual void drawSelf() override;

    /**
     * @brief A dialógus tartalmának elrendezése felüldefiniálva
     * @details Felülírja a MessageDialog::layoutDialogContent() metódust, hogy
     * létrehozza a navigációs gombokat és mini fontot állítson be minden gombra,
     * beleértve az OK gombot is.
     */
    virtual void layoutDialogContent() override;

    /**
     * @brief Visszaadja az összes dialógus gombot (kivéve a bezáró X gombot).
     * @return A gombok listája shared_ptr-ekben.
     * @details Felülírja a UIDialogBase virtuális metódusát. A SystemInfoDialog esetében
     * ez magában foglalja a MessageDialog gombokat (OK) és a navigációs gombokat (Previous, Next).
     */
    virtual std::vector<std::shared_ptr<UIButton>> getButtonsList() const override;

  private:
    // Lapozási állapot és navigáció
    uint8_t currentPage; ///< Aktuális oldal száma (0-3)

    // Navigációs UI gombok
    std::shared_ptr<UIButton> prevButton; ///< "< Prev" gomb az előző oldalra lépéshez
    std::shared_ptr<UIButton> nextButton; ///< "Next >" gomb a következő oldalra lépéshez    // Adatok összegyűjtési és formázási módszerek

    /**
     * @brief Az aktuális oldal tartalmának lekérése
     * @return Formázott string az aktuális oldal adataival
     * @details A currentPage változó alapján visszaadja a megfelelő oldal tartalmát.
     */
    String getCurrentPageContent();

    /**
     * @brief Program információk formázása (1. oldal)
     * @return Formázott string a program nevével, verziójával, szerzőjével és build idővel
     */
    String formatProgramInfo();

    /**
     * @brief Memória információk formázása (2. oldal)
     * @return Formázott string a Flash, RAM és EEPROM használattal valós időben
     */
    String formatMemoryInfo();

    /**
     * @brief Hardware információk formázása (3. oldal)
     * @return Formázott string az MCU és kijelző specifikációkkal
     */
    String formatHardwareInfo();

    /**
     * @brief Si4735 rádio chip információk formázása (4. oldal)
     * @return Formázott string a rádio chip adataival (jelenleg helykitöltő)
     */
    String formatSi4735Info();

    // Segéd módszerek

    /**
     * @brief Navigációs gombok állapotának frissítése
     * @details Az aktuális oldal alapján engedélyezi/letiltja a Previous és Next gombokat.
     * Minden oldal váltás után meghívódik.
     */
    void updateNavigationButtons();
};
