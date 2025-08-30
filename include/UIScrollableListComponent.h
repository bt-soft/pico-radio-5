#pragma once

#include "IScrollableListDataSource.h"
#include "UIComponent.h"
#include <vector>

/**
 * @brief Újrafelhasználható görgethető lista UI komponens.
 *
 * Ez a komponens egy listát jelenít meg, amely görgethető,
 * és az elemeket egy IScrollableListDataSource interfészen keresztül kapja.
 */
class UIScrollableListComponent : public UIComponent {
  public:
    static constexpr uint8_t DEFAULT_VISIBLE_ITEMS = 5;
    static constexpr uint8_t DEFAULT_ITEM_HEIGHT = 20; // Vagy számoljuk a font magasságból
    static constexpr uint8_t SCROLL_BAR_WIDTH = 8;
    static constexpr uint8_t ITEM_TEXT_PADDING_X = 5;
    static constexpr uint8_t SELECTED_ITEM_PADDING = 2;                                // Hány pixellel legyen kisebb a kijelölés minden oldalon
    static constexpr uint8_t COMPONENT_BORDER_THICKNESS = 1;                           // A komponens saját keretének vastagsága
    static constexpr uint8_t SELECTED_ITEM_RECT_REDUCTION = SELECTED_ITEM_PADDING * 2; // Teljes csökkentés a szélességben/magasságban

  private:
    IScrollableListDataSource *dataSource = nullptr;
    int topItemIndex = 0;      // A lista tetején látható elem indexe
    int selectedItemIndex = 0; // A kiválasztott elem indexe (abszolút)
    uint8_t visibleItemCount = DEFAULT_VISIBLE_ITEMS;
    uint8_t itemHeight = DEFAULT_ITEM_HEIGHT;

    // Színek
    uint16_t itemTextColor;
    uint16_t selectedItemTextColor;
    uint16_t selectedItemBackground;
    uint16_t scrollBarColor;
    uint16_t scrollBarBackgroundColor;

    /**
     * @brief A lista elemeinek újrarajzolása a jelenlegi állapot alapján.
     */
    void drawScrollBar() {
        // Ha nincs dataSource, vagy nincs elég elem a görgetéshez, ne rajzolj scrollbart.
        if (!dataSource || dataSource->getItemCount() <= visibleItemCount) {
            return; // Nincs szükség scrollbarra
        }

        // A görgetősáv a komponens belső keretén belül helyezkedik el
        int16_t scrollBarContainerX = bounds.x + bounds.width - COMPONENT_BORDER_THICKNESS - SCROLL_BAR_WIDTH;
        int16_t scrollBarContainerY = bounds.y + COMPONENT_BORDER_THICKNESS;
        uint16_t scrollBarContainerHeight = bounds.height - (2 * COMPONENT_BORDER_THICKNESS);

        if (scrollBarContainerHeight <= 0 || SCROLL_BAR_WIDTH <= 0)
            return; // Nincs hely a scrollbarnak

        tft.fillRect(scrollBarContainerX, scrollBarContainerY, SCROLL_BAR_WIDTH, scrollBarContainerHeight, scrollBarBackgroundColor);

        float ratio = (float)visibleItemCount / dataSource->getItemCount();
        uint16_t thumbHeight = std::max(static_cast<int>(scrollBarContainerHeight * ratio), 10); // Minimális magasság

        // A thumbPosRatio számításának javítása, hogy elkerüljük a 0-val való osztást
        float thumbPosRatio = 0;
        int totalItems = dataSource->getItemCount();
        if (totalItems > visibleItemCount) { // Csak akkor van értelme a görgetésnek, ha több elem van, mint amennyi látható
            thumbPosRatio = (float)topItemIndex / (totalItems - visibleItemCount);
        }
        // Biztosítjuk, hogy a thumbPosRatio 0 és 1 között legyen
        thumbPosRatio = std::max(0.0f, std::min(thumbPosRatio, 1.0f));

        uint16_t thumbYInContainer = static_cast<int>((scrollBarContainerHeight - thumbHeight) * thumbPosRatio);
        uint16_t thumbActualY = scrollBarContainerY + thumbYInContainer;

        tft.fillRect(scrollBarContainerX, thumbActualY, SCROLL_BAR_WIDTH, thumbHeight, scrollBarColor);
    }

