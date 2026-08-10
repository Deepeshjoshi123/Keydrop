#include "keydrop/benchmark/heap_tracker.hpp"

#include <cstdlib>
#include <new>

namespace {

// ── Per-thread state ─────────────────────────────────────────────
thread_local bool      t_tracking   = false;
thread_local bool      t_in_tracker = false;   // recursion guard
thread_local std::size_t t_alloc_count = 0;
thread_local std::size_t t_alloc_bytes = 0;

} // namespace

namespace keydrop {

std::size_t HeapTracker::allocation_count() { return t_alloc_count; }
std::size_t HeapTracker::allocated_bytes()  { return t_alloc_bytes; }

void HeapTracker::begin()
{
    t_alloc_count = 0;
    t_alloc_bytes = 0;
    t_tracking = true;
}

void HeapTracker::end()
{
    t_tracking = false;
}

void HeapTracker::reset()
{
    t_alloc_count = 0;
    t_alloc_bytes = 0;
}

bool HeapTracker::is_tracking()
{
    return t_tracking && !t_in_tracker;
}

void HeapTracker::record_allocation(std::size_t size)
{
    ++t_alloc_count;
    t_alloc_bytes += size;
}

void HeapTracker::record_deallocation()
{
    // Gross allocation tracking: deallocations do not decrement.
    // We measure total allocation activity, not net live memory.
}

} // namespace keydrop

// ── Global operator new / delete overrides ───────────────────────

void* operator new(std::size_t size)
{
    if (keydrop::HeapTracker::is_tracking())
    {
        // Guard against recursion from malloc itself calling operator new
        struct RecursionGuard {
            RecursionGuard() { t_in_tracker = true; }
            ~RecursionGuard() { t_in_tracker = false; }
        } guard;

        void* ptr = std::malloc(size);
        if (ptr == nullptr)
            throw std::bad_alloc();
        keydrop::HeapTracker::record_allocation(size);
        return ptr;
    }

    void* ptr = std::malloc(size);
    if (ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}

void* operator new[](std::size_t size)
{
    return operator new(size);
}

void operator delete(void* ptr) noexcept
{
    if (ptr == nullptr)
        return;

    if (keydrop::HeapTracker::is_tracking())
    {
        struct RecursionGuard {
            RecursionGuard() { t_in_tracker = true; }
            ~RecursionGuard() { t_in_tracker = false; }
        } guard;

        keydrop::HeapTracker::record_deallocation();
    }

    std::free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    operator delete(ptr);
}

// Sized deallocation (C++14)
void operator delete(void* ptr, std::size_t /*size*/) noexcept
{
    operator delete(ptr);
}

void operator delete[](void* ptr, std::size_t /*size*/) noexcept
{
    operator delete(ptr);
}
