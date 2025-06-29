
#include <pgmspace.h> // For PROGMEM support, ez kell a patch_full elé
//
#include <patch_full.h> // SSB patch for whole SSBRX full download

#include "Si4735Band.h"

/**
 * Band inicializálása konfig szerint
 * @param sysStart Rendszer indítás van?
 *                 Ha true, akkor a konfigból betölti az alapértelmezett értékeket
 */
void Si4735Band::bandInit(bool sysStart) {

    DEBUG("Si4735Band::BandInit() ->currentBandIdx: %d\n", config.data.currentBandIdx); // Rendszer indítás van?
    if (sysStart) {
        DEBUG("Si4735Band::bandInit() -> System start, loading defaults...\n");

        // Band adatok betöltése a BandStore-ból
        loadBandData();

        rtv::freqstep = 1000; // hz
        rtv::freqDec = rtv::currentBFO;
    }

    // Currentband beállítása
    BandTable &currentBand = getCurrentBand();

    if (getCurrentBandType() == FM_BAND_TYPE) {
        si4735.setup(PIN_SI4735_RESET, FM_BAND_TYPE);
        si4735.setFM(); // RDS is typically automatically enabled for FM mode in Si4735

        // RDS inicializálás és konfiguráció RÖGTÖN az FM setup után
        si4735.RdsInit();
        si4735.setRdsConfig(1, 2, 2, 2, 2); // enable=1, threshold=2 (mint a working projektben)

        // Seek beállítások
        si4735.setSeekFmRssiThreshold(2); // 2dB RSSI threshold
        si4735.setSeekFmSrnThreshold(2);  // 2dB SNR threshold
        si4735.setSeekFmSpacing(10);      // 10kHz seek lépésköz
                                          // 87.5MHz - 108MHz között
        si4735.setSeekFmLimits(currentBand.minimumFreq, currentBand.maximumFreq);

    } else {
        si4735.setup(PIN_SI4735_RESET, MW_BAND_TYPE);
        si4735.setAM();

        // Seek beállítások
        si4735.setSeekAmRssiThreshold(50); // 50dB RSSI threshold
        si4735.setSeekAmSrnThreshold(20);  // 20dB SNR threshold
    }
}

/**
 * SSB patch betöltése
 */
void Si4735Band::loadSSB() {

    // Ha már be van töltve, akkor nem megyünk tovább
    if (ssbLoaded) {
        return;
    }

    si4735.reset();
    si4735.queryLibraryId(); // Is it really necessary here? I will check it.
    si4735.patchPowerUp();
    delay(50);

    si4735.setI2CFastMode(); // Recommended
    si4735.downloadPatch(ssb_patch_content, sizeof(ssb_patch_content));
    si4735.setI2CStandardMode(); // goes back to default (100KHz)
    delay(50);

    // Parameters
    // AUDIOBW - SSB Audio bandwidth; 0 = 1.2KHz (default); 1=2.2KHz; 2=3KHz; 3=4KHz; 4=500Hz; 5=1KHz;
    // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
    // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
    // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
    // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
    // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
    si4735.setSSBConfig(config.data.bwIdxSSB, 1, 0, 1, 0, 1);
    delay(25);

    ssbLoaded = true;
}

/**
 * Band beállítása
 * @param useDefaults
 */
