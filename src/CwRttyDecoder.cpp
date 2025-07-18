#include "CwRttyDecoder.h"

CwRttyDecoder::CwRttyDecoder() { clear(); }

void CwRttyDecoder::clear() {
    decodedText = "";
    // ... (belső állapotok nullázása)
}

String CwRttyDecoder::getDecodedText() { return decodedText; }

/**
 * @brief Feldolgozza az FFT adatokat és dekódolja a CW jelet.
 * Ez a metódus lesz a dekóder "szíve".
 */
void CwRttyDecoder::processFftData(const float *fftData, uint16_t fftSize, float binWidth) {
    // 1. Peak detektálás: Keresd meg a legerősebb jelet a hangolási sávban (pl. 600-900 Hz).
    // 2. Állapotgép:
    //    - Ha a jel erőssége egy küszöb felett van -> 'ON' állapot.
    //    - Ha alatta -> 'OFF' állapot.
    // 3. Időmérés:
    //    - Mérd az 'ON' állapotok hosszát (dit vagy dah).
    //    - Mérd az 'OFF' állapotok hosszát (karakter- vagy szóköz).
    // 4. Morse kód konverzió:
    //    - A mért időtartamok alapján állítsd össze a Morse karaktereket.
    //    - Fűzd hozzá a dekódolt karaktert a `decodedText`-hez.
    //
    // Példa (nagyon leegyszerűsítve):
    // decodedText += "E";
}
