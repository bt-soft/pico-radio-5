#ifndef __CW_RTTY_DECODER_H
#define __CW_RTTY_DECODER_H

#include <Arduino.h>

class CwRttyDecoder {
  private:
    // Belső állapotváltozók a CW dekódoláshoz
    String decodedText;
    // ... (állapotgép változói, időzítők a dit/dah/szünetekhez, stb.)

  public:
    CwRttyDecoder();
    void processFftData(const float *fftData, uint16_t fftSize, float binWidth);
    String getDecodedText();
    void clear();
};

#endif // __CW_RTTY_DECODER_H