void Si4735Band::bandSet(bool useDefaults) {

    // Beállítjuk az aktuális Band rekordot - FONTOS: pointer frissítése!
    BandTable &currentBand = getCurrentBand();
    DEBUG("Si4735Band::bandSet(%s) -> BandIdx: %d, Name: %s, CurrFreq: %d, CurrStep: %d, currDemod: %d, antCap: %d\n", //
          useDefaults ? "true" : "false",
          config.data.currentBandIdx, //
          currentBand.bandName,       //
          currentBand.currFreq,       //
          currentBand.currStep,       //
          currentBand.currDemod,      //
          currentBand.antCap          //
    );

    // Demoduláció beállítása
    uint8_t currMod = currentBand.currDemod;

    // // A sávhoz preferált demodulációs módot állítunk be?
    // if (useDefaults) {
    //     // Átmásoljuk a preferált modulációs módot
    //     currMod = currentBand.currMod = currentBand.prefMod;
    //     ssbLoaded = false; // SSB patch betöltése szükséges
    // }

    if (currMod == AM_DEMOD_TYPE or currMod == FM_DEMOD_TYPE) {
        ssbLoaded = false;

    } else if (isCurrentDemodSSBorCW()) { // LSB or USB or CW
        if (ssbLoaded == false) {
            this->loadSSB();
        }
    }

    // CW módra váltás esetén ellenőrizzük és inicializáljuk a sávszélességet
    if (currMod == CW_DEMOD_TYPE) {
        // Ha még nincs megfelelő sávszélesség beállítva CW módhoz,
        // akkor állítsuk be az optimális 1.0 kHz-et (index 5)
        // De csak akkor, ha a jelenlegi érték nem CW-optimalizált (4 vagy 5)
        if (config.data.bwIdxSSB != 4 && config.data.bwIdxSSB != 5) {
            config.data.bwIdxSSB = 5; // 1.0 kHz - optimális CW sávszélesség
        }
    }

    // Sáv beállítása
    useBand(useDefaults);

    // HF Sávszélesség beállítása
    setAfBandWidth();
}

/**
 * Band beállítása
 */
