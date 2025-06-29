#include "DebugDataInspector.h"

#include "BandStore.h" // BandStoreData_t struktúrához
#include "Config.h"
#include "utils.h"

/**
 * @brief Kiírja az FM állomáslista tartalmát a soros portra.
 * @param fmStore Az FM állomáslista objektum.
 */
void DebugDataInspector::printFmStationData(const FmStationList_t &fmData) {
#ifdef __DEBUG
    DEBUG("=== DebugDataInspector -> FM Station Store ===\n");
    for (size_t i = 0; i < fmData.count; ++i) {
        const StationData &station = fmData.stations[i];
        DEBUG("  Station %d: Freq: %d, Name: %s, Mod: %d, BW: %d\n", i, station.frequency, station.name, station.modulation, station.bandwidthIndex);
    }
    DEBUG("====================\n");
#endif
}

/**
 * @brief Kiírja az AM állomáslista tartalmát a soros portra.
 * @param amStore Az AM állomáslista objektum.
 */
void DebugDataInspector::printAmStationData(const AmStationList_t &amData) {
#ifdef __DEBUG
    Serial.println("=== DebugDataInspector -> AM Station Store ===");
    for (size_t i = 0; i < amData.count; ++i) {
        const StationData &station = amData.stations[i];
        DEBUG("  Station %d: Freq: %d, Name: %s, Mod: %d, BW: %d\n", i, station.frequency, station.name, station.modulation, station.bandwidthIndex);
    }
    DEBUG("====================\n");
#endif
}

/**
 * @brief Kiírja a Config struktúra tartalmát a soros portra.
 * @param config A Config objektum.
 */
void DebugDataInspector::printConfigData(const Config_t &configData) {
#ifdef __DEBUG
    DEBUG("=== DebugDataInspector -> Config Data ===\n");
    DEBUG("  currentBandIdx: %u\n", configData.currentBandIdx);
    DEBUG("  bwIdxAM: %u\n", configData.bwIdxAM);
    DEBUG("  bwIdxFM: %u\n", configData.bwIdxFM);
    DEBUG("  bwIdxMW: %u\n", configData.bwIdxMW);
    DEBUG("  bwIdxSSB: %u\n", configData.bwIdxSSB);
    DEBUG("  ssIdxMW: %u\n", configData.ssIdxMW);
    DEBUG("  ssIdxAM: %u\n", configData.ssIdxAM);
    DEBUG("  ssIdxFM: %u\n", configData.ssIdxFM);
    DEBUG("  currentSquelch: %u\n", configData.currentSquelch);
    DEBUG("  squelchUsesRSSI: %s\n", configData.squelchUsesRSSI ? "true" : "false");
    DEBUG("  rdsEnabled: %s\n", configData.rdsEnabled ? "true" : "false");
    DEBUG("  currVolume: %u\n", configData.currVolume);
    DEBUG("  agcGain: %u\n", configData.agcGain);
    DEBUG("  currentAGCgain: %u\n", configData.currentAGCgain);
    DEBUG("  tftCalibrateData: [%u, %u, %u, %u, %u]\n", configData.tftCalibrateData[0], configData.tftCalibrateData[1], configData.tftCalibrateData[2],
          configData.tftCalibrateData[3], configData.tftCalibrateData[4]);
    DEBUG("  tftBackgroundBrightness: %u\n", configData.tftBackgroundBrightness);
    DEBUG("  tftDigitLigth: %s\n", configData.tftDigitLigth ? "true" : "false");
    DEBUG("  screenSaverTimeoutMinutes: %u\n", configData.screenSaverTimeoutMinutes);
    DEBUG("  beeperEnabled: %s\n", configData.beeperEnabled ? "true" : "false");
    DEBUG("  rotaryAcceleratonEnabled: %s\n", configData.rotaryAcceleratonEnabled ? "true" : "false");
    DEBUG("  miniAudioFftModeAm: %u\n", configData.miniAudioFftModeAm);
    DEBUG("  miniAudioFftModeFm: %u\n", configData.miniAudioFftModeFm);
    if (configData.miniAudioFftConfigAm == -1.0f) {
        DEBUG("  miniAudioFftConfigAm: Disabled\n");
    } else if (configData.miniAudioFftConfigAm == 0.0f) {
        DEBUG("  miniAudioFftConfigAm: Auto Gain\n");
    } else {
        DEBUG("  miniAudioFftConfigAm: Manual Gain %.1fx\n", configData.miniAudioFftConfigAm);
    }
    if (configData.miniAudioFftConfigFm == -1.0f) {
        DEBUG("  miniAudioFftConfigFm: Disabled\n");
    } else if (configData.miniAudioFftConfigFm == 0.0f) {
        DEBUG("  miniAudioFftConfigFm: Auto Gain\n");
    } else {
        DEBUG("  miniAudioFftConfigFm: Manual Gain %.1fx\n", configData.miniAudioFftConfigFm);
    }
    if (configData.miniAudioFftConfigAnalyzer == -1.0f) {
        DEBUG("  miniAudioFftConfigAnalyzer: Disabled\n");
    } else if (configData.miniAudioFftConfigAnalyzer == 0.0f) {
        DEBUG("  miniAudioFftConfigAnalyzer: Auto Gain\n");
    } else {
        DEBUG("  miniAudioFftConfigAnalyzer: Manual Gain %.1fx\n", configData.miniAudioFftConfigAnalyzer);
    }
    if (configData.miniAudioFftConfigRtty == -1.0f) {
        DEBUG("  miniAudioFftConfigRtty: Disabled\n");
    } else if (configData.miniAudioFftConfigRtty == 0.0f) {
        DEBUG("  miniAudioFftConfigRtty: Auto Gain\n");
    } else {
        DEBUG("  miniAudioFftConfigRtty: Manual Gain %.1fx\n", configData.miniAudioFftConfigRtty);
    }
    DEBUG("  cwReceiverOffsetHz: %u\n", configData.cwReceiverOffsetHz);
    DEBUG("  rttyMarkFrequencyHz: %.1f\n", configData.rttyMarkFrequencyHz);
    DEBUG("  rttyShiftHz: %.1f\n", configData.rttyShiftHz);
    DEBUG("====================\n");
#endif
}

/**
 * @brief Kiírja a Band store adatok tartalmát a soros portra.
 * @param bandData A Band store adatok.
 */
void DebugDataInspector::printBandStoreData(const BandStoreData_t &bandData) {
#ifdef __DEBUG
    DEBUG("=== DebugDataInspector -> Band Store Data ===\n");
    for (size_t i = 0; i < BANDTABLE_SIZE; ++i) {
        const BandTableData_t &band = bandData.bands[i];
        // Csak akkor írjuk ki, ha van érvényes adat (currFreq != 0)
        if (band.currFreq != 0) {
            DEBUG("  BandNdx %d: Freq: %u, Step: %u, Mod: %u, AntCap: %u\n", i, band.currFreq, band.currStep, band.currMod, band.antCap);
        }
    }
    DEBUG("====================\n");
#endif
}
