#include "RDSComponent.h"
#include "Si4735Manager.h"

// ===================================================================
// PTY (Program Type) tábla - Statikus definíciók
// ===================================================================

/**
 * @brief RDS Program Type (PTY) nevek táblája
 * @details Az RDS standard 32 különböző program típust definiál (0-31).
 */
const char *RDSComponent::RDS_PTY_NAMES[] = {
    "No defined",            // 0
    "News",                  // 1
    "Current affairs",       // 2
    "Information",           // 3
    "Sport",                 // 4
    "Education",             // 5
    "Drama",                 // 6
    "Culture",               // 7
    "Science",               // 8
    "Varied",                // 9
    "Pop Music",             // 10
    "Rock Music",            // 11
    "Easy Listening",        // 12
    "Light Classical",       // 13
    "Serious Classical",     // 14
    "Other Music",           // 15
    "Weather",               // 16
    "Finance",               // 17
    "Children's Programmes", // 18
    "Social Affairs",        // 19
    "Religion",              // 20
    "Phone-in",              // 21
    "Travel",                // 22
    "Leisure",               // 23
    "Jazz Music",            // 24
    "Country Music",         // 25
    "National Music",        // 26
    "Oldies Music",          // 27
    "Folk Music",            // 28
    "Documentary",           // 29
    "Alarm Test",            // 30
    "Alarm"                  // 31
};

const uint8_t RDSComponent::RDS_PTY_COUNT = sizeof(RDSComponent::RDS_PTY_NAMES) / sizeof(RDSComponent::RDS_PTY_NAMES[0]);

// ===================================================================
// PTY konverzió
// ===================================================================

/**
 * @brief PTY kód konvertálása szöveges leírássá
 * @param ptyCode A PTY kód (0-31)
 * @return String A PTY szöveges leírása
 */
String RDSComponent::convertPtyCodeToString(uint8_t ptyCode) {
    if (ptyCode < RDS_PTY_COUNT) {
        return String(RDS_PTY_NAMES[ptyCode]);
    }
    return "Unknown PTY";
}

// ===================================================================
// Konstruktor és inicializálás
// ===================================================================

/**
 * @brief RDSComponent konstruktor
 */
RDSComponent::RDSComponent(const Rect &bounds)
    : UIComponent(bounds, ColorScheme::defaultScheme()), lastScrollUpdate(0), dataChanged(false), scrollSprite(nullptr), scrollOffset(0), radioTextPixelWidth(0), needsScrolling(false), scrollSpriteCreated(false) {
    // Alapértelmezett színek beállítása
    stationNameColor = TFT_CYAN;   // Állomásnév - cián
    programTypeColor = TFT_ORANGE; // Program típus - narancs
    radioTextColor = TFT_WHITE;    // Radio text - fehér
    dateTimeColor = TFT_YELLOW;    // Dátum/idő - sárga
    backgroundColor = TFT_BLACK;   // Háttér - fekete

    // Alapértelmezett pozíciók beállítása (a jelenlegi elrendezés alapján)
    calculateDefaultLayout();

    // Kezdetben nincs RDS adat
    dataChanged = false;
}

/**
 * @brief Destruktor - erőforrások felszabadítása
 */
RDSComponent::~RDSComponent() { cleanupScrollSprite(); }

// ===================================================================
// Layout és konfigurálás
// ===================================================================

/**
 * @brief Alapértelmezett layout számítása - most abszolút pozíciókkal
 * @details A részkomponensek alapértelmezett pozíciói.
 * Ezek felülírhatók a set...Area() metódusokkal.
 */
