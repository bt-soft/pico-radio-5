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

    // Arduino-kompatibilis float-to-string konverzió
    char flashTotalKb[16], heapTotalKb[16];
    char flashUsedKb[16], flashUsedPct[16], heapUsedKb[16], heapUsedPct[16];
    char flashFreeKb[16], flashFreePct[16], heapFreeKb[16], heapFreePct[16];
    char heapChangeKb[16], heapAveKb[16];

    dtostrf(FULL_FLASH_SIZE / 1024.0, 8, 2, flashTotalKb);
    dtostrf(status.heapSize / 1024.0, 8, 2, heapTotalKb);
    DEBUG("Total: %d B (%s kB)\t\t\t%d B (%s kB)\n", FULL_FLASH_SIZE, flashTotalKb, status.heapSize, heapTotalKb);

    dtostrf(status.programSize / 1024.0, 8, 2, flashUsedKb);
    dtostrf(status.programPercent, 6, 2, flashUsedPct);
    dtostrf(status.usedHeap / 1024.0, 8, 2, heapUsedKb);
    dtostrf(status.usedHeapPercent, 6, 2, heapUsedPct);
    DEBUG("Used: %d B (%s kB) - %s%%\t\t%d B (%s kB) - %s%%\n", status.programSize, flashUsedKb, flashUsedPct, status.usedHeap, heapUsedKb, heapUsedPct);

    dtostrf(status.freeFlash / 1024.0, 8, 2, flashFreeKb);
    dtostrf(status.freeFlashPercent, 6, 2, flashFreePct);
    dtostrf(status.freeHeap / 1024.0, 8, 2, heapFreeKb);
    dtostrf(status.freeHeapPercent, 6, 2, heapFreePct);
    DEBUG("Free: %d B (%s kB) - %s%%\t\t%d B (%s kB) - %s%%\n", status.freeFlash, flashFreeKb, flashFreePct, status.freeHeap, heapFreeKb, heapFreePct);

    dtostrf(usedHeapMemoryMonitor.getChangeFromPreviousMeasurement() / 1024.0, 8, 2, heapChangeKb);
    dtostrf(usedHeapMemoryMonitor.getAverageUsedHeap() / 1024.0, 8, 2, heapAveKb);
    DEBUG("Heap usage:\n changed(from prev): %s kB, ave: %s kB - (%d/%d)\n", heapChangeKb, heapAveKb, usedHeapMemoryMonitor.index, MEASUREMENTS_COUNT);

    DEBUG("---\n");
    DEBUG("\n");
    Serial.flush();
}
#endif

} // namespace PicoMemoryInfo
