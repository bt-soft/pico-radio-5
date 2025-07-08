// PÉLDA: Hogyan fogják a képernyők használni az új AudioProcessor API-t

#include "AudioProcessor.h"

// ScreenFM példa implementáció
void ScreenFM::onActivate() {
    // FM képernyő aktiválásakor beállítjuk az FM sávszűrőt
    AudioProcessorCore1::setBandFilterFrequencies(300.0f, 15000.0f); // FM: 300 Hz - 15 kHz

    // Spektrum vizualizáció bekapcsolása
    AudioProcessorCore1::setVisualizationMode(AudioVisualizationType::SPECTRUM_LOW_RES);
    AudioProcessorCore1::setAudioEnabled(true);
}

// ScreenAM példa implementáció
void ScreenAM::onActivate() {
    // AM képernyő aktiválásakor beállítjuk az AM sávszűrőt
    AudioProcessorCore1::setBandFilterFrequencies(300.0f, 6000.0f); // AM: 300 Hz - 6 kHz

    // Spektrum vizualizáció bekapcsolása
    AudioProcessorCore1::setVisualizationMode(AudioVisualizationType::SPECTRUM_LOW_RES);
    AudioProcessorCore1::setAudioEnabled(true);
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

void ScreenRTTY::onActivate() {
    // RTTY módhoz: 1000-3000 Hz
    AudioProcessorCore1::setBandFilterFrequencies(1000.0f, 3000.0f);
    AudioProcessorCore1::setVisualizationMode(AudioVisualizationType::RTTY_WATERFALL);
    AudioProcessorCore1::setAudioEnabled(true);
}

// ELŐNYÖK:
// 1. Nincs függőség a Si4735Manager-re
// 2. Minden képernyő saját maga tudja, milyen sávot akar
// 3. Rugalmas - akár dinamikusan is változtatható
// 4. Tiszta API - könnyen használható
// 5. Thread-safe mutex védelemmel