void RDSComponent::calculateDefaultLayout() {
    // Alapértelmezett pozíciók - ezek később felülírhatók
    const uint16_t defaultY = 150; // Alapértelmezett Y pozíció
    const uint16_t margin = 10;
    const uint16_t lineHeight = 18;
    const uint16_t dateTimeWidth = 85;
    const uint16_t stationNameWidth = 200;

    // Állomásnév - bal oldal
    stationNameArea = Rect(margin, defaultY, stationNameWidth, lineHeight);

    // Program típus - középen
    programTypeArea = Rect(220, defaultY, 150, lineHeight);

    // Dátum/idő - jobb oldal
    dateTimeArea = Rect(380, defaultY, dateTimeWidth, lineHeight);

    // Radio text - alsó sor
    radioTextArea = Rect(margin, defaultY + 20, 460, lineHeight);
}

/**
 * @brief Állomásnév területének beállítása
 */
void RDSComponent::setStationNameArea(const Rect &area) { stationNameArea = area; }

/**
 * @brief Program típus területének beállítása
 */
void RDSComponent::setProgramTypeArea(const Rect &area) { programTypeArea = area; }

/**
 * @brief Radio text területének beállítása
 */
void RDSComponent::setRadioTextArea(const Rect &area) {
    radioTextArea = area;
    // Ha változott a terület, újra kell inicializálni a scroll sprite-ot
    if (scrollSpriteCreated) {
        cleanupScrollSprite();
        initializeScrollSprite();
    }
}

/**
 * @brief Dátum/idő területének beállítása
 */
void RDSComponent::setDateTimeArea(const Rect &area) { dateTimeArea = area; }

/**
 * @brief RDS színek testreszabása
 */
void RDSComponent::setRdsColors(uint16_t stationColor, uint16_t typeColor, uint16_t textColor, uint16_t timeColor, uint16_t bgColor) {
    stationNameColor = stationColor;
    programTypeColor = typeColor;
    radioTextColor = textColor;
    dateTimeColor = timeColor;
    backgroundColor = bgColor;

    // Ha már létezik scroll sprite, frissítjük a színeit
    if (scrollSprite && scrollSpriteCreated) {
        scrollSprite->setTextColor(radioTextColor, backgroundColor);
    }
}

// ===================================================================
// Scroll sprite kezelés
// ===================================================================

/**
 * @brief Radio text feldolgozása - többszörös szóközök kezelése
 * @param radioText A feldolgozandó radio text
 * @return String A feldolgozott radio text
 */
String RDSComponent::processRadioText(const String &radioText) {
    if (radioText.isEmpty()) {
        return radioText;
    }

    String result = radioText;
    int spaceCount = 0;
    int cutPosition = -1;

    // Végigmegyünk a stringen és keressük a többszörös szóközöket
    for (int i = 0; i < result.length(); i++) {
        if (result.charAt(i) == ' ') {
            spaceCount++;
            // Ha több mint 2 egymás utáni szóköz van
            if (spaceCount > 2 && cutPosition == -1) {
                // Az első szóköznél vágjuk le (i - spaceCount + 1 az első szóköz pozíciója)
                cutPosition = i - spaceCount + 1;
                break;
            }
        } else {
            spaceCount = 0; // Reset, ha nem szóköz karakter
        }
    }

    // Ha találtunk levágási pontot, levágjuk a stringet
    if (cutPosition >= 0) {
        result = result.substring(0, cutPosition);
        result.trim(); // Eltávolítjuk a felesleges szóközöket a végéről

        // Debug kimenet - előtte-utána string megjelenítése
        DEBUG("RDSComponent::processRadioText() -> space cutter -  Előtte: '%s' Utána: '%s'", radioText.c_str(), result.c_str());
    }

    return result;
}

/**
 * @brief Scroll sprite inicializálása
 */
void RDSComponent::initializeScrollSprite() {
    if (scrollSprite || scrollSpriteCreated) {
        cleanupScrollSprite();
    }
    if (radioTextArea.width > 0 && radioTextArea.height > 0) {
        scrollSprite = new TFT_eSprite(&tft);
        if (scrollSprite->createSprite(radioTextArea.width, radioTextArea.height)) {
            scrollSprite->setFreeFont(); // Alapértelmezett font
            scrollSprite->setTextSize(2);
            scrollSprite->setTextColor(radioTextColor, backgroundColor);
            scrollSprite->setTextDatum(TL_DATUM);
            scrollSpriteCreated = true;
        } else {
            delete scrollSprite;
            scrollSprite = nullptr;
            scrollSpriteCreated = false;
        }
    }
}