  public:
    /**
     * @brief Konstruktor a görgethető lista komponenshez.
     * @param bounds A komponens határai (Rect)
     * @param ds Az IScrollableListDataSource interfész implementációja, amely az adatokat szolgáltatja
     * @param visItems A látható elemek száma (alapértelmezett: 5)
     * @param itmHeight Az egyes listaelemek magassága (alapértelmezett: 0, ami a font magasságából számítódik)
     */
    UIScrollableListComponent(const Rect &bounds, IScrollableListDataSource *ds, uint8_t visItems = DEFAULT_VISIBLE_ITEMS, uint8_t itmHeight = 0) : UIComponent(bounds, ColorScheme::defaultScheme()), dataSource(ds) {

        if (itmHeight == 0) {
            // Item magasság számítása a font alapján, ha nincs megadva
            uint8_t prevSize = ::tft.textsize;     // Aktuális szövegméret mentése
            ::tft.setFreeFont(&FreeSansBold9pt7b); // Nagyobb font a magasság számításához
            ::tft.setTextSize(1);                  // Natív méret a FreeSansBold9pt7b-hez
            itemHeight = tft.fontHeight() + 8;     // Kis padding
            ::tft.setTextSize(prevSize);           // Eredeti szövegméret visszaállítása
        } else {
            itemHeight = itmHeight;
        }

        // Komponens háttérszínének beállítása feketére
        this->colors.background = TFT_COLOR_BACKGROUND; // UIComponent::colors.background

        // Listaelemek színeinek explicit beállítása
        itemTextColor = TFT_WHITE;              // Nem kiválasztott elem szövege
        selectedItemTextColor = TFT_BLACK;      // Kiválasztott elem szövege (fekete)
        selectedItemBackground = TFT_LIGHTGREY; // Kiválasztott elem háttere (világosszürke)

        // Görgetősáv színei
        scrollBarColor = TFT_LIGHTGREY;
        scrollBarBackgroundColor = TFT_DARKGREY;

        uint16_t contentAreaHeight = bounds.height - (2 * COMPONENT_BORDER_THICKNESS);
        if (contentAreaHeight > 0 && itemHeight > 0) {
            visibleItemCount = contentAreaHeight / itemHeight;
        }
    }

    /**
     * @brief Beállítja a látható elemek számát.
     * @param count A látható elemek száma.
     */
    void setDataSource(IScrollableListDataSource *ds) {
        dataSource = ds;
        topItemIndex = 0;
        selectedItemIndex = 0;
        markForRedraw();
    }

