#pragma once

#include <string>
#include <functional>
#include <chrono>
#include <Windows.h>

// #define UWC_DEBUG_ON


// Window utilities
bool IsFullScreenWindow(HWND hWnd);
bool IsAltTabWindow(HWND hWnd);
UINT GetWindowZOrder(HWND hWnd);
bool GetWindowTitle(HWND hWnd, std::wstring& outTitle);
bool GetWindowTitle(HWND hWnd, std::wstring& outTitle, int timeout);
bool GetWindowClassName(HWND hWnd, std::string& outClassName);
bool IsUWP(DWORD pid);
bool IsApplicationFrameWindow(const std::string& className);


// Releaser
class ScopedReleaser
{
public:
    using ReleaseFuncType = std::function<void()>;
    ScopedReleaser(ReleaseFuncType&& func) : func_(func) {}
    ~ScopedReleaser() { func_(); }

private:
    const ReleaseFuncType func_;
};


// Timer
class ScopedTimer
{
public:
    using microseconds = std::chrono::microseconds;
    using TimerFuncType = std::function<void(microseconds)>;
    ScopedTimer(TimerFuncType&& func);
    ~ScopedTimer();

private:
    const TimerFuncType func_;
    const std::chrono::time_point<std::chrono::steady_clock> start_;
};


#ifdef UWC_DEBUG_ON
#define UWC_FUNCTION_SCOPE_TIMER \
    ScopedTimer _timer_##__COUNTER__([](std::chrono::microseconds us) \
    { \
        Debug::Log(__FUNCTION__, "@", __FILE__, ":", __LINE__, " => ", us.count(), " [us]"); \
    });
#define UWC_SCOPE_TIMER(Name) \
    ScopedTimer _timer_##__COUNTER__([](std::chrono::microseconds us) \
    { \
        Debug::Log(#Name, " => ", us.count(), " [us]"); \
    });
#else
#define UWC_FUNCTION_SCOPE_TIMER
#define UWC_SCOPE_TIMER(Name)
#endif
