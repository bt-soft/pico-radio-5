#ifndef __BAND_H
#define __BAND_H

#include "Config.h"
#include "defines.h"
#include "rtVars.h"

// Forward declaration
class BandStore;

// Band index
#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3

// DeModulation types
#define FM_DEMOD_TYPE 0
#define LSB_DEMOD_TYPE 1
#define USB_DEMOD_TYPE 2
#define AM_DEMOD_TYPE 3
#define CW_DEMOD_TYPE 4

// Egységes BandTable struktúra
struct BandTable {
    const char *bandName; // Sáv neve
    uint8_t bandType;     // Sáv típusa (FM, MW, LW vagy SW)
    uint16_t prefDemod;   // Preferált demodulációs mód (AM, FM, USB, LSB, CW)
    uint16_t minimumFreq; // A sáv minimum frekvenciája
    uint16_t maximumFreq; // A sáv maximum frekvenciája
    uint16_t defFreq;     // Alapértelmezett frekvencia
    uint8_t defStep;      // Alapértelmezett lépésköz (növelés/csökkentés)
    bool isHam;           // HAM sáv-e?

    // Változó és mentendő adatok
    uint16_t currFreq; // Aktuális frekvencia
    uint8_t currStep;  // Aktuális lépésköz (növelés/csökkentés)
    uint8_t currDemod; // Aktuális demodulációs mód (FM, AM, LSB, USB, CW)
    uint16_t antCap;   // Antenna Tuning Capacitor
};

// Sávszélesség struktúra (Címke és Érték)
struct BandWidth {
    const char *label; // Megjelenítendő felirat
    uint8_t index;     // Az si4735-nek átadandó index
};

// Lépésköz struktúra (Címke és Érték)
struct FrequencyStep {
    const char *label; // Megjelenítendő felirat (pl. "1kHz")
    uint8_t value;     // A lépésköz értéke (pl. 1, 5, 9, 10, 100)
};

/**
 * Band class
 */
class Band {
  private:
    // BandStore pointer a mentett adatok kezeléséhez
    BandStore *bandStore;

  public:
    // BandMode description
    static const char *bandModeDesc[5];

    // Sávszélesség struktúrák tömbjei
    static const BandWidth bandWidthFM[5];
    static const BandWidth bandWidthAM[7];
    static const BandWidth bandWidthSSB[6];

    // Lépésköz konfigurációk (érték beállításához)
    static const FrequencyStep stepSizeAM[4];
    static const FrequencyStep stepSizeFM[3];
    static const FrequencyStep stepSizeBFO[4];
    Band();
    virtual ~Band() = default;

    /**
     * @brief BandStore beállítása
     * @param store A BandStore objektum pointere
     */
    void setBandStore(BandStore *store) { bandStore = store; }

    /**
     * @brief Band tábla dinamikus adatainak egyszeri inicializálása
     * @details Ezt a metódust csak egyszer kell meghívni az alkalmazás indításakor!
     * @param forceReinit Ha true, akkor újrainicializálja a dinamikus adatokat, függetlenül a jelenlegi állapotuktól
     */
    void initializeBandTableData(bool forceReinit = false);

    /**
     * @brief Band adatok mentése a BandStore-ba
     */
    void saveBandData();

    /**
     * @brief Band adatok betöltése a BandStore-ból
     */
    void loadBandData();

    /**
     * @brief A Default Antenna Tuning Capacitor értékének lekérdezése
     * @return Az alapértelmezett antenna tuning capacitor értéke
     */
    inline uint16_t getDefaultAntCapValue() {

        // Kikeressük az aktuális Band rekordot
        switch (getCurrentBandType()) {

            case SW_BAND_TYPE:
                return 1; // SW band esetén antenna tuning capacitor szükséges

            case FM_BAND_TYPE:
            case MW_BAND_TYPE:
            case LW_BAND_TYPE:
            default:
                return 0; // FM és sima AM esetén nem kell antenna tuning capacitor
        }
    }

    /**
     * @brief A Band egy rekordjának elkérése az index alapján
     * @param bandIdx a band indexe
     * @return A BandTable rekord referenciája, vagy egy üres rekord, ha nem található
     */
    BandTable &getBandByIdx(uint8_t bandIdx);

    /**
     * @brief A jelenlegi BandType lekérdezése
     * @return A jelenlegi BandType (FM, MW, SW, LW)
     */
    inline BandTable &getCurrentBand() { return getBandByIdx(config.data.currentBandIdx); }

    /**
     * @brief A Band indexének elkérése a bandName alapján
     *
     * @param bandName A keresett sáv neve
     * @return A BandTable rekord indexe, vagy -1, ha nem található
     */
    int8_t getBandIdxByBandName(const char *bandName);