    /**
     * @brief Egyetlen listaelemet rajzol újra a megadott abszolút index alapján.
     * @param absoluteIndex A lista teljes hosszában vett indexe az újrarajzolandó elemnek.
     */
  public:
    // Publikus, hogy kívülről is hívható legyen egyedi elem frissítésére
    virtual void redrawListItem(int absoluteIndex) {
        if (!dataSource)
            return;

        // Ellenőrizzük, hogy az elem látható-e
        if (absoluteIndex < topItemIndex || absoluteIndex >= topItemIndex + visibleItemCount) {
            return; // Nem látható, nincs teendő
        }

        // Tartalomterület meghatározása a komponens keretén belül
        int16_t contentX = bounds.x + COMPONENT_BORDER_THICKNESS;
        int16_t contentY = bounds.y + COMPONENT_BORDER_THICKNESS;
        uint16_t contentWidth = bounds.width - (2 * COMPONENT_BORDER_THICKNESS) - SCROLL_BAR_WIDTH;
        uint16_t contentHeight = bounds.height - (2 * COMPONENT_BORDER_THICKNESS);

        if (contentWidth <= 0 || contentHeight <= 0)
            return;

        int visibleItemSlot = absoluteIndex - topItemIndex; // 0-tól (visibleItemCount-1)-ig
        int16_t itemRelY = visibleItemSlot * itemHeight;    // Y pozíció a contentY-hoz képest

        Rect itemVisualBounds(contentX, contentY + itemRelY, contentWidth, itemHeight);

        // Biztosítjuk, hogy az elem ne lógjon ki a tartalomterületből vertikálisan
        if (itemVisualBounds.y < contentY) {
            itemVisualBounds.height -= (contentY - itemVisualBounds.y);
            itemVisualBounds.y = contentY;
        }
        if (itemVisualBounds.y + itemVisualBounds.height > contentY + contentHeight) {
            itemVisualBounds.height = (contentY + contentHeight) - itemVisualBounds.y;
        }
        if (itemVisualBounds.height <= 0)
            return; // Nincs mit rajzolni

        // Szövegbeállítások mentése és visszaállítása
        uint8_t prevDatum = tft.getTextDatum();
        uint8_t prevSize = tft.textsize;
        tft.setTextDatum(ML_DATUM);

        // A fontot és méretet a label/value részeknél külön állítjuk
        if (absoluteIndex == selectedItemIndex) {
            // Kijelölés SELECTED_ITEM_PADDING pixellel kisebb
            tft.fillRect(itemVisualBounds.x + SELECTED_ITEM_PADDING, itemVisualBounds.y + SELECTED_ITEM_PADDING, itemVisualBounds.width - SELECTED_ITEM_RECT_REDUCTION,
                         itemVisualBounds.height - SELECTED_ITEM_RECT_REDUCTION, selectedItemBackground);
            tft.setTextColor(selectedItemTextColor, selectedItemBackground);
        } else {
            // Normál elem háttere (ez a "törlés" is, ha korábban ki volt választva)
            tft.fillRect(itemVisualBounds.x, itemVisualBounds.y, itemVisualBounds.width, itemVisualBounds.height, TFT_COLOR_BACKGROUND);
            tft.setTextColor(itemTextColor, TFT_COLOR_BACKGROUND);
        }

        String labelPart = dataSource->getItemLabelAt(absoluteIndex);
        String valuePart = dataSource->getItemValueAt(absoluteIndex);
        // Label rész rajzolása (nagyobb, balra igazított)
        tft.setFreeFont(&FreeSansBold9pt7b); // Nagyobb font a labelnek
        tft.setTextSize(1);                  // Natív méret
        tft.drawString(labelPart, itemVisualBounds.x + ITEM_TEXT_PADDING_X, itemVisualBounds.y + itemVisualBounds.height / 2);

        // Value rész rajzolása (kisebb, jobbra igazított)
        if (valuePart.length() > 0) {
            tft.setFreeFont(); // Kisebb, alapértelmezett font
            tft.setTextSize(1);
            tft.setTextDatum(MR_DATUM); // Middle Right
            tft.drawString(valuePart, itemVisualBounds.x + itemVisualBounds.width - ITEM_TEXT_PADDING_X, itemVisualBounds.y + itemVisualBounds.height / 2);
            tft.setTextDatum(ML_DATUM); // Visszaállítás ML_DATUM-ra a következő elemhez/állapothoz
        }

        // Szövegbeállítások visszaállítása
        tft.setTextDatum(prevDatum);
        tft.setTextSize(prevSize);
        // tft.setFreeFont(prevFont); // Ha egyedi fontot használnánk
    } /**
       * @brief Beállítja a látható elemek számát.
       * @param absoluteIndex A frissítendő elem abszolút indexe.
       */
    void refreshItemDisplay(int absoluteIndex) {
        if (!dataSource)
            return;
        if (absoluteIndex < 0 || absoluteIndex >= dataSource->getItemCount())
            return;

        redrawListItem(absoluteIndex);
        drawScrollBar(); // A görgetősávot is frissítjük, hátha az elem tartalma megváltozott
                         // (bár ebben az esetben valószínűleg nem változik a magassága)
    }

    // Touch margin növelése a lista elemeknél jobb felhasználói élményért
    virtual int16_t getTouchMargin() const override { return 4; }

    // A lista komponens nem igényel vizuális lenyomott visszajelzést, mert saját maga kezeli az újrarajzolást
    virtual bool allowsVisualPressedFeedback() const override { return false; }

    /**
     * @brief Beállítja a látható elemek számát.
     * @param count A látható elemek száma.
     */
    virtual void draw() override {
        if (!needsRedraw || !dataSource)
            return;

        // 1. Teljes komponens háttér törlése
        tft.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, colors.background); // Használjuk a UIComponent háttérszínét
        // 2. Komponens keretének rajzolása
        tft.drawRect(bounds.x, bounds.y, bounds.width, bounds.height, scrollBarColor); // Keret (pl. világosszürke)

        // Tartalomterület meghatározása a komponens keretén belül
        int16_t contentX = bounds.x + COMPONENT_BORDER_THICKNESS;
        int16_t contentY = bounds.y + COMPONENT_BORDER_THICKNESS;
        uint16_t contentWidth = bounds.width - (2 * COMPONENT_BORDER_THICKNESS) - SCROLL_BAR_WIDTH;
        uint16_t contentHeight = bounds.height - (2 * COMPONENT_BORDER_THICKNESS);

        if (contentWidth <= 0 || contentHeight <= 0) { // Nincs hely a tartalomnak
            drawScrollBar();                           // Scrollbar még lehet, ha csak az fér el
            needsRedraw = false;
            return;
        }

