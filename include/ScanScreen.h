/**
 * @file ScanScreen.h
 * @brief Spektrum analizátor scan képernyő
 * @details Grafikus frekvencia pásztázás RSSI/SNR megjelenítéssel
 */

#ifndef __SCANSCREEN_H
#define __SCANSCREEN_H

#include "Config.h"
#include "Si4735Manager.h"
#include "UIButton.h"
#include "UIHorizontalButtonBar.h"
#include "UIScreen.h"
#include <memory>
#include <vector>

/**
 * @brief Scan képernyő állapotok
 */
enum class ScanState {
    Idle,    ///< Nincs aktív scan
    Scanning ///< Aktívan pásztáz
};

/**
 * @brief Scan módok
 */
enum class ScanMode {
    Spectrum, ///< Spektrum analizátor mód
    Seek,     ///< Gyors seek-based pásztázás
    Memory    ///< Mentett állomások pásztázása
};

/**
 * @brief Spektrum analizátor scan képernyő
 * @details Grafikus frekvencia pásztázás valós idejű RSSI/SNR megjelenítéssel
 */
class ScanScreen : public UIScreen {
  public:
    /**
     * @brief ScanScreen konstruktor
     * @param tft TFT display referencia
     * @param si4735Manager Si4735Manager referencia
     */
    ScanScreen();
    /**
     * @brief Destruktor
     */
    virtual ~ScanScreen() = default;

    // UIScreen interface implementáció
    void activate() override;
    void deactivate() override;
    void drawContent() override;
    void handleOwnLoop() override;
    bool handleTouch(const TouchEvent &event) override;
    bool handleRotary(const RotaryEvent &event) override;

  private:
    // Button IDs
    static constexpr uint8_t BACK_BUTTON_ID = 40;
    static constexpr uint8_t PLAY_PAUSE_BUTTON_ID = 41;
    static constexpr uint8_t ZOOM_IN_BUTTON_ID = 42;
    static constexpr uint8_t ZOOM_OUT_BUTTON_ID = 43;
    static constexpr uint8_t RESET_BUTTON_ID = 44;    // Screen layout constants (480x320 display)
    static constexpr uint16_t SCAN_AREA_WIDTH = 460;  // Spektrum szélessége (pixelben)
    static constexpr uint16_t SCAN_RESOLUTION = 920;  // Mintavételi pontok száma (2x felbontás)
    static constexpr uint16_t SCAN_AREA_HEIGHT = 180; // Spektrum magassága
    static constexpr uint16_t SCAN_AREA_X = 10;       // Spektrum X pozíciója
    static constexpr uint16_t SCAN_AREA_Y = 40;       // Spektrum Y pozíciója
    static constexpr uint16_t SCALE_HEIGHT = 20;      // Skála magassága
    static constexpr uint16_t INFO_AREA_Y = 250;      // Info terület Y pozíciója (frekvencia címkék után)

    // UI komponensek
    std::shared_ptr<UIButton> backButton;
    std::shared_ptr<UIButton> playPauseButton;
    std::shared_ptr<UIButton> zoomInButton;
    std::shared_ptr<UIButton> zoomOutButton;
    std::shared_ptr<UIButton> resetButton;

    // Scan állapot változók
    ScanState scanState;
    ScanMode scanMode;
    bool scanPaused;
    uint32_t lastScanTime;    // Frekvencia és zoom kezelés
    uint32_t currentScanFreq; // Aktuális scan frekvencia (kHz-ben)
    uint32_t scanStartFreq;   // Scan tartomány kezdete
    uint32_t scanEndFreq;     // Scan tartomány vége
    float scanStep;           // Scan lépésköz (kHz)
    float zoomLevel;          // Zoom szint (1.0 = teljes sáv)
    uint16_t currentScanPos;  // Aktuális pozíció a spektrumban
    uint8_t zoomGeneration;   // Zoom generációk száma (interpoláció limitáláshoz)

    // RSSI/SNR adatok (nagyobb felbontással)
    int16_t scanValueRSSI[SCAN_RESOLUTION]; // RSSI értékek
    uint8_t scanValueSNR[SCAN_RESOLUTION];  // SNR értékek
    bool scanMark[SCAN_RESOLUTION];         // Állomás jelzők
    uint8_t scanScaleLine[SCAN_RESOLUTION]; // Skála vonalak
    bool scanDataValid[SCAN_RESOLUTION];    // Érvényes adatok jelzői

    // Sáv határok
    int16_t scanBeginBand; // Sáv kezdete a spektrumban
    int16_t scanEndBand;   // Sáv vége a spektrumban
    uint8_t scanMarkSNR;   // SNR küszöb az állomás jelzéshez
    bool scanEmpty;        // Üres scan (inicializálás)

    // Konfiguráció
    uint8_t countScanSignal; // Jel mérések száma átlagoláshoz
    float signalScale;       // Jel skálázási tényező

    // UI állapot cache (villogás elkerülésére)
    String lastStatusText; // Előző státusz szöveg cache    // Metódusok
    void layoutComponents();
    void createHorizontalButtonBar();
    void initializeScan();
    void resetScan();
    void startScan();
    void pauseScan();
    void stopScan();
    void updateScan();
    void drawSpectrum();
    void drawSpectrumLine(uint16_t x);
    void drawScale();
    void drawFrequencyLabels();
    void drawBandBoundaries();
    void drawScanInfoStatic();
    void drawScanInfo();
    void getSignalQuality(int16_t &rssiY, uint8_t &snr);
    void setFrequency(uint32_t freq);
    void calculateScanParameters();
    void zoomIn();
    void zoomOut();
    uint32_t positionToFreq(uint16_t x);
    uint16_t freqToPosition(uint32_t freq);
    void handleZoom(float newZoomLevel);
    bool isDataValid(uint16_t scanPos) const;
};

#endif // __SCANSCREEN_H
