#include <cmath> // std::abs, std::round

#include "AudioProcessor.h"
#include "defines.h"
#include "utils.h"

constexpr uint8_t NOISE_REDUCTION_ANALOG_SAMPLES_COUNT = 2; // Minta átlagolás zajcsökkentéshez

/**
 * @brief AudioProcessor konstruktor - inicializálja az audio feldolgozó objektumot
 * @param gainConfigRef Referencia a gain konfigurációs értékre
 * @param audioPin Az audio bemenet pin száma
 * @param targetSamplingFrequency Cél mintavételezési frekvencia Hz-ben
 * @param fftSize FFT méret (alapértelmezett: DEFAULT_FFT_SAMPLES)
 */
AudioProcessor::AudioProcessor(float &gainConfigRef, uint8_t audioPin, uint16_t targetSamplingFrequency, uint16_t fftSize)
    : FFT(),                                             //
      activeFftGainConfigRef(gainConfigRef),             //
      audioInputPin(audioPin),                           //
      targetSamplingFrequency_(targetSamplingFrequency), //
      binWidthHz_(0.0f),                                 //
      smoothed_auto_gain_factor_(1.0f),                  // Simított erősítési faktor inicializálása
      currentFftSize_(0),                                //
      vReal(nullptr),                                    //
      vImag(nullptr),                                    //
      RvReal(nullptr) {

    // FFT méret érvényesítése és beállítása
    if (!validateFftSize(fftSize)) {
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

    calculateBinWidthHz(); // Bin szélesség számítása

    // Arduino-kompatibilis float-to-string konverzió
    DEBUG("AudioProcessor: FFT Méret: %d, Cél Fs: %s Hz, Minta Intervallum: %lu us, Bin Szélesség: %s Hz\n", currentFftSize_, Utils::floatToString(targetSamplingFrequency_).c_str(), sampleIntervalMicros_,
          Utils::floatToString(binWidthHz_).c_str());

    // Oszcilloszkóp minták inicializálása középpontra (ADC nyers érték)
    std::fill(osciSamples, osciSamples + AudioProcessorConstants::OSCI_SAMPLE_MAX_INTERNAL_WIDTH, 2048);
}

/**
 * @brief AudioProcessor destruktor - felszabadítja az allokált memóriát
 */
AudioProcessor::~AudioProcessor() { deallocateFftArrays(); }

/**
 * @brief Bin szélesség Hz-ben
 * @return A kiszámított bin szélesség
 */
void AudioProcessor::calculateBinWidthHz() {

    // Mintavételezési frekvencia és bin szélesség beállítása
    if (targetSamplingFrequency_ > 0) {
        sampleIntervalMicros_ = static_cast<uint32_t>(ONE_SECOND_IN_MICROS / targetSamplingFrequency_);
        binWidthHz_ = static_cast<float>(targetSamplingFrequency_) / static_cast<float>(currentFftSize_);
    } else {
        DEBUG("AudioProcessor: Figyelmeztetés - targetSamplingFrequency nulla, tartalék használata.");
        sampleIntervalMicros_ = 25; // Tartalék: 40kHz
        binWidthHz_ = static_cast<float>((ONE_SECOND_IN_MICROS / static_cast<float>(currentFftSize_)) / static_cast<float>(sampleIntervalMicros_) / static_cast<float>(currentFftSize_));
    }
}

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
 * @brief Mintavételezési frekvencia beállítása futási időben
 * @param newFs Az új mintavételezési frekvencia Hz-ben
 * @return true ha sikeres, false ha hiba történt
 */
bool AudioProcessor::setSamplingFrequency(uint16_t newFs) {

    if (newFs < AudioProcessorConstants::MIN_SAMPLING_FREQUENCY || newFs > AudioProcessorConstants::MAX_SAMPLING_FREQUENCY) {
        DEBUG("AudioProcessor: Érvénytelen mintavételezési frekvencia: %d Hz, tartomány: [%d, %d] Hz\n", newFs, AudioProcessorConstants::MIN_SAMPLING_FREQUENCY, AudioProcessorConstants::MAX_SAMPLING_FREQUENCY);
        return false; // Érvénytelen frekvencia, nem állítjuk be
    }

    if (newFs == targetSamplingFrequency_) {
        DEBUG("AudioProcessor: Nincs változtatásra szükség, a mintavételezési frekvencia már %d Hz\n", newFs);
        return false; // Nincs változtatás -> sikeres a méret beállítása
    }

    targetSamplingFrequency_ = newFs;
    calculateBinWidthHz(); // Frissítjük a bin szélességet az új mintavételezési frekvenciával

    DEBUG("AudioProcessor: Mintavételezési frekvencia beállítva %d Hz-re\n", (int)targetSamplingFrequency_);

    return true;
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

    if (newSize == currentFftSize_) {
        DEBUG("AudioProcessor: Nincs változtatásra szükség, az FFT méret már %d\n", newSize);
        return true; // Nincs változtatás -> sikeres a méret beállítása
    }

    DEBUG("AudioProcessor: FFT méret beállítása %d-re\n", newSize);

    // Valid az érték?
    if (!validateFftSize(newSize)) {
        DEBUG("AudioProcessor: Érvénytelen FFT méret %d\n", newSize);
        return false;
    }

    // Új tömbök allokálása az új mérettel
    if (!allocateFftArrays(newSize)) {
        DEBUG("AudioProcessor: FFT méret beállítása sikertelen: %d\n", newSize);
        return false;
    }

    currentFftSize_ = newSize;

    // Bin szélesség frissítése az új FFT mérettel
    calculateBinWidthHz();

    // Arduino-kompatibilis float-to-string konverzió
    DEBUG("AudioProcessor: FFT méret módosítva %d-re, új bin szélesség: %s Hz\n", currentFftSize_, Utils::floatToString(binWidthHz_).c_str());

    return true;
}

/**
 * @brief Fő audio feldolgozó függvény - mintavételezés, FFT számítás és spektrum analízis
 * @param collectOsciSamples true ha oszcilloszkóp mintákat is gyűjteni kell
 */
void AudioProcessor::process(bool collectOsciSamples) {

    int osci_sample_idx = 0;
    double max_abs_sample_for_auto_gain = 0.0;

    // Ha az FFT ki van kapcsolva (-1.0f), akkor töröljük a puffereket és visszatérünk
    if (activeFftGainConfigRef == -1.0f) {
        memset(RvReal, 0, currentFftSize_ * sizeof(double)); // Magnitúdó buffer törlése
        if (collectOsciSamples) {
            // Oszcilloszkóp minták inicializálása középpontra (ADC nyers érték)
            std::fill(osciSamples, osciSamples + AudioProcessorConstants::OSCI_SAMPLE_MAX_INTERNAL_WIDTH, 2048);
        }
        return;
    }

    // 1. Mintavételezés és középre igazítás, opcionális oszcilloszkóp mintagyűjtés
    // A teljes mintavételezési ciklus idejét is mérhetnénk, de az egyes minták időzítése fontosabb.
    uint32_t nextSampleTime = micros();
    for (uint16_t i = 0; i < currentFftSize_; i++) {
        // Pontos időzítés a mintavételezési frekvencia betartásához
        while (micros() < nextSampleTime) {
            // busy wait a CPU magon, ez itt elfogadható, mert a Core1 dedikált
        }
        nextSampleTime += sampleIntervalMicros_;

        uint32_t sum = 0;
        for (uint8_t j = 0; j < NOISE_REDUCTION_ANALOG_SAMPLES_COUNT; j++) {
            sum += analogRead(audioInputPin);
        }
        double averaged_sample = sum / (double)NOISE_REDUCTION_ANALOG_SAMPLES_COUNT;

        // Oszcilloszkóp minta gyűjtése ha szükséges (decimation factor 2 hatványa: bitmaszk)
        if (collectOsciSamples) {
            if (((AudioProcessorConstants::OSCI_SAMPLE_DECIMATION_FACTOR & (AudioProcessorConstants::OSCI_SAMPLE_DECIMATION_FACTOR - 1)) == 0) ? ((i & (AudioProcessorConstants::OSCI_SAMPLE_DECIMATION_FACTOR - 1)) == 0)
                                                                                                                                               : (i % AudioProcessorConstants::OSCI_SAMPLE_DECIMATION_FACTOR == 0)) {
                if (osci_sample_idx < AudioProcessorConstants::OSCI_SAMPLE_MAX_INTERNAL_WIDTH) {
                    osciSamples[osci_sample_idx++] = static_cast<int>(averaged_sample);
                }
            }
        }

        vReal[i] = averaged_sample - 2048.0;
        vImag[i] = 0.0; // vImag nullázása minden iterációban

        if (activeFftGainConfigRef == 0.0f) {
            double abs_val = std::abs(vReal[i]);
            if (abs_val > max_abs_sample_for_auto_gain) {
                max_abs_sample_for_auto_gain = abs_val;
            }
        }
    }
    // Oszcilloszkóp mintaszám csak a ciklus végén
    osciSampleCount = osci_sample_idx;

    // 2. Erősítés alkalmazása (manuális vagy automatikus)
    if (activeFftGainConfigRef > 0.0f) { // Manuális erősítés
        for (uint16_t i = 0; i < currentFftSize_; i++) {
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
        for (uint16_t i = 0; i < currentFftSize_; i++) {
            vReal[i] *= smoothed_auto_gain_factor_;
        }
    }

    // 3. Ablakozás, FFT számítás, magnitúdó számítás
    FFT.windowing(vReal, currentFftSize_, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, currentFftSize_, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, currentFftSize_); // Az eredmény a vReal-be kerül

    // Magnitúdók átmásolása az RvReal tömbbe memcpy-val
    memcpy(RvReal, vReal, currentFftSize_ * sizeof(double));

    // 4. Alacsony frekvenciák csillapítása az RvReal tömbben
    // A binWidthHz_ már tagváltozóként rendelkezésre áll
    const uint16_t attenuation_cutoff_bin = static_cast<int>(AudioProcessorConstants::LOW_FREQ_ATTENUATION_THRESHOLD_HZ / binWidthHz_);

    // Csak az attenuation_cutoff_bin-ig futtatjuk a csillapítást
    for (uint16_t i = 0; i < attenuation_cutoff_bin && i < (currentFftSize_ / 2); ++i) {
        RvReal[i] /= AudioProcessorConstants::LOW_FREQ_ATTENUATION_FACTOR;
    }
}
