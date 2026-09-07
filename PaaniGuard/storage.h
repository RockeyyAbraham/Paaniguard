// storage.h — LittleFS circular buffer of quality + usage readings.
// Fixed capacity (STORAGE_MAX_RECORDS); once full, the oldest record is
// silently overwritten — appends never fail and never crash when full.

#ifndef PAANIGUARD_STORAGE_H
#define PAANIGUARD_STORAGE_H

#include <Arduino.h>

struct StoredRecord {
  uint32_t timestamp;       // millis() (or NTP epoch seconds once synced) at capture time
  float ph;
  float correctedTdsPpm;
  float temperatureC;
  float flowRateLPM;
  float totalLiters;
  uint8_t severity;         // Severity enum value, stored as a plain int
};

void storage_init();

// Always succeeds: writes into the next ring-buffer slot, overwriting the
// oldest record once STORAGE_MAX_RECORDS is reached.
void storage_appendRecord(const StoredRecord &rec);

int storage_getRecordCount();

// index 0 = oldest surviving record, storage_getRecordCount()-1 = newest.
bool storage_readRecord(int indexFromOldest, StoredRecord &out);

#endif // PAANIGUARD_STORAGE_H
