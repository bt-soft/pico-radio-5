#include <Arduino.h>

#include "PicoMemoryInfo.h"

namespace PicoMemoryInfo {

#ifdef __DEBUG
// Globális memóriafigyelő objektum, csak DEBUG módban
UsedHeapMemoryMonitor usedHeapMemoryMonitor;
#endif

/**
 * Memóriaállapot lekérdezése
 */
MemoryStatus_t getMemoryStatus() {
    MemoryStatus_t status;

    // Flash memória méretének meghatározása
    status.programSize = (uint32_t)&__flash_binary_end - 0x10000000;
    status.programPercent = (status.programSize * 100.0) / FULL_FLASH_SIZE;
    status.freeFlash = FULL_FLASH_SIZE - status.programSize;
    status.freeFlashPercent = 100.0 - status.programPercent;

    // Heap memória (RAM)
    RP2040 rp2040;
    status.heapSize = rp2040.getTotalHeap();
    status.usedHeap = rp2040.getUsedHeap();
    status.freeHeap = rp2040.getFreeHeap();

    // Százalékszámítás a heap teljes méretére vonatkozóan
    status.usedHeapPercent = (status.usedHeap * 100.0) / status.heapSize;
    status.freeHeapPercent = (status.freeHeap * 100.0) / status.heapSize;

#ifdef __DEBUG
    // Used Heap mért érték hozzáadása
    usedHeapMemoryMonitor.addMeasurement(status.usedHeap);
#endif

    return status;
}

/**
 * Debug módban az adatok kiírása
 */
#ifdef __DEBUG
void debugMemoryInfo() {

    MemoryStatus_t status = getMemoryStatus(); // Adatok lekérése

    Serial.flush();

    DEBUG("===== Memory info =====\n");

    // Program memória (flash)
    DEBUG("Flash\t\t\t\t\t\tHeap\n");
    DEBUG("Total: %d B (%.2f kB)\t\t\t%d B (%.2f kB)\n", FULL_FLASH_SIZE, FULL_FLASH_SIZE / 1024.0, // Flash
          status.heapSize, status.heapSize / 1024.0                                                 // Heap
    );
    DEBUG("Used: %d B (%.2f kB) - %.2f%%\t\t%d B (%.2f kB) - %.2f%%\n", status.programSize, status.programSize / 1024.0, status.programPercent, // Flash
          status.usedHeap, status.usedHeap / 1024.0, status.usedHeapPercent                                                                     // Heap
    );
    DEBUG("Free: %d B (%.2f kB) - %.2f%%\t\t%d B (%.2f kB) - %.2f%%\n", status.freeFlash, status.freeFlash / 1024.0, status.freeFlashPercent, // Flash
          status.freeHeap, status.freeHeap / 1024.0, status.freeHeapPercent                                                                   // Heap
    );

    DEBUG("Heap usage:\n changed(from prev): %.2f kB, ave: %.2f kB - (%d/%d)\n",
          usedHeapMemoryMonitor.getChangeFromPreviousMeasurement() / 1024.0, // Az előző méréshez képesti változás
          usedHeapMemoryMonitor.getAverageUsedHeap() / 1024.0,               // average
          usedHeapMemoryMonitor.index, MEASUREMENTS_COUNT                    // max grow
    );

    DEBUG("---\n");
    DEBUG("\n");
    Serial.flush();
}
#endif

} // namespace PicoMemoryInfo
