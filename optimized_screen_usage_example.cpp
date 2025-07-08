// PÉLDA: Hogyan fogják a képernyők használni az új optimalizált AudioProcessor API-t

#include "AudioProcessor.h"

// ScreenFM példa implementáció
void ScreenFM::onActivate() {
    // FM képernyő aktiválásakor beállítjuk az FM sávszűrőt
    AudioProcessorCore1::setBandFilterFrequencies(300.0f, 15000.0f); // FM: 300 Hz - 15 kHz

    // Spektrum vizualizáció bekapcsolása
    AudioProcessorCore1::setVisualizationMode(AudioVisualizationType::SPECTRUM_LOW_RES);
    AudioProcessorCore1::setAudioEnabled(true);
}

void ScreenFM::renderSpectrum() {
    // Spektrum adatok olvasása a union-ból
    mutex_enter_blocking(&g_sharedAudioData.dataMutex);

    if (g_sharedAudioData.dataReady && g_sharedAudioData.mode == AudioVisualizationType::SPECTRUM_LOW_RES) {
        // Union spectrum adatok használata
        for (int i = 0; i < AudioProcessorConstants::LOW_RES_BINS; i++) {
            float magnitude = g_sharedAudioData.data.spectrum.lowResBins[i];
            float peak = g_sharedAudioData.data.spectrum.lowResPeaks[i];
            // Spektrum vizualizáció rajzolása...
        }
        g_sharedAudioData.dataReady = false;
    }

    mutex_exit(&g_sharedAudioData.dataMutex);
}

// ScreenAM példa implementáció
void ScreenAM::onActivate() {
    // AM képernyő aktiválásakor beállítjuk az AM sávszűrőt
    AudioProcessorCore1::setBandFilterFrequencies(300.0f, 6000.0f); // AM: 300 Hz - 6 kHz

    // Spektrum vizualizáció bekapcsolása
    AudioProcessorCore1::setVisualizationMode(AudioVisualizationType::SPECTRUM_LOW_RES);
    AudioProcessorCore1::setAudioEnabled(true);
}

// Oszcilloszkóp képernyő példa
void ScreenOscilloscope::onActivate() {
    // Oszcilloszkóp mód bekapcsolása
    AudioProcessorCore1::setVisualizationMode(AudioVisualizationType::OSCILLOSCOPE);
    AudioProcessorCore1::setAudioEnabled(true);
}

void ScreenOscilloscope::renderWaveform() {
    mutex_enter_blocking(&g_sharedAudioData.dataMutex);

    if (g_sharedAudioData.dataReady && g_sharedAudioData.mode == AudioVisualizationType::OSCILLOSCOPE) {
        // Union oscilloscope adatok használata
        for (int i = 0; i < AudioProcessorConstants::OSCILLOSCOPE_SAMPLES; i++) {
            int16_t sample = g_sharedAudioData.data.oscilloscope.samples[i];
            // Hullámforma rajzolása...
        }

        float rms = g_sharedAudioData.data.oscilloscope.rms;
        float peak = g_sharedAudioData.data.oscilloscope.peak;
        // RMS és peak megjelenítése...

        g_sharedAudioData.dataReady = false;
    }

    mutex_exit(&g_sharedAudioData.dataMutex);
}

// Setup képernyő - audio kikapcsolás
void ScreenSetup::onActivate() {
    // Setup módban kikapcsoljuk az audio feldolgozást (takarékosság)
    AudioProcessorCore1::setAudioEnabled(false);
}

// CW/RTTY képernyők - speciális sávok
void ScreenCW::onActivate() {
    // CW módhoz keskeny sáv: 500-800 Hz (CW szűrő)
    AudioProcessorCore1::setBandFilterFrequencies(500.0f, 800.0f);
    AudioProcessorCore1::setVisualizationMode(AudioVisualizationType::CW_WATERFALL);
    AudioProcessorCore1::setAudioEnabled(true);
}

void ScreenCW::renderWaterfall() {
    mutex_enter_blocking(&g_sharedAudioData.dataMutex);

    if (g_sharedAudioData.dataReady && g_sharedAudioData.mode == AudioVisualizationType::CW_WATERFALL) {
        // Union waterfall adatok használata
        uint16_t currentRow = g_sharedAudioData.data.waterfall.currentRow;

        for (int freq = 0; freq < AudioProcessorConstants::SPECTRUM_BINS; freq++) {
            for (int row = 0; row < AudioProcessorConstants::WATERFALL_HEIGHT; row++) {
                uint8_t intensity = g_sharedAudioData.data.waterfall.waterfallBuffer[row][freq];
                // Waterfall pixel rajzolása...
            }
        }

        g_sharedAudioData.dataReady = false;
    }

    mutex_exit(&g_sharedAudioData.dataMutex);
}

// UNION ELŐNYÖK:
// 1. **5,232 bytes RAM megtakarítás** (8.9% csökkenés!)
// 2. **Logikus**: Csak egy vizualizációs mód aktív egyszerre
// 3. **Cache-friendly**: Kisebb memória footprint
// 4. **Tiszta API**: Ugyanúgy használható, mint előtte
// 5. **Thread-safe**: Mutex védelemmel
// 6. **Egyszerű váltás**: Csak a mode beállítás változik

// FONTOS MEGJEGYZÉSEK:
// - Mód váltáskor az union tartalma felülíródik (ez OK, mert új módba váltunk)
// - Minden képernyő saját vizualizációs módot állít be
// - A union automatikusan kezel minden memória allokációt