    // /**
    //  * @brief Demodulációs mód index szerinti elkérése (FM, AM, LSB, USB, CW)
    //  * @param demodIndex A demodulációs mód indexe
    //  */
    // inline const char *getBandDemoModDescByIndex(uint8_t demodIndex) { return bandModeDesc[demodIndex]; }

    /**
     * @brief Aktuális mód/modulációs típus (FM, AM, LSB, USB, CW)
     */
    inline const char *getCurrentBandDemodModDesc() { return bandModeDesc[getCurrentBand().currDemod]; }

    /**
     * @brief A sáv FM? (A többi úgy is AM, emiatt a tagadással egyszerűbb azt kezelni, ha kell)
     */
    inline bool isCurrentBandFM() { return getCurrentBand().bandType == FM_BAND_TYPE; }

    // Demodulációs módok lekérdezése
    inline bool isCurrentDemodFM() { return getCurrentBand().currDemod == FM_DEMOD_TYPE; }
    inline bool isCurrentDemodAM() { return getCurrentBand().currDemod == AM_DEMOD_TYPE; }
    inline bool isCurrentDemodLSB() { return getCurrentBand().currDemod == LSB_DEMOD_TYPE; }
    inline bool isCurrentDemodUSB() { return getCurrentBand().currDemod == USB_DEMOD_TYPE; }
    inline bool isCurrentDemodCW() { return getCurrentBand().currDemod == CW_DEMOD_TYPE; }
    inline bool isCurrentDemodSSBorCW() { return isCurrentDemodLSB() || isCurrentDemodUSB() || isCurrentDemodCW(); }

    /**
     * A lehetséges AM demodulációs módok kigyűjtése
     */
    inline const char **getAmDemodulationModes(uint8_t &count) {
        count = ARRAY_ITEM_COUNT(bandModeDesc) - 1;
        return &bandModeDesc[1];
    }

    /**
     * @brief Az aktuális sávszélesség labeljének lekérdezése
     * @return A sávszélesség labelje, vagy nullptr, ha nem található
     */
    const char *getCurrentBandWidthLabel() {
        const char *p;

        if (isCurrentDemodAM())
            p = getCurrentBandWidthLabelByIndex(bandWidthAM, config.data.bwIdxAM);

        if (isCurrentDemodSSBorCW())
            p = getCurrentBandWidthLabelByIndex(bandWidthSSB, config.data.bwIdxSSB);

        if (isCurrentBandFM())
            p = getCurrentBandWidthLabelByIndex(bandWidthFM, config.data.bwIdxFM);

        return p;
    }

    /**
     * @brief Sávszélesség tömb labeljeinek visszaadása
     * @param bandWidth A sávszélesség tömbje
     * @param count A tömb elemeinek száma
     * @return A label-ek tömbje
     */
    template <size_t N> const char **getBandWidthLabels(const BandWidth (&bandWidth)[N], size_t &count) {
        count = N; // A tömb mérete
        static const char *labels[N];
        for (size_t i = 0; i < N; i++) {
            labels[i] = bandWidth[i].label;
        }
        return labels;
    }

    /**
     * @brief A sávszélesség labeljének lekérdezése az index alapján
     * @param bandWidth A sávszélesség tömbje
     * @param index A keresett sávszélesség indexe
     * @return A sávszélesség labelje, vagy nullptr, ha nem található
     */
    template <size_t N> const char *getCurrentBandWidthLabelByIndex(const BandWidth (&bandWidth)[N], uint8_t index) {
        for (size_t i = 0; i < N; i++) {
            if (bandWidth[i].index == index) {
                return bandWidth[i].label; // Megtaláltuk a labelt
            }
        }
        return nullptr; // Ha nem található
    }

    /**
     * @brief A sávszélesség indexének lekérdezése a label alapján
     * @param bandWidth A sávszélesség tömbje
     * @param label A keresett sávszélesség labelje
     * @return A sávszélesség indexe, vagy -1, ha nem található
     */
    template <size_t N> int8_t getBandWidthIndexByLabel(const BandWidth (&bandWidth)[N], const char *label) {
        for (size_t i = 0; i < N; i++) {
            if (strcmp(label, bandWidth[i].label) == 0) {
                return bandWidth[i].index; // Megtaláltuk az indexet
            }
        }
        return -1; // Ha nem található
    }

    /**
     * @brief Lépésköz tömb labeljeinek visszaadása
     * @param bandWidth A lépésköz tömbje
     * @param count A tömb elemeinek száma
     * @return A label-ek tömbje
     */
    template <size_t N> const char **getStepSizeLabels(const FrequencyStep (&stepSizeTable)[N], size_t &count) {
        count = N; // A tömb mérete
        static const char *labels[N];
        for (size_t i = 0; i < N; i++) {
            labels[i] = stepSizeTable[i].label;
        }
        return labels;
    }

