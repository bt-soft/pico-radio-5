#pragma once

#include "IScrollableListDataSource.h"
#include "StationData.h"
#include "StationStore.h"
#include "UIHorizontalButtonBar.h"
#include "UIScreen.h"
#include "UIScrollableListComponent.h"

/**
 * @brief Memória képernyő állomások kezeléséhez
 * @details Listázza, szerkeszti és kezeli a tárolt állomásokat
 */
class ScreenMemory : public UIScreen, public IScrollableListDataSource {
  public:
    /**
     * @brief Konstruktor
     */
    ScreenMemory();
    virtual ~ScreenMemory();

    // UIScreen interface
    virtual bool handleRotary(const RotaryEvent &event) override;
    virtual void handleOwnLoop() override;
    virtual void drawContent() override;
    virtual void activate() override;
    virtual void onDialogClosed(UIDialogBase *closedDialog) override;
    virtual void setParameters(void *params) override;

    // IScrollableListDataSource interface
    virtual int getItemCount() const override;
    virtual String getItemLabelAt(int index) const override;
    virtual String getItemValueAt(int index) const override;
    virtual bool onItemClicked(int index) override;

  private:
    String rdsStationName; // RDS állomásnév, ha van

    // Vízszintes gombsor azonosítók
    static constexpr uint8_t ADD_CURRENT_BUTTON = 30;
    static constexpr uint8_t EDIT_BUTTON = 31;
    static constexpr uint8_t DELETE_BUTTON = 32;
    static constexpr uint8_t BACK_BUTTON = 33;

    // UI komponensek
    std::shared_ptr<UIScrollableListComponent> memoryList;
    std::shared_ptr<UIHorizontalButtonBar> horizontalButtonBar;
    std::shared_ptr<UIButton> backButton;

    // Adatok
    std::vector<StationData> stations;
    int selectedIndex = -1;
    int lastTunedIndex = -1; // Utolsó behangolt állomás indexe optimalizált frissítéshez
    bool isFmMode = true;    // Dialógus állapotok
    enum class DialogState { None, AddingStation, EditingStationName, ConfirmingDelete } currentDialogState = DialogState::None;

    StationData pendingStation;    // Új állomás hozzáadásakor
    char deleteMessageBuffer[200]; // Buffer a delete dialógus üzenetéhez

    // Metódusok
    void layoutComponents();
    void createHorizontalButtonBar();
    void updateHorizontalButtonStates();
    void loadStations();
    void refreshList();
    void refreshCurrentTunedIndication();
    void refreshTunedIndicationOptimized(); // Optimalizált frissítés csak az érintett elemekre

    // Gomb eseménykezelők
    void handleAddCurrentButton(const UIButton::ButtonEvent &event);
    void handleEditButton(const UIButton::ButtonEvent &event);
    void handleDeleteButton(const UIButton::ButtonEvent &event);
    void handleBackButton(const UIButton::ButtonEvent &event); // Dialógus kezelők
    void showAddStationDialog();
    void showEditStationDialog();
    void showDeleteConfirmDialog();
    void showStationExistsDialog(); // Állomás műveletek
    void tuneToStation(int index);
    void addCurrentStation(const String &name);
    void updateStationName(int index, const String &newName);
    void deleteStation(int index); // Segéd metódusok
    StationData getCurrentStationData();
    bool isCurrentStationInMemory();
    bool isStationCurrentlyTuned(const StationData &station);
    String formatFrequency(uint16_t frequency, bool isFm) const;
    String getModulationName(uint8_t modulation) const;
    bool isCurrentBandFm();
    uint8_t getCurrentStationCount() const;
    uint8_t getMaxStationCount() const;
    bool isMemoryFull() const;

  private:
    // Konstansok
    static constexpr const char *CURRENT_TUNED_ICON = "> ";
};
