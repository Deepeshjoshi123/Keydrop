#pragma once

#include <functional>
#include <utility>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#else

#include <thread>

#endif

// Small test-only wrapper. Older MinGW distributions do not implement
// std::thread even though they otherwise support the C++17 library.
class TestThread {
public:
    explicit TestThread(std::function<void()> function)
#ifdef _WIN32
        : function_(std::move(function))
    {
        handle_ = ::CreateThread(nullptr, 0, &TestThread::entry, this, 0, nullptr);
    }
#else
        : thread_(std::move(function))
    {
    }
#endif

    TestThread(const TestThread&) = delete;
    TestThread& operator=(const TestThread&) = delete;

    ~TestThread()
    {
#ifdef _WIN32
        if (handle_ != nullptr)
        {
            ::CloseHandle(handle_);
        }
#endif
    }

    void join()
    {
#ifdef _WIN32
        if (handle_ != nullptr)
        {
            (void)::WaitForSingleObject(handle_, INFINITE);
        }
#else
        thread_.join();
#endif
    }

private:
#ifdef _WIN32
    static DWORD WINAPI entry(void* context)
    {
        static_cast<TestThread*>(context)->function_();
        return 0;
    }

    std::function<void()> function_;
    HANDLE handle_ = nullptr;
#else
    std::thread thread_;
#endif
};
