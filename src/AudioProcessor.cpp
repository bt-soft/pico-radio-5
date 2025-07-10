#include "AudioProcessor.h"

#include <cmath> // std::abs, std::round

#include "defines.h" // DEBUG makróhoz, ha szükséges

/**
 * @brief AudioProcessor konstruktor - inicializálja az audio feldolgozó objektumot
 * @param gainConfigRef Referencia a gain konfigurációs értékre
 * @param audioPin Az audio bemenet pin száma
 * @param targetSamplingFrequency Cél mintavételezési frekvencia Hz-ben
 * @param fftSize FFT méret (alapértelmezett: DEFAULT_FFT_SAMPLES)
 */
AudioProcessor::AudioProcessor(float &gainConfigRef, int audioPin, double targetSamplingFrequency, uint16_t fftSize)
    : FFT(), activeFftGainConfigRef(gainConfigRef), audioInputPin(audioPin), targetSamplingFrequency_(targetSamplingFrequency), binWidthHz_(0.0f),
      smoothed_auto_gain_factor_(1.0f), // Simított erősítési faktor inicializálása
      currentFftSize_(0), vReal(nullptr), vImag(nullptr), RvReal(nullptr) {

    // FFT méret érvényesítése és beállítása
    if (!validateFftSize(fftSize)) {
        DEBUG("AudioProcessor: Érvénytelen FFT méret %d, alapértelmezett %d használata\n", fftSize, AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
        fftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
    }

    // FFT tömbök allokálása
    if (!allocateFftArrays(fftSize)) {
        DEBUG("AudioProcessor: FFT tömbök allokálása sikertelen, alapértelmezett méret használata\n");
        if (!allocateFftArrays(AudioProcessorConstants::DEFAULT_FFT_SAMPLES)) {
            DEBUG("AudioProcessor: KRITIKUS: Még az alapértelmezett FFT tömbök allokálása is sikertelen!\n");
            return;
        }
    }
    if (targetSamplingFrequency_ > 0) {
        sampleIntervalMicros_ = static_cast<uint32_t>(1000000.0 / targetSamplingFrequency_);
        binWidthHz_ = static_cast<float>(targetSamplingFrequency_) / currentFftSize_;
    } else {
        sampleIntervalMicros_ = 25; // Tartalék: 40kHz
        binWidthHz_ = (1000000.0f / sampleIntervalMicros_) / currentFftSize_;
        DEBUG("AudioProcessor: Figyelmeztetés - targetSamplingFrequency nulla, tartalék használata.");
    }
    DEBUG("AudioProcessor: FFT Méret: %d, Cél Fs: %.1f Hz, Minta Intervallum: %lu us, Bin Szélesség: %.2f Hz\n", currentFftSize_, targetSamplingFrequency_, sampleIntervalMicros_, binWidthHz_);

    // Oszcilloszkóp minták inicializálása középpontra (ADC nyers érték)
    for (int i = 0; i < AudioProcessorConstants::MAX_INTERNAL_WIDTH; ++i) {
        osciSamples[i] = 2048;
    }
}

/**
 * @brief AudioProcessor destruktor - felszabadítja az allokált memóriát
 */
AudioProcessor::~AudioProcessor() { deallocateFftArrays(); }

/**
 * @brief FFT tömbök allokálása a megadott mérettel
 * @param size A kívánt FFT méret
 * @return true ha sikeres, false ha hiba történt
 */
bool AudioProcessor::allocateFftArrays(uint16_t size) {
    // Méret érvényesítése első lépésben
    if (!validateFftSize(size)) {
        return false;
    }

    // Meglévő tömbök felszabadítása, ha léteznek
    deallocateFftArrays();

    // Új tömbök allokálása
    vReal = new (std::nothrow) double[size];
    vImag = new (std::nothrow) double[size];
    RvReal = new (std::nothrow) double[size];

    // Allokálás sikerességének ellenőrzése
    if (!vReal || !vImag || !RvReal) {
        DEBUG("AudioProcessor: FFT tömbök allokálása sikertelen a %d mérethez\n", size);
        deallocateFftArrays(); // Részleges allokálás takarítása
        return false;
    }

    // Tömbök nullázása
    memset(vReal, 0, size * sizeof(double));
    memset(vImag, 0, size * sizeof(double));
    memset(RvReal, 0, size * sizeof(double));

    // FFT objektum frissítése az új tömbökkel
    FFT.setArrays(vReal, vImag, size);
    currentFftSize_ = size;

    DEBUG("AudioProcessor: FFT tömbök sikeresen allokálva a %d mérethez\n", size);
    return true;
}

/**
 * @brief FFT tömbök felszabadítása és nullázása
 */
void AudioProcessor::deallocateFftArrays() {
    delete[] vReal;
    delete[] vImag;
    delete[] RvReal;

    vReal = nullptr;
    vImag = nullptr;
    RvReal = nullptr;
    currentFftSize_ = 0;
}

/**
 * @brief FFT méret érvényesítése
 * @param size Az ellenőrizendő FFT méret
 * @return true ha érvényes, false ha nem
 */
bool AudioProcessor::validateFftSize(uint16_t size) const {
    // Méret ellenőrzése az engedélyezett tartományon belül
    if (size < AudioProcessorConstants::MIN_FFT_SAMPLES || size > AudioProcessorConstants::MAX_FFT_SAMPLES) {
        return false;
    }

    // Ellenőrizni, hogy a méret 2 hatványa-e (FFT követelmény)
    return (size > 0) && ((size & (size - 1)) == 0);
}

/**
 * @brief FFT méret beállítása futásidőben
 * @param newSize Az új FFT méret
 * @return true ha sikeres, false ha hiba történt
 */
bool AudioProcessor::setFftSize(uint16_t newSize) {

    if (!validateFftSize(newSize)) {
        DEBUG("AudioProcessor: Érvénytelen FFT méret %d\n", newSize);
        return false;
    }

    if (newSize == currentFftSize_) {
        return true; // Nincs változtatás szükséges
    }

    // Új tömbök allokálása az új mérettel
    if (!allocateFftArrays(newSize)) {
        DEBUG("AudioProcessor: FFT méret beállítása sikertelen: %d\n", newSize);
        return false;
    }

    // Bin szélesség frissítése az új FFT mérettel
    if (targetSamplingFrequency_ > 0) {
        binWidthHz_ = static_cast<float>(targetSamplingFrequency_) / currentFftSize_;
    } else {
        binWidthHz_ = (1000000.0f / sampleIntervalMicros_) / currentFftSize_;
    }

    DEBUG("AudioProcessor: FFT méret módosítva %d-re, új bin szélesség: %.2f Hz\n", currentFftSize_, binWidthHz_);

    return true;
}

/**
 * @brief Fő audio feldolgozó függvény - mintavételezés, FFT számítás és spektrum analízis
 * @param collectOsciSamples true ha oszcilloszkóp mintákat is gyűjteni kell
 */
void AudioProcessor::process(bool collectOsciSamples) {
    int osci_sample_idx = 0;
    uint32_t loopStartTimeMicros;
    double max_abs_sample_for_auto_gain = 0.0;

    // Ha az FFT ki van kapcsolva (-1.0f), akkor töröljük a puffereket és visszatérünk
    if (activeFftGainConfigRef == -1.0f) {
        memset(RvReal, 0, currentFftSize_ * sizeof(double)); // Magnitúdó buffer törlése
        if (collectOsciSamples) {
            for (int i = 0; i < AudioProcessorConstants::MAX_INTERNAL_WIDTH; ++i)
                osciSamples[i] = 2048; // Oszcilloszkóp buffer reset
        }
        return;
    }

    // 1. Mintavételezés és középre igazítás, opcionális oszcilloszkóp mintagyűjtés
    // A teljes mintavételezési ciklus idejét is mérhetnénk, de az egyes minták időzítése fontosabb.
    for (int i = 0; i < currentFftSize_; i++) {
        loopStartTimeMicros = micros();
        uint32_t sum = 0;
        for (int j = 0; j < 4; j++) { // 4 minta átlagolása a zajcsökkentés érdekében
            sum += analogRead(audioInputPin);
        }
        double averaged_sample = sum / 4.0;

        // Oszcilloszkóp minta gyűjtése ha szükséges
        if (collectOsciSamples) {
            if (i % AudioProcessorConstants::OSCI_SAMPLE_DECIMATION_FACTOR == 0 && osci_sample_idx < AudioProcessorConstants::MAX_INTERNAL_WIDTH) {
                if (osci_sample_idx < sizeof(osciSamples) / sizeof(osciSamples[0])) { // Biztonsági ellenőrzés
                    osciSamples[osci_sample_idx] = static_cast<int>(averaged_sample);
                    osci_sample_idx++;
                }
            }
        }

        vReal[i] = averaged_sample - 2048.0; // Középre igazítás (2048 a nulla szint 12 bites ADC-nél)
        vImag[i] = 0.0;

        // Auto Gain mód esetén a legnagyobb minta keresése
        if (activeFftGainConfigRef == 0.0f) { // Auto Gain mód
            if (std::abs(vReal[i]) > max_abs_sample_for_auto_gain) {
                max_abs_sample_for_auto_gain = std::abs(vReal[i]);
            }
        }

        // Időzítés a cél mintavételezési frekvencia eléréséhez
        uint32_t processingTimeMicros = micros() - loopStartTimeMicros;
        if (processingTimeMicros < sampleIntervalMicros_) {
            delayMicroseconds(sampleIntervalMicros_ - processingTimeMicros);
        }
    }

    // 2. Erősítés alkalmazása (manuális vagy automatikus)
    if (activeFftGainConfigRef > 0.0f) { // Manuális erősítés
        for (int i = 0; i < currentFftSize_; i++) {
            vReal[i] *= activeFftGainConfigRef;
        }
    } else if (activeFftGainConfigRef == 0.0f) { // Automatikus erősítés
        float target_auto_gain_factor = 1.0f;    // Alapértelmezett erősítés, ha nincs jel

        if (max_abs_sample_for_auto_gain > 0.001) { // Nullával osztás és extrém erősítés elkerülése
            target_auto_gain_factor = AudioProcessorConstants::FFT_AUTO_GAIN_TARGET_PEAK / max_abs_sample_for_auto_gain;
            target_auto_gain_factor = constrain(target_auto_gain_factor, AudioProcessorConstants::FFT_AUTO_GAIN_MIN_FACTOR, AudioProcessorConstants::FFT_AUTO_GAIN_MAX_FACTOR);
        }

        // Az erősítési faktor simítása (attack/release karakterisztika)
        if (target_auto_gain_factor < smoothed_auto_gain_factor_) {
            // A jel hangosabb lett, vagy a cél erősítés alacsonyabb -> gyorsabb "attack"
            smoothed_auto_gain_factor_ += AudioProcessorConstants::AUTO_GAIN_ATTACK_COEFF * (target_auto_gain_factor - smoothed_auto_gain_factor_);
        } else {
            // A jel halkabb lett, vagy a cél erősítés magasabb -> lassabb "release"
            smoothed_auto_gain_factor_ += AudioProcessorConstants::AUTO_GAIN_RELEASE_COEFF * (target_auto_gain_factor - smoothed_auto_gain_factor_);
        }
        // Biztosítjuk, hogy a simított faktor is a határokon belül maradjon
        smoothed_auto_gain_factor_ = constrain(smoothed_auto_gain_factor_, AudioProcessorConstants::FFT_AUTO_GAIN_MIN_FACTOR, AudioProcessorConstants::FFT_AUTO_GAIN_MAX_FACTOR);

        // Erősítés alkalmazása a mintákra
        for (int i = 0; i < currentFftSize_; i++) {
            vReal[i] *= smoothed_auto_gain_factor_;
        }
    }

    // 3. Ablakozás, FFT számítás, magnitúdó számítás
    FFT.windowing(vReal, currentFftSize_, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, currentFftSize_, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, currentFftSize_); // Az eredmény a vReal-be kerül

    // Magnitúdók átmásolása az RvReal tömbbe
    for (int i = 0; i < currentFftSize_; ++i) {
        RvReal[i] = vReal[i];
    }

    // 4. Alacsony frekvenciák csillapítása az RvReal tömbben
    // A binWidthHz_ már tagváltozóként rendelkezésre áll
    const int attenuation_cutoff_bin = static_cast<int>(AudioProcessorConstants::LOW_FREQ_ATTENUATION_THRESHOLD_HZ / binWidthHz_);

    for (int i = 0; i < (currentFftSize_ / 2); ++i) { // Csak a releváns (nem tükrözött) frekvencia bin-eken iterálunk
        if (i < attenuation_cutoff_bin) {
            RvReal[i] /= AudioProcessorConstants::LOW_FREQ_ATTENUATION_FACTOR;
        }
    }
}