/**
 * @brief Scroll sprite felszabadítása
 */
void RDSComponent::cleanupScrollSprite() {
    if (scrollSprite) {
        if (scrollSpriteCreated) {
            scrollSprite->deleteSprite();
        }
        delete scrollSprite;
        scrollSprite = nullptr;
        scrollSpriteCreated = false;
    }
}

// ===================================================================
// RDS adatok kezelése
// ===================================================================

/**
 * @brief RDS adatok frissítése a Si4735Manager-től
 */
void RDSComponent::updateRdsData() {

    // Az Si4735Rds osztály cache funkcionalitását használjuk
    dataChanged = ::pSi4735Manager->updateRdsDataWithCache(); // Ha változott a radio text, újraszámítjuk a scroll paramétereket
    if (dataChanged) {
        String newRadioText = ::pSi4735Manager->getCachedRadioText();
        if (!newRadioText.isEmpty()) {
            // Radio text feldolgozása - ha több mint 2 egymás utáni szóköz van, levágás az elsőnél
            String processedRadioText = processRadioText(newRadioText);

            // Radio text változott - scroll újraszámítás
            tft.setFreeFont();
            tft.setTextSize(2);
            radioTextPixelWidth = tft.textWidth(processedRadioText);
            needsScrolling = (radioTextPixelWidth > radioTextArea.width);
            scrollOffset = 0; // Scroll restart
        } else {
            needsScrolling = false;
        }
    }
}

// ===================================================================
// Rajzolási metódusok
// ===================================================================

/**
 * @brief Állomásnév kirajzolása
 */
void RDSComponent::drawStationName() {
    String stationName = ::pSi4735Manager->getCachedStationName();

    // Terület törlése
    tft.fillRect(stationNameArea.x, stationNameArea.y, stationNameArea.width, stationNameArea.height, backgroundColor);

#ifdef DRAW_DEBUG_FRAMES
    // DEBUG KERET - piros
    tft.drawRect(stationNameArea.x, stationNameArea.y, stationNameArea.width, stationNameArea.height, TFT_RED);
#endif

    if (stationName.isEmpty()) {
        return; // Nincs megjeleníthető adat
    }
    tft.setFreeFont();
    tft.setTextSize(3);
    tft.setTextColor(stationNameColor, backgroundColor);
    tft.setTextDatum(MC_DATUM); // Middle Center - vízszintesen és függőlegesen is középre igazítva

    // Szöveg kirajzolása - terület közepére (vízszintes és függőleges központ)
    int16_t centerX = stationNameArea.x + stationNameArea.width / 2;
    int16_t centerY = stationNameArea.y + stationNameArea.height / 2;
    tft.drawString(stationName, centerX, centerY);
}

/**
 * @brief Program típus kirajzolása
 */
void RDSComponent::drawProgramType() {
    String programType = ::pSi4735Manager->getCachedProgramType();

    // Terület törlése
    tft.fillRect(programTypeArea.x, programTypeArea.y, programTypeArea.width, programTypeArea.height, backgroundColor);

#ifdef DRAW_DEBUG_FRAMES
    // DEBUG KERET - zöld
    tft.drawRect(programTypeArea.x, programTypeArea.y, programTypeArea.width, programTypeArea.height, TFT_GREEN);
#endif

    if (programType.isEmpty()) {
        return; // Nincs megjeleníthető adat
    }
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextColor(programTypeColor, backgroundColor);
    tft.setTextDatum(MC_DATUM); // Middle Center - vízszintesen és függőlegesen is középre igazítva

    // Szöveg kirajzolása - terület közepére (vízszintes és függőleges központ)
    int16_t centerX = programTypeArea.x + programTypeArea.width / 2;
    int16_t centerY = programTypeArea.y + programTypeArea.height / 2;
    tft.drawString(programType, centerX, centerY);
}

