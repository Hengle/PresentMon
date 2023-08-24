#pragma once

#include <Windows.h>
#include <stdint.h>
#include <iostream>
#include <vector>

// Extremely simple memory buffer helper class that is used
// to create and grow memory buffers. This class was specifically
// created for the shared named pipe code to assist in managing
// the memory buffers used in the ReadFile and WriteFile calls.
class MemBuffer {
 public:
  MemBuffer() {}
  size_t GetCurrentSize() { return buffer_.size(); }
  bool AddItem(LPVOID item, size_t item_size) {
    if (item == nullptr) {
      return false;
    }
    BYTE* copy_ptr = (BYTE*)item;
    for (size_t i = 0; i < item_size; i++) {
      try {
        buffer_.emplace_back(*copy_ptr++);
      } catch (...) {
        return false;
      }
    }
    return true;
  }
  LPVOID AccessMem() { return buffer_.data(); }
  void ClearMemory() { buffer_.clear(); }

 private:
  std::vector<BYTE> buffer_;
};