        // Szövegbeállítások mentése és visszaállítása a teljes lista rajzolásához
        uint8_t prevDatum = tft.getTextDatum();
        uint8_t prevSize = tft.textsize;
        int itemCount = dataSource->getItemCount();

        for (int i = 0; i < visibleItemCount; ++i) {
            int currentItemIndex = topItemIndex + i;
            if (currentItemIndex >= itemCount)
                break;

            int16_t itemRelY = i * itemHeight; // Y pozíció a contentY-hoz képest
            Rect itemVisualBounds(contentX, contentY + itemRelY, contentWidth, itemHeight);

            // Biztosítjuk, hogy az elem ne lógjon ki a tartalomterületből vertikálisan
            if (itemVisualBounds.y < contentY) {
                itemVisualBounds.height -= (contentY - itemVisualBounds.y);
                itemVisualBounds.y = contentY;
            }
            if (itemVisualBounds.y + itemVisualBounds.height > contentY + contentHeight) {
                itemVisualBounds.height = (contentY + contentHeight) - itemVisualBounds.y;
            }
            if (itemVisualBounds.height <= 0)
                continue;

            if (currentItemIndex == selectedItemIndex) {
                tft.fillRect(itemVisualBounds.x + SELECTED_ITEM_PADDING, itemVisualBounds.y + SELECTED_ITEM_PADDING, itemVisualBounds.width - SELECTED_ITEM_RECT_REDUCTION,
                             itemVisualBounds.height - SELECTED_ITEM_RECT_REDUCTION, selectedItemBackground);
                tft.setTextColor(selectedItemTextColor, selectedItemBackground);
            } else {
                // A háttér már fekete a fő fillRect miatt
                tft.setTextColor(itemTextColor, TFT_COLOR_BACKGROUND);
            }

            // Label rész rajzolása
            String labelPart = dataSource->getItemLabelAt(currentItemIndex);
            tft.setTextDatum(ML_DATUM);
            tft.setFreeFont(&FreeSansBold9pt7b);
            tft.setTextSize(1);
            tft.drawString(labelPart, itemVisualBounds.x + ITEM_TEXT_PADDING_X, itemVisualBounds.y + itemVisualBounds.height / 2);

            // Value rész rajzolása
            String valuePart = dataSource->getItemValueAt(currentItemIndex);
            if (valuePart.length() > 0) {
                tft.setTextDatum(MR_DATUM);
                tft.setFreeFont(); // Kisebb font
                tft.setTextSize(1);
                tft.drawString(valuePart, itemVisualBounds.x + itemVisualBounds.width - ITEM_TEXT_PADDING_X, itemVisualBounds.y + itemVisualBounds.height / 2);
            }
        }
        // Szövegbeállítások visszaállítása
        tft.setTextDatum(prevDatum);
        tft.setTextSize(prevSize);

