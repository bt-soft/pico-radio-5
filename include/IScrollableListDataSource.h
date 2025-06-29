#ifndef __ISCROLLABLE_LIST_DATA_SOURCE_H
#define __ISCROLLABLE_LIST_DATA_SOURCE_H

#include <Arduino.h>

/**
 * @brief Interfész a UIScrollableListComponent adatforrásához.
 *
 * Ez az interfész definiálja azokat a metódusokat, amelyeket egy
 * adatforrásnak implementálnia kell, hogy a UIScrollableListComponent
 * meg tudja jeleníteni az elemeit.
 */
class IScrollableListDataSource {
  public:
    virtual ~IScrollableListDataSource() = default;

    /** @brief Visszaadja a lista elemeinek teljes számát. */
    virtual int getItemCount() const = 0;

    /** @brief Visszaadja a megadott indexű elem címke (label) részét. */
    virtual String getItemLabelAt(int index) const = 0;

    /** @brief Visszaadja a megadott indexű elem érték (value) részét. Lehet üres String, ha nincs érték. */
    virtual String getItemValueAt(int index) const = 0;

    /**
     * @brief Akkor hívódik meg, amikor egy elemre kattintanak (kiválasztják).
     * @return true, ha a lista komponensnek továbbra is teljes újrarajzolást kell végeznie, false egyébként.
     */
    virtual bool onItemClicked(int index) = 0;
};

#endif // __ISCROLLABLE_LIST_DATA_SOURCE_H