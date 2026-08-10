#pragma once

#include <cstddef>

namespace keydrop {

/// Thread-local heap allocation tracker.
///
/// Hooks global operator new / operator delete via override in the
/// benchmark translation unit.  Call HeapTracker::begin() / end()
/// (or use HeapTrackerScope) around the region you want to measure.
///
/// The tracker is recursive-safe: allocations made inside the
/// tracking code itself are not counted.
class HeapTracker {
public:
    /// Number of heap allocations captured in the current window.
    static std::size_t allocation_count();

    /// Total bytes requested across all captured allocations.
    static std::size_t allocated_bytes();

    /// Reset counters to zero and enable tracking.
    static void begin();

    /// Disable tracking (counters retain their values).
    static void end();

    /// Reset counters to zero (tracking state unchanged).
    static void reset();

private:
    friend void* ::operator new(std::size_t size);
    friend void* ::operator new[](std::size_t size);
    friend void  ::operator delete(void* ptr) noexcept;
    friend void  ::operator delete[](void* ptr) noexcept;

    static bool is_tracking();
    static void record_allocation(std::size_t size);
    static void record_deallocation();
};

/// RAII guard: calls begin() on construction, end() on destruction.
class HeapTrackerScope {
public:
    HeapTrackerScope()  { HeapTracker::begin(); }
    ~HeapTrackerScope() { HeapTracker::end(); }
};

} // namespace keydrop