/**
 * @brief Radio text kirajzolása (scroll támogatással)
 */
void RDSComponent::drawRadioText() {
    String radioText = ::pSi4735Manager->getCachedRadioText();

    // Radio text feldolgozása - többszörös szóközök kezelése
    String processedRadioText = processRadioText(radioText);

    // Terület törlése
    tft.fillRect(radioTextArea.x, radioTextArea.y, radioTextArea.width, radioTextArea.height, backgroundColor);

    // DEBUG KERET - sárga
    tft.drawRect(radioTextArea.x, radioTextArea.y, radioTextArea.width, radioTextArea.height, TFT_YELLOW);

    if (processedRadioText.isEmpty()) {
        return;
    }
    if (!needsScrolling) {
        // Egyszerű megjelenítés, ha elfér
        tft.setFreeFont();
        tft.setTextSize(2);
        tft.setTextColor(radioTextColor, backgroundColor);
        tft.setTextDatum(ML_DATUM); // Middle Left - függőlegesen középre, balra igazítva

        // Szöveg kirajzolása - függőlegesen középre + 1px gap
        int16_t centerY = radioTextArea.y + radioTextArea.height / 2;
        tft.drawString(processedRadioText, radioTextArea.x + 5, centerY); // +5px gap a bal oldaltól
    } else {
        // Scroll esetén sprite használata
        if (!scrollSpriteCreated) {
            initializeScrollSprite();
        }

        if (scrollSprite && scrollSpriteCreated) {
            handleRadioTextScroll();
        }
    }
}

/**
 * @brief Dátum és idő kirajzolása
 */
void RDSComponent::drawDateTime() {
    String dateTime = ::pSi4735Manager->getCachedDateTime();

    // Háttér törlése
    tft.fillRect(dateTimeArea.x, dateTimeArea.y, dateTimeArea.width, dateTimeArea.height, backgroundColor);

#ifdef DRAW_DEBUG_FRAMES
    // DEBUG KERET - kék
    tft.drawRect(dateTimeArea.x, dateTimeArea.y, dateTimeArea.width, dateTimeArea.height, TFT_BLUE);
#endif
    if (dateTime.isEmpty()) {
        return;
    }
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextColor(dateTimeColor, backgroundColor);
    tft.setTextDatum(ML_DATUM); // Middle Left - függőlegesen középre, balra igazítva

    // Szöveg kirajzolása - függőlegesen középre + 1px gap a bal oldaltól
    int16_t centerY = dateTimeArea.y + dateTimeArea.height / 2;
    tft.drawString(dateTime, dateTimeArea.x + 1, centerY); // +1px gap a bal oldaltól
}

/**
 * @brief Radio text scroll kezelése
 */
void RDSComponent::handleRadioTextScroll() {
    if (!scrollSprite || !scrollSpriteCreated || !needsScrolling) {
        return;
    }

    uint32_t currentTime = millis();
    if (currentTime - lastScrollUpdate < SCROLL_INTERVAL_MS) {
        return;
    }
    lastScrollUpdate = currentTime;

    // Sprite törlése
    scrollSprite->fillScreen(backgroundColor); // Aktuális radio text lekérése és feldolgozása
    String radioText = ::pSi4735Manager->getCachedRadioText();
    String processedRadioText = processRadioText(radioText);

    // Fő szöveg rajzolása (balra mozog)
    scrollSprite->drawString(processedRadioText, -scrollOffset, 0);

    // Ha szükséges, "újra beúszó" szöveg rajzolása
    const int gapPixels = radioTextArea.width; // Szóköz a szöveg vége és újrakezdés között
    int secondTextX = -scrollOffset + radioTextPixelWidth + gapPixels;

    if (secondTextX < radioTextArea.width) {
        scrollSprite->drawString(processedRadioText, secondTextX, 0);
    }

    // Sprite kirakása a képernyőre
    scrollSprite->pushSprite(radioTextArea.x, radioTextArea.y);

    // Scroll pozíció frissítése
    scrollOffset += SCROLL_STEP_PIXELS;

    // Ciklus újraindítása
    if (scrollOffset >= radioTextPixelWidth + gapPixels) {
        scrollOffset = 0;
    }
}

