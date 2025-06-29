#ifndef __SCREEN_FREQU_DISPLAY_BASE_H
#define __SCREEN_FREQU_DISPLAY_BASE_H

#include "FreqDisplay.h"
#include "UIScreen.h"

class ScreenFrequDisplayBase : public UIScreen {
  protected:
    // Frekvencia kijelző komponens
    std::shared_ptr<FreqDisplay> freqDisplayComp;

    /**
     * @brief Létrehozza a frekvencia kijelző komponenst
     * @param freqBounds A frekvencia kijelző határai (Rect)
     */
    inline void createFreqDisplay(Rect freqBounds) {
        freqDisplayComp = std::make_shared<FreqDisplay>(freqBounds);
        addChild(freqDisplayComp);
    }

  public:
    ScreenFrequDisplayBase(const char *name) : UIScreen(name) {}

    /**
     * @brief Frekvencia kijelző komponens lekérése
     */
    inline std::shared_ptr<FreqDisplay> getFreqDisplayComp() const { return freqDisplayComp; }
};

#endif // __SCREEN_FREQU_DISPLAY_BASE_H