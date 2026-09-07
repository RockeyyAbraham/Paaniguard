// storage.cpp — fixed-layout ring buffer on LittleFS. The file is
// preallocated to header + STORAGE_MAX_RECORDS slots at init, so every
// append is an in-place overwrite of one slot plus the header — no
// filesystem growth, no fragmentation, and no failure mode when full: the
// write index just wraps and the oldest slot is overwritten.

#include "storage.h"
#include "config.h"
#include <LittleFS.h>

struct StorageHeader {
  uint32_t magic;
  uint32_t writeIndex; // next slot to write, mod STORAGE_MAX_RECORDS
  uint32_t count;      // valid record count, capped at STORAGE_MAX_RECORDS
};

static const uint32_t STORAGE_MAGIC = 0x50475354; // "PGST"
static StorageHeader header;

static size_t recordOffset(uint32_t slot) {
  return sizeof(StorageHeader) + (size_t)slot * sizeof(StoredRecord);
}

static void initFreshFile() {
  File f = LittleFS.open(STORAGE_FILE_PATH, "w");
  if (!f) {
    Serial.println(F("storage: failed to create log file"));
    return;
  }
  header.magic = STORAGE_MAGIC;
  header.writeIndex = 0;
  header.count = 0;
  f.write((const uint8_t *)&header, sizeof(header));

  StoredRecord blank;
  memset(&blank, 0, sizeof(blank));
  for (uint32_t i = 0; i < STORAGE_MAX_RECORDS; i++) {
    f.write((const uint8_t *)&blank, sizeof(blank));
  }
  f.close();
}

void storage_init() {
  LittleFS.begin();

  size_t expectedSize = sizeof(StorageHeader) + (size_t)STORAGE_MAX_RECORDS * sizeof(StoredRecord);
  bool needsInit = true;

  if (LittleFS.exists(STORAGE_FILE_PATH)) {
    File f = LittleFS.open(STORAGE_FILE_PATH, "r");
    if (f) {
      if (f.size() == expectedSize) {
        f.read((uint8_t *)&header, sizeof(header));
        needsInit = (header.magic != STORAGE_MAGIC);
      }
      f.close();
    }
  }

  if (needsInit) {
    initFreshFile();
  }
}

void storage_appendRecord(const StoredRecord &rec) {
  File f = LittleFS.open(STORAGE_FILE_PATH, "r+");
  if (!f) {
    Serial.println(F("storage: append failed, could not open log file"));
    return;
  }

  uint32_t slot = header.writeIndex % STORAGE_MAX_RECORDS;
  f.seek(recordOffset(slot), SeekSet);
  f.write((const uint8_t *)&rec, sizeof(rec));

  header.writeIndex = (header.writeIndex + 1) % STORAGE_MAX_RECORDS;
  if (header.count < STORAGE_MAX_RECORDS) {
    header.count++;
  }

  f.seek(0, SeekSet);
  f.write((const uint8_t *)&header, sizeof(header));
  f.close();
}

int storage_getRecordCount() {
  return header.count;
}

bool storage_readRecord(int indexFromOldest, StoredRecord &out) {
  if (indexFromOldest < 0 || (uint32_t)indexFromOldest >= header.count) {
    return false;
  }

  // When the buffer hasn't wrapped yet, slot 0 is the oldest. Once it has
  // wrapped, writeIndex points at the next slot to be overwritten, which is
  // exactly the oldest surviving record.
  uint32_t oldestSlot = (header.count < STORAGE_MAX_RECORDS) ? 0 : header.writeIndex;
  uint32_t slot = (oldestSlot + (uint32_t)indexFromOldest) % STORAGE_MAX_RECORDS;

  File f = LittleFS.open(STORAGE_FILE_PATH, "r");
  if (!f) return false;
  f.seek(recordOffset(slot), SeekSet);
  size_t bytesRead = f.read((uint8_t *)&out, sizeof(out));
  f.close();
  return bytesRead == sizeof(out);
}
