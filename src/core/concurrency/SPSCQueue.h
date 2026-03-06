#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace Sonatrix {
namespace Core {
namespace Concurrency {

// -----------------------------------------------------------------------------
// SPSCQueue (Single-Producer Single-Consumer Lock-Free Ring Buffer)
//
// Designed explicitly for pushing UI parameter changes (Producer: Main Thread)
// down to the Audio Engine (Consumer: CoreAudio Real-Time Render Thread).
// Avoids all locks, mutexes, and allocations during audio processing.
// -----------------------------------------------------------------------------

template <typename T> class SPSCQueue {
public:
  explicit SPSCQueue(size_t capacity)
      : buffer(capacity + 1), capacity_(capacity + 1) {
    head.store(0, std::memory_order_relaxed);
    tail.store(0, std::memory_order_relaxed);
  }

  // Called by the UI (Main Thread)
  bool Push(const T &item) {
    size_t currentTail = tail.load(std::memory_order_relaxed);
    size_t nextTail = (currentTail + 1) % capacity_;

    // Is the queue full?
    if (nextTail == head.load(std::memory_order_acquire)) {
      return false;
    }

    // Write data
    buffer[currentTail] = item;

    // Commit memory
    tail.store(nextTail, std::memory_order_release);
    return true;
  }

  // Called by CoreAudio (DSP Thread)
  bool Pop(T &outItem) {
    size_t currentHead = head.load(std::memory_order_relaxed);

    // Is the queue empty?
    if (currentHead == tail.load(std::memory_order_acquire)) {
      return false;
    }

    // Read data
    outItem = buffer[currentHead];

    // Commit memory
    head.store((currentHead + 1) % capacity_, std::memory_order_release);
    return true;
  }

private:
  std::vector<T> buffer;
  const size_t capacity_;
  alignas(64) std::atomic<size_t> head; // Padding prevents false sharing
  alignas(64) std::atomic<size_t> tail;
};

} // namespace Concurrency
} // namespace Core
} // namespace Sonatrix