    /**
     * @brief A lépésköz értékének lekérdezése az index alapján
     * @param bandWidth A lépésköz tömbje
     * @param index A keresett lépésköz indexe
     * @return A lépésköz labelje, vagy nullptr, ha nem található
     */
    template <size_t N> const uint16_t getStepSizeByIndex(const FrequencyStep (&stepSizeTable)[N], uint8_t index) {
        // Ellenőrizzük, hogy az index érvényes-e a tömbhöz
        if (index < N) {
            return stepSizeTable[index].value; // Közvetlenül visszaadjuk a labelt az index alapján
        }

        return 0; // Ha az index érvénytelen
    }

    /**
     * @brief A lépésköz labeljének lekérdezése az index alapján
     * @param bandWidth A lépésköz tömbje
     * @param index A keresett lépésköz indexe
     * @return A lépésköz labelje, vagy nullptr, ha nem található
     */
    template <size_t N> const char *getStepSizeLabelByIndex(const FrequencyStep (&stepSizeTable)[N], uint8_t index) {
        // Ellenőrizzük, hogy az index érvényes-e a tömbhöz
        if (index < N) {
            return stepSizeTable[index].label; // Közvetlenül visszaadjuk a labelt az index alapján
        }
        return nullptr; // Ha az index érvénytelen
    }

    /**
     * @brief Aktuális frekvencia lépésköz felirat megszerzése
     */
    const char *currentStepSizeStr() {

        // Statikus buffer a formázott string tárolására
        static char formattedStepStr[10]; // Elég nagynak kell lennie (pl. "100Hz" + '\0')

        // BFO esetén az érték az érték :')
        if (rtv::bfoOn) {
            snprintf(formattedStepStr, sizeof(formattedStepStr), "%dHz", rtv::currentBFOStep);
            return formattedStepStr; // Visszaadjuk a buffer pointerét
        }
        const char *currentStepStr = nullptr;
        BandTable &currentBand = getCurrentBand();
        uint8_t currentBandType = currentBand.bandType; // Kikeressük az aktuális Band típust

        if (currentBandType == FM_BAND_TYPE) {
            currentStepStr = getStepSizeLabelByIndex(Band::stepSizeFM, config.data.ssIdxFM);

        } else { // Nem FM

            // Ha SSB vagy CW, akkor a lépésköz a BFO-val van megoldva
            if (isCurrentDemodSSBorCW()) {
                switch (rtv::freqstepnr) {
                    default:
                    case 0:
                        currentStepStr = "1kHz";
                        break;
                    case 1:
                        currentStepStr = "100Hz";
                        break;
                    case 2:
                        currentStepStr = "10Hz";
                        break;
                }

            } else { // AM/LW/MW

                uint8_t index = (currentBandType == MW_BAND_TYPE or currentBandType == LW_BAND_TYPE) ? config.data.ssIdxMW : config.data.ssIdxAM;
                currentStepStr = getStepSizeLabelByIndex(Band::stepSizeAM, index);
            }
        }

        return currentStepStr;
    }

    // Current band utils metódusok
    inline const char *getCurrentBandName() { return getCurrentBand().bandName; }
    inline uint8_t getCurrentBandType() { return getCurrentBand().bandType; }
    inline uint16_t getCurrentBandMinimumFreq() { return getCurrentBand().minimumFreq; }
    inline uint16_t getCurrentBandMaximumFreq() { return getCurrentBand().maximumFreq; }
    inline uint16_t getCurrentBandDefaultFreq() { return getCurrentBand().defFreq; }
    inline uint8_t getCurrentBandDefaultStep() { return getCurrentBand().defStep; }
    inline bool isCurrentHamBand() { return getCurrentBand().isHam; }

    /**
     * @brief Band nevek lekérdezése - a hívó fél adja meg a tömböt
     * @param names A hívó által allokált tömb, amelybe a neveket betöltjük (legalább getFilteredBandCount() méretű kell legyen)
     * @param count A talált elemek száma (kimeneti paraméter)
     * @param isHamFilter HAM szűrő
     */
    void getBandNames(const char **names, uint8_t &count, bool isHamFilter);

    /**
     * @brief Band tábla méretének lekérdezése
     * @return A teljes band tábla mérete
     */
    static uint8_t getBandTableSize();

    /**
     * @brief Szűrt band nevek számának lekérdezése
     * @param isHamFilter HAM szűrő
     * @return A szűrt band nevek száma
     */
    static uint8_t getFilteredBandCount(bool isHamFilter);
};

#endif // __BAND_H