void Si4735Band::useBand(bool useDefaults) {

    DEBUG("Si4735Band::useBand(%s) Start\n", useDefaults ? "true" : "false");

    BandTable &currentBand = getCurrentBand();

    DEBUG("Si4735Band::useBand() -> BandIdx: %d, Name: %s, CurrFreq: %d, CurrStep: %d, currDemod: %d, antCap: %d\n", //
          config.data.currentBandIdx,                                                                                //
          currentBand.bandName,                                                                                      //
          currentBand.currFreq,                                                                                      //
          currentBand.currStep,                                                                                      //
          currentBand.currDemod,                                                                                     //
          currentBand.antCap                                                                                         //
    );

    // Index ellenőrzés (biztonsági okokból)
    uint8_t stepIndex;

    // AM esetén 1...1000Khz között bármi lehet - {"1kHz", "5kHz", "9kHz", "10kHz"};
    uint8_t currentBandType = currentBand.bandType;

    if (currentBandType == MW_BAND_TYPE or currentBandType == LW_BAND_TYPE) {
        stepIndex = config.data.ssIdxMW;

        // Határellenőrzés
        if (stepIndex >= ARRAY_ITEM_COUNT(stepSizeAM)) {
            DEBUG("Si4735Band Hiba: Érvénytelen ssIdxMW index: %d. Alapértelmezett használata.\n", stepIndex);
            stepIndex = 0;                   // Visszaállás alapértelmezettre (pl. 1kHz)
            config.data.ssIdxMW = stepIndex; // Opcionális: Konfig frissítése
        }

        currentBand.currStep = stepSizeAM[stepIndex].value;

    } else if (currentBandType == SW_BAND_TYPE) {

        // AM/SSB/CW SW esetén
        stepIndex = config.data.ssIdxAM;
        // Határellenőrzés
        if (stepIndex >= ARRAY_ITEM_COUNT(stepSizeAM)) {
            DEBUG("Si4735Band Hiba: Érvénytelen ssIdxAM index: %d. Alapértelmezett használata.\n", stepIndex);
            stepIndex = 0; // Visszaállás alapértelmezettre
            config.data.ssIdxAM = stepIndex;
        }
        currentBand.currStep = stepSizeAM[stepIndex].value;

    } else {
        // FM esetén csak 3 érték lehet - {"50Khz", "100KHz", "1MHz"};
        stepIndex = config.data.ssIdxFM;

        // Határellenőrzés
        if (stepIndex >= ARRAY_ITEM_COUNT(stepSizeFM)) {
            DEBUG("Si4735Band Hiba: Érvénytelen ssIdxFM index: %d. Alapértelmezett használata.\n", stepIndex);
            stepIndex = 0; // Visszaállás alapértelmezettre
            config.data.ssIdxFM = stepIndex;
        }
        currentBand.currStep = stepSizeFM[stepIndex].value;
    }

    if (currentBandType == FM_BAND_TYPE) { // FM-ben vagyunk
        ssbLoaded = false;
        rtv::bfoOn = false;

        // FM beállítások + Frekvencia beállítása
        si4735.setFM(currentBand.minimumFreq, currentBand.maximumFreq, currentBand.currFreq, currentBand.currStep);
        si4735.setFMDeEmphasis(1); // 1 = 50 μs. Usedin Europe, Australia, Japan;  2 = 75 μs. Used in USA (default)

        // RDS inicializálás és konfiguráció
        constexpr uint8_t RDS_ENABLE = 1;
        constexpr uint8_t RDS_BLOCK_ERROR_TRESHOLD = 2;
        si4735.RdsInit();
        si4735.setRdsConfig(RDS_ENABLE, RDS_BLOCK_ERROR_TRESHOLD, RDS_BLOCK_ERROR_TRESHOLD, RDS_BLOCK_ERROR_TRESHOLD, RDS_BLOCK_ERROR_TRESHOLD);

        // RDS státusz ellenőrzése a konfiguráció után
        delay(200); // Várakozás hogy a chip feldolgozza
        si4735.getRdsStatus();
    } else { // AM-ben vagyunk

        if (ssbLoaded) { // SSB vagy CW mód

            bool isCWMode = isCurrentDemodCW();

            // SSB/CW esetén a step mindig 1kHz a chipen belül
            constexpr uint8_t FREQUENCY_STEP = 1;

            // CW mód esetén mindig USB-t használunk, mivel a CW jelek az USB oldalsávban
            // jönnek át jobban (pozitív frekvencia offset)
            uint8_t modeForChip = isCWMode ? USB_DEMOD_TYPE : currentBand.currDemod;
            si4735.setSSB(currentBand.minimumFreq, currentBand.maximumFreq, currentBand.currFreq, FREQUENCY_STEP, modeForChip);

            // BFO beállítása

            // CW mód: Fix BFO offset (pl. 700 Hz) + manuális finomhangolás
            const int16_t cwBaseOffset = isCWMode ? config.data.cwReceiverOffsetHz : 0;
            // Alap CW eltolás a configból
            si4735.setSSBBfo(cwBaseOffset + rtv::currentBFO + rtv::currentBFOmanu);
            rtv::CWShift = isCWMode; // Jelezzük a kijelzőnek

            // SSB/CW esetén a lépésköz a chipen mindig 1kHz, de a finomhangolás BFO-val történik
            currentBand.currStep = FREQUENCY_STEP;
            si4735.setFrequencyStep(currentBand.currStep);

        } else { // Sima AM mód

            // AM határok és a frekvencia beállítása
            si4735.setAM(currentBand.minimumFreq, currentBand.maximumFreq, currentBand.currFreq, currentBand.currStep);
            // si4735.setAutomaticGainControl(1, 0);
            // si4735.setAmSoftMuteMaxAttenuation(0); // // Disable Soft Mute for AM
            rtv::bfoOn = false;
            rtv::CWShift = false; // AM módban biztosan nincs CW shift
        }
    }

    // Alapértelmezett értékek beállítása
    if (useDefaults) {
        // Antenna tuning capacitor beállítása
        currentBand.antCap = getDefaultAntCapValue();
    }

    // Antenna tuning capacitor beállítása
    si4735.setTuneFrequencyAntennaCapacitor(currentBand.antCap);
    delay(100);
}

/**
 * Sávszélesség beállítása
 */
