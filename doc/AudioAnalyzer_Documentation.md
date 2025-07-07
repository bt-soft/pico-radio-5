# Audio Analyzer - Dual Core Implementation

## Áttekintés

A Pico Radio projekt audio feldolgozó rendszere dual-core architektúrát használ:
- **Core0**: Fő program, UI kezelés, képernyők, touch, rotary encoder
- **Core1**: Audio feldolgozás, FFT számítás, spektrum analízis

## Komponensek

### 1. AudioAnalyzer osztály (`AudioAnalyzer.h/cpp`)

**Fő funkciók:**
- Core1-en futó audio feldolgozás 60 FPS-el
- 8kHz mintavételezés 256 pontos FFT-vel
- Dupla pufferelés thread-safe adatcseréhez
- Peak hold funkció lassú csökkenéssel

**Spektrum felbontások:**
- **Kis felbontás**: 16 oszlop (300Hz-6kHz AM, 300Hz-15kHz FM sávokhoz)
- **Nagy felbontás**: 127 oszlop (teljes FFT spektrum)

**Audio módok:**
- `OFF`: Kikapcsolt
- `SPECTRUM_LOW_RES`: Kis felbontású spektrum + peak hold
- `SPECTRUM_HIGH_RES`: Nagy felbontású spektrum
- `OSCILLOSCOPE`: Oszcilloszkóp
- `ENVELOPE`: Burkológörbe
- `WATERFALL`: Waterfall diagram
- `WATERFALL_CW_RTTY`: CW/RTTY hangolássegéd

### 2. AudioDisplayComponent osztály (`AudioDisplayComponent.h/cpp`)

**Funkciók:**
- Sprite-alapú rajzolás villódzásmentes megjelenítéshez
- Touch-al váltogatható módok
- Mód felirat megjelenítése 20 másodpercig
- "MUTED" felirat némítás esetén
- Adaptív színezés (spektrumnál: zöld→sárga→piros)

**CW/RTTY hangolássegéd:**
- Konfigurálható célfrekvenciák
- Vörös vonalak a waterfall-on
- RTTY esetén sáv jelölése

### 3. Integráció az AM/FM képernyőkbe

**ScreenAM és ScreenFM módosításai:**
- AudioDisplayComponent hozzáadása a layout-hoz
- Kis terület (160x60-70 pixel) az S-Meter mellett
- Automatikus inicializálás, ha az AudioAnalyzer fut

## Technikai részletek

### Audio mintavételezés
```cpp
// 8kHz mintavétel 125μs késleltetéssel
for (int i = 0; i < 256; i++) {
    uint16_t sample = analogRead(PIN_AUDIO_INPUT);
    workingData.vReal[i] = (double)(sample - 2048); // -2048..+2047
    sleep_us(125); // 8kHz
}
```

### Thread-safe adatcsere
```cpp
// Dupla pufferelés mutex-szal
if (mutex_try_enter(&dataMutex, nullptr)) {
    dataBuffers[activeBuffer] = workingData;
    activeBuffer = 1 - activeBuffer;
    newDataAvailable = true;
    mutex_exit(&dataMutex);
}
```

### Spektrum rajzolás
```cpp
// Gradiens színezés
if (intensity < 85) {
    color = sprite->color565(0, intensity * 3, 0); // Zöld
} else if (intensity < 170) {
    color = sprite->color565((intensity - 85) * 3, 255, 0); // Sárga
} else {
    color = sprite->color565(255, 255 - (intensity - 170) * 3, 0); // Piros
}
```

## Használat

### Inicializálás (main.cpp)
```cpp
pAudioAnalyzer = new AudioAnalyzer();
if (!pAudioAnalyzer->init()) {
    // Hiba kezelése
}
```

### Módváltás
- Érintéssel a komponens területén
- Automatikus körbejárás: OFF → SPECTRUM_LOW_RES → ... → OFF

### Debug információk
```cpp
AudioAnalyzer::Stats stats = pAudioAnalyzer->getStats();
DEBUG("Audio stats - Samples: %u, FFT: %u, Updates: %u, ProcessTime: %uus\n", 
      stats.samplesProcessed, stats.fftCalculations, 
      stats.dataUpdates, stats.processingTimeUs);
```

## Hardware kapcsolat

```cpp
// pins.h
#define PIN_AUDIO_INPUT A1 // A1/GPIO27 az audio bemenethez
#define PIN_AUDIO_MUTE 20  // Némítás kimenet
```

## Optimalizációk

1. **Core1 optimalizáció**: Külön core az audio feldolgozáshoz
2. **Sprite használat**: Villódzásmentes rajzolás
3. **Mutex lock minimalizálás**: `mutex_try_enter()` non-blocking
4. **Dupla pufferelés**: Folyamatos adatfrissítés lag nélkül
5. **Peak hold decay**: Csak időzített frissítés (50ms)

## Konfigurációs lehetőségek

### CW/RTTY beállítások (defines.h)
```cpp
#define CW_DECODER_DEFAULT_FREQUENCY 750
#define RTTY_DEFAULT_MARKER_FREQUENCY 1100.0f
#define RTTY_DEFAULT_SHIFT_FREQUENCY 425.0f
```

### Audio paraméterek (AudioAnalyzer.h)
```cpp
static constexpr uint16_t FFT_SIZE = 256;
static constexpr uint16_t SAMPLE_RATE = 8000;
static constexpr uint32_t PEAK_DECAY_INTERVAL = 50; // ms
```

## További fejlesztési lehetőségek

1. **Optimalizált ADC**: DMA-alapú mintavételezés
2. **Változó FFT méret**: Különböző felbontások módokban
3. **Audio szűrők**: Sávszűrők és zajcsökkentés
4. **Konfigurálható színpaletta**: Felhasználói testreszabás
5. **AGC integráció**: Automatikus erősítésszabályozás
6. **Demodulator integráció**: CW/RTTY dekódolás