        drawScrollBar();
        needsRedraw = false;
    }

    /**
     * @brief Beállítja, hogy a lista elemei ne legyenek láthatóak.
     * @param disable true, ha el akarjuk rejteni a listát, false, ha újra meg akarjuk jeleníteni.
     */
    virtual bool handleRotary(const RotaryEvent &event) override {
        if (disabled || !dataSource || dataSource->getItemCount() == 0)
            return false;

        bool handled = false;
        int oldSelectedIndex = selectedItemIndex;
        int oldTopItemIndex = topItemIndex;

        if (event.direction == RotaryEvent::Direction::Up) {
            selectedItemIndex--;
            if (selectedItemIndex < 0)
                selectedItemIndex = 0;
            handled = true;
        } else if (event.direction == RotaryEvent::Direction::Down) {
            selectedItemIndex++;
            if (selectedItemIndex >= dataSource->getItemCount())
                selectedItemIndex = dataSource->getItemCount() - 1;
            handled = true;
        }

        if (event.buttonState == RotaryEvent::ButtonState::Clicked) {
            bool fullRedrawNeeded = dataSource->onItemClicked(selectedItemIndex);
            if (fullRedrawNeeded) {
                // Az onItemClicked kérte a teljes újrarajzolást.
                markForRedraw();
            }
            handled = true;
            return handled; // Kilépés a kattintás után
        }

        if (oldSelectedIndex != selectedItemIndex) {
            // Görgetés, ha a kiválasztás kilóg a látható részből
            if (selectedItemIndex < topItemIndex) {
                topItemIndex = selectedItemIndex;
            } else if (selectedItemIndex >= topItemIndex + visibleItemCount) {
                topItemIndex = selectedItemIndex - visibleItemCount + 1;
            }

            if (oldTopItemIndex != topItemIndex) {
                // A látható elemek megváltoztak, teljes újrarajzolás szükséges
                markForRedraw();
            } else {
                // Csak a kiválasztás változott a látható elemeken belül
                redrawListItem(oldSelectedIndex);  // Régi kiválasztott normál stílussal
                redrawListItem(selectedItemIndex); // Új kiválasztott kiemelt stílussal
                drawScrollBar();                   // Görgetősáv frissítése
            }
        }
        return handled;
    }

    virtual bool handleTouch(const TouchEvent &event) override {
        if (disabled || !dataSource || !bounds.contains(event.x, event.y) || dataSource->getItemCount() == 0) {
            return false;
        }

        if (event.pressed) { // Csak a lenyomásra reagálunk itt, a kattintást a UIComponent kezeli
            // Érintés helyének ellenőrzése a tényleges tartalomterületen belül
            int16_t contentMinY = bounds.y + COMPONENT_BORDER_THICKNESS;
            int16_t contentMaxY = bounds.y + bounds.height - COMPONENT_BORDER_THICKNESS;
            int16_t contentMinX = bounds.x + COMPONENT_BORDER_THICKNESS;
            int16_t contentMaxX = bounds.x + bounds.width - COMPONENT_BORDER_THICKNESS - SCROLL_BAR_WIDTH;

            if (event.y < contentMinY || event.y >= contentMaxY || event.x < contentMinX || event.x >= contentMaxX) {
                return UIComponent::handleTouch(event); // Scrollbar vagy border touch, adjuk tovább
            }

            int touchedItemOffset = (event.y - contentMinY) / itemHeight;
            if (touchedItemOffset >= 0 && touchedItemOffset < visibleItemCount) {
                int newSelectedItemIndex = topItemIndex + touchedItemOffset;
                if (newSelectedItemIndex < dataSource->getItemCount()) {
                    if (selectedItemIndex != newSelectedItemIndex) {
                        // A selectedItemIndex frissítését és az újrarajzolást az onClick-re bízzuk,
                        // hogy a debounce után történjen meg, de a logikát itt előkészítjük.
                        // Az UIComponent::handleTouch fogja ezt tovább vinni az onClick-ig.
                    }
                    // A tényleges onItemClicked hívást az UIComponent::onClick-re bízzuk,
                    // miután a debounce és egyéb ellenőrzések lefutottak.
                }
            }
        }
        return UIComponent::handleTouch(event); // Átadjuk az ősosztálynak a kattintáskezeléshez
    }

  protected:
    virtual bool onClick(const TouchEvent &event) override {
        if (disabled || !dataSource || dataSource->getItemCount() == 0) {
            UIComponent::onClick(event);
            return false;
        }

        bool handled = false;

        int16_t contentMinY = bounds.y + COMPONENT_BORDER_THICKNESS;
        int16_t contentMaxY = bounds.y + bounds.height - COMPONENT_BORDER_THICKNESS;
        int16_t contentMinX = bounds.x + COMPONENT_BORDER_THICKNESS;
        int16_t contentMaxX = bounds.x + bounds.width - COMPONENT_BORDER_THICKNESS - SCROLL_BAR_WIDTH;

        if (event.y >= contentMinY && event.y < contentMaxY && event.x >= contentMinX && event.x < contentMaxX) {
            int touchedItemOffset = (event.y - contentMinY) / itemHeight;
            if (touchedItemOffset >= 0 && touchedItemOffset < visibleItemCount) {
                int newSelectedItemIndex = topItemIndex + touchedItemOffset;
                if (newSelectedItemIndex < dataSource->getItemCount()) {
                    int oldSelectedIndex = selectedItemIndex;
                    selectedItemIndex = newSelectedItemIndex; // Itt frissítjük a kiválasztást

                    // Csak akkor rajzoljuk újra a listát, ha megváltozott a kiválasztás
                    if (oldSelectedIndex != selectedItemIndex) {
                        redrawListItem(oldSelectedIndex);  // Régi kiválasztott normál stílussal
                        redrawListItem(selectedItemIndex); // Új kiválasztott kiemelt stílussal
                    }

                    bool fullRedrawNeeded = dataSource->onItemClicked(selectedItemIndex); // És itt hívjuk a callback-et
                    if (fullRedrawNeeded) {
                        markForRedraw(); // Teljes újrarajzolás a kattintás után, ha a dataSource kéri
                    }

                    handled = true; // Kattintás kezelve
                }
            }
        }
        UIComponent::onClick(event); // Hívjuk az ősosztályt is, ha van benne logika

        return handled;
    }
};