void Si4735Band::setAfBandWidth() {

    DEBUG("Si4735Band::setAfBandWidth() -> currentBandIdx: %d\n", config.data.currentBandIdx);

    // BandTable &currentBand = getCurrentBand();
    //  uint8_t currDemod = currentBand.currDemod;
    if (isCurrentDemodSSBorCW()) { // SSB vagy CW mód
        /**
         * @ingroup group17 Patch and SSB support
         *
         * @brief SSB Audio Bandwidth for SSB mode
         *
         * @details 0 = 1.2 kHz low-pass filter  (default).
         * @details 1 = 2.2 kHz low-pass filter.
         * @details 2 = 3.0 kHz low-pass filter.
         * @details 3 = 4.0 kHz low-pass filter.
         * @details 4 = 500 Hz band-pass filter for receiving CW signal, i.e. [250 Hz, 750 Hz] with center
         * frequency at 500 Hz when USB is selected or [-250 Hz, -750 1Hz] with center frequency at -500Hz
         * when LSB is selected* .
         * @details 5 = 1 kHz band-pass filter for receiving CW signal, i.e. [500 Hz, 1500 Hz] with center
         * frequency at 1 kHz when USB is selected or [-500 Hz, -1500 1 Hz] with center frequency
         *     at -1kHz when LSB is selected.
         * @details Other values = reserved.
         *
         * @details If audio bandwidth selected is about 2 kHz or below, it is recommended to set SBCUTFLT[3:0] to 0
         * to enable the band pass filter for better high- cut performance on the wanted side band. Otherwise, set it to 1.
         *
         * @see AN332 REV 0.8 UNIVERSAL PROGRAMMING GUIDE; page 24
         *
         * @param AUDIOBW the valid values are 0, 1, 2, 3, 4 or 5; see description above
         */
        // CW módban is a felhasználó által beállított sávszélességet használjuk
        // Alapértelmezetten 1.0 kHz (index 5) optimális CW vételhez
        uint8_t bandwidthIndex = config.data.bwIdxSSB;
        si4735.setSSBAudioBandwidth(bandwidthIndex);

        // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
        if (bandwidthIndex == 0 or bandwidthIndex == 4 or bandwidthIndex == 5) {
            // Band pass filter to cutoff both the unwanted side band and high frequency components > 2.0 kHz of the wanted side band. (default)
            si4735.setSSBSidebandCutoffFilter(0);
        } else {
            // Low pass filter to cutoff the unwanted side band.
            si4735.setSSBSidebandCutoffFilter(1);
        }

    } else if (isCurrentDemodAM()) { // AM mód
        /**
         * @ingroup group08 Set bandwidth
         * @brief Selects the bandwidth of the channel filter for AM reception.
         * @details The choices are 6, 4, 3, 2, 2.5, 1.8, or 1 (kHz). The default bandwidth is 2 kHz. It works only in AM / SSB (LW/MW/SW)
         * @see Si47XX PROGRAMMING GUIDE; AN332 (REV 1.0); pages 125, 151, 277, 181.
         * @param AMCHFLT the choices are:   0 = 6 kHz Bandwidth
         *                                   1 = 4 kHz Bandwidth
         *                                   2 = 3 kHz Bandwidth
         *                                   3 = 2 kHz Bandwidth
         *                                   4 = 1 kHz Bandwidth
         *                                   5 = 1.8 kHz Bandwidth
         *                                   6 = 2.5 kHz Bandwidth, gradual roll off
         *                                   7–15 = Reserved (Do not use).
         * @param AMPLFLT Enables the AM Power Line Noise Rejection Filter.
         */
        si4735.setBandwidth(config.data.bwIdxAM, 0);

    } else if (isCurrentDemodFM()) { // FM mód
        /**
         * @brief Sets the Bandwith on FM mode
         * @details Selects bandwidth of channel filter applied at the demodulation stage. Default is automatic which means the device automatically selects proper
         * channel filter. <BR>
         * @details | Filter  | Description |
         * @details | ------- | -------------|
         * @details |    0    | Automatically select proper channel filter (Default) |
         * @details |    1    | Force wide (110 kHz) channel filter |
         * @details |    2    | Force narrow (84 kHz) channel filter |
         * @details |    3    | Force narrower (60 kHz) channel filter |
         * @details |    4    | Force narrowest (40 kHz) channel filter |
         *
         * @param filter_value
         */
        si4735.setFmBandwidth(config.data.bwIdxFM);
    }
}

/**
 * @brief Hangolás a memória állomásra
 * @param bandIndex A band indexe, amelyre hangolunk
 * @param frequency A hangolási frekvencia (Hz)
 * @param demodModIndex A demodulációs mód indexe (FM, AM, LSB, USB, CW)
 * @param bandwidthIndex A sávszélesség indexe
 */
