#ifndef __SCREEN_SETUP_BASE_H
#define __SCREEN_SETUP_BASE_H

#include "IScrollableListDataSource.h"
#include "UIScreen.h"
#include "UIScrollableListComponent.h"
#include "defines.h"

/**
 * @brief Setup képernyők alaposztálya.
 *
 * Ez az osztály tartalmazza a közös funktionalitást minden setup képernyőhöz:
 * - Görgethető lista kezelése
 * - Exit gomb
 * - Alapvető layout és megjelenítés
 * - Menüpont struktúra és kezelés
 */
class ScreenSetupBase : public UIScreen, public IScrollableListDataSource {
  protected:
    /**
     * @brief Menüpont akció típusok - leszármazott osztályok definiálják a specifikus értékeket
     */
    enum class BaseItemAction {
        NONE = 0,
        // Leszármazott osztályok kiterjeszthetik ezt az enumot
        // pl: BRIGHTNESS = 100, VOLUME = 101, stb.
    };

    /**
     * @brief Menüpont struktúra
     */
    struct SettingItem {
        const char *label;        ///< Menüpont címkéje
        String value;             ///< Menüpont értéke
        int action;               ///< Akció azonosító (leszármazott osztály definiálja)
        bool isSubmenu;           ///< True ha ez egy almenü (navigáció másik képernyőre)
        const char *targetScreen; ///< Cél képernyő neve almenü esetén

        SettingItem(const char *l, const String &v, int a, bool sub = false, const char *target = nullptr) : label(l), value(v), action(a), isSubmenu(sub), targetScreen(target) {}
    };

    // UI komponensek
    std::shared_ptr<UIScrollableListComponent> menuList;
    std::vector<SettingItem> settingItems;
    std::shared_ptr<UIButton> exitButton;

    // Közös segédfüggvények
    void updateListItem(int index);
    void createCommonUI(const char *title);

    /**
     * @brief Virtuális metódusok - leszármazott osztályok implementálják
     */
    virtual void populateMenuItems() = 0;
    virtual void handleItemAction(int index, int action) = 0;
    virtual const char *getScreenTitle() const = 0;

  public:
    /**
     * @brief Konstruktor.
     * @param tft TFT_eSPI referencia
     * @param screenName Képernyő neve
     */
    ScreenSetupBase(const char *screenName);
    virtual ~ScreenSetupBase() = default;

    /**
     * @brief UI komponensek létrehozása és elhelyezése
     * Ez a metódus hívja meg a createCommonUI-t a leszármazott konstruktor után
     */
    virtual void layoutComponents();

    // UIScreen interface
    virtual void activate() override;
    virtual void drawContent() override;

    // IScrollableListDataSource interface
    virtual int getItemCount() const override;
    virtual String getItemLabelAt(int index) const override;
    virtual String getItemValueAt(int index) const override;
    virtual bool onItemClicked(int index) override;
};

#endif // __SCREEN_SETUP_BASE_H