// ===================================================================
// UIComponent interface implementáció
// ===================================================================

/**
 * @brief Komponens teljes újrarajzolása
 * @details Most már nem rajzol közös keretet vagy hátteret,
 * mivel a részkomponensek függetlenül pozícionálhatók
 */
void RDSComponent::draw() {
    // Minden elem újrarajzolása a saját pozíciójukban
    drawStationName();
    drawProgramType();
    drawRadioText();
    drawDateTime();

    // Clear the redraw flag after drawing
    needsRedraw = false;
}

/**
 * @brief Újrarajzolásra jelölés
 */
void RDSComponent::markForRedraw(bool markChildren) {
    UIComponent::markForRedraw(markChildren);
    // Nem állítjuk be a dataChanged-et automatikusan
    // Az csak akkor legyen true, ha tényleg változtak az RDS adatok
}

// ===================================================================
// Publikus interface
// ===================================================================

/**
 * @brief RDS adatok frissítése (loop-ban hívandó)
 */
void RDSComponent::updateRDS() {

    // Adaptív frissítési időköz figyelembevételével frissítjük az RDS adatokat
    updateRdsData();

    // Ha a UIComponent szintjén újrarajzolás szükséges, akkor teljes újrarajzolás
    if (isRedrawNeeded()) {
        draw();
        needsRedraw = false; // Fontos: töröljük a flag-et
        return;
    }

    // Egyébként csak akkor rajzoljuk újra, ha változtak az adatok
    if (dataChanged) {
        // Csak az érintett részek újrarajzolása (nem a keret!)
        drawStationName();
        drawProgramType();
        drawRadioText();
        drawDateTime();
        dataChanged = false;
    }

    // Scroll kezelése még akkor is, ha nincs adatváltozás
    if (needsScrolling) {
        handleRadioTextScroll();
    }
}

/**
 * @brief RDS adatok törlése
 */
void RDSComponent::clearRDS() {
    // Si4735Rds cache törlése
    ::pSi4735Manager->clearRdsCache();

    // UI állapot resetelés
    needsScrolling = false;
    scrollOffset = 0;

    draw(); // Teljes törlés

    cleanupScrollSprite();
}

/**
 * @brief RDS cache törlése frekvencia változáskor
 * @details Azonnal törli az összes RDS adatot és reseteli az időzítőket.
 * Használatos frekvencia váltáskor, amikor az RDS adatok már nem érvényesek.
 */
void RDSComponent::clearRdsOnFrequencyChange() {
    // Si4735Rds cache törlése
    ::pSi4735Manager->clearRdsCache();

    // UI állapot resetelés
    dataChanged = true;
    needsScrolling = false;
    scrollOffset = 0;

    // Sprite tisztítás
    cleanupScrollSprite();

    // Képernyő frissítés
    markForRedraw(false);
}

/**
 * @brief Ellenőrzi, hogy van-e érvényes RDS adat
 */
bool RDSComponent::hasValidRDS() const {
    return ::pSi4735Manager->isRdsAvailable() &&                      //
           (                                                          //
               !::pSi4735Manager->getCachedStationName().isEmpty()    //
               || !::pSi4735Manager->getCachedProgramType().isEmpty() //
               || !::pSi4735Manager->getCachedRadioText().isEmpty()   //
               || !::pSi4735Manager->getCachedDateTime().isEmpty()    //
           );
}