void Si4735Band::tuneMemoryStation(uint8_t bandIndex, uint16_t frequency, uint8_t demodModIndex, uint8_t bandwidthIndex) {

    // 1. Elkérjük a Band táblát
    BandTable &currentBand = getCurrentBand();

    // Band index beállítása a konfigban
    config.data.currentBandIdx = bandIndex;

    // 2. Demodulátor beállítása a chipen.  Ha CW módra
    uint8_t savedMod = demodModIndex; // A demodulációs mód kiemelése

    if (savedMod != CW_DEMOD_TYPE and rtv::CWShift == true) {
        // TODO: ezt még kidolgozni
        rtv::CWShift = false;
    }

    // Átállítjuk a demodulációs módot
    currentBand.currDemod = demodModIndex;

    // 3. Sávszélesség index beállítása a configban a MENTETT érték alapján ---
    uint8_t savedBwIndex = bandwidthIndex;
    if (savedMod == FM_DEMOD_TYPE) {
        config.data.bwIdxFM = savedBwIndex;
    } else if (savedMod == AM_DEMOD_TYPE) {
        config.data.bwIdxAM = savedBwIndex;
    } else { // LSB, USB, CW
        config.data.bwIdxSSB = savedBwIndex;
    }

    // 4. Újra beállítjuk a sávot az új móddal (false -> ne a preferált adatokat töltse be)
    this->bandSet(false); // 5. Explicit módon állítsd be a frekvenciát és a módot a chipen
    si4735.setFrequency(frequency);

    // A tényleges frekvenciát olvassuk vissza a chip-ből (lehet, hogy nem pontosan azt állította be, amit kértünk)
    currentBand.currFreq = si4735.getCurrentFrequency();

    // BFO eltolás visszaállítása SSB/CW esetén ---
    if (demodModIndex == LSB_DEMOD_TYPE || demodModIndex == USB_DEMOD_TYPE || demodModIndex == CW_DEMOD_TYPE) {
        const int16_t cwBaseOffset = (demodModIndex == CW_DEMOD_TYPE) ? config.data.cwReceiverOffsetHz : 0;

        si4735.setSSBBfo(cwBaseOffset);
        rtv::CWShift = (demodModIndex == CW_DEMOD_TYPE); // CW shift állapot frissítése

    } else {
        // AM/FM esetén biztosítjuk, hogy a BFO nullázva legyen
        rtv::lastBFO = 0;
        rtv::currentBFO = 0;
        rtv::freqDec = 0;
        rtv::CWShift = false;
    }

    // 6. Hangerő visszaállítása
    si4735.setVolume(config.data.currVolume);
}

/**
 * @brief A frekvencia léptetése a rotary encoder értéke alapján
 * @param rotaryValue A rotary encoder értéke (növelés/csökkentés)
 * @return A léptetett frekvencia
 */
uint16_t Si4735Band::stepFrequency(int16_t rotaryValue) {

    BandTable &currentBand = getCurrentBand();

    // Kiszámítjuk a frekvencia lépés nagyságát
    int16_t step = rotaryValue * currentBand.currStep; // A lépés nagysága
    uint16_t targetFreq = currentBand.currFreq + step;

    // Korlátozás a sáv határaira
    if (targetFreq < currentBand.minimumFreq) {
        targetFreq = currentBand.minimumFreq;
    } else if (targetFreq > currentBand.maximumFreq) {
        targetFreq = currentBand.maximumFreq;
    }

    // Csak akkor változtatunk, ha tényleg más a cél frekvencia
    if (targetFreq != currentBand.currFreq) {
        // Beállítjuk a frekvenciát
        si4735.setFrequency(targetFreq);

        // El is mentjük a band táblába
        currentBand.currFreq = si4735.getCurrentFrequency();

        // Band adatok mentését megjelöljük
        saveBandData();

        // Ez biztosítja, hogy az S-meter azonnal frissüljön az új frekvencián
        invalidateSignalCache();
    }

    return currentBand.currFreq;
}