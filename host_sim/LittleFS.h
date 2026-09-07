// LittleFS.h — minimal in-memory stand-in for the ESP8266 LittleFS API,
// just enough to compile and run drift_compensation.cpp's checkpoint
// persistence natively on a dev machine. NOT part of the firmware — see
// host_sim/Arduino.h for why this exists.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <vector>

enum { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };

class File {
public:
  File() : valid_(false), data_(nullptr), pos_(0) {}
  explicit File(std::vector<uint8_t> *data) : valid_(true), data_(data), pos_(0) {}

  operator bool() const { return valid_; }

  size_t write(const uint8_t *buf, size_t len) {
    if (!valid_) return 0;
    if (pos_ + len > data_->size()) data_->resize(pos_ + len);
    std::memcpy(data_->data() + pos_, buf, len);
    pos_ += len;
    return len;
  }

  size_t read(uint8_t *buf, size_t len) {
    if (!valid_) return 0;
    size_t avail = (pos_ < data_->size()) ? (data_->size() - pos_) : 0;
    size_t n = (len < avail) ? len : avail;
    std::memcpy(buf, data_->data() + pos_, n);
    pos_ += n;
    return n;
  }

  bool seek(size_t pos, int mode = SeekSet) {
    (void)mode;
    pos_ = pos;
    return true;
  }

  size_t size() const { return data_ ? data_->size() : 0; }
  void close() {}

private:
  bool valid_;
  std::vector<uint8_t> *data_;
  size_t pos_;
};

class LittleFSClass {
public:
  bool begin() { return true; }

  bool exists(const char *path) { return files_.count(path) > 0; }

  File open(const char *path, const char *mode) {
    std::string m(mode);
    if (m == "w") {
      files_[path] = std::vector<uint8_t>();
      return File(&files_[path]);
    }
    if (m == "r" || m == "r+") {
      auto it = files_.find(path);
      if (it == files_.end()) return File();
      return File(&it->second);
    }
    return File();
  }

private:
  std::map<std::string, std::vector<uint8_t>> files_;
};

extern LittleFSClass LittleFS;
