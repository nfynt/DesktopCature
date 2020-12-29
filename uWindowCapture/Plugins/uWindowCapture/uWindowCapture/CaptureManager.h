#pragma once

#include <Windows.h>
#include <deque>
#include <mutex>

#include "WindowQueue.h"
#include "Thread.h"


enum class CapturePriority
{
    High = 0,
    Middle = 1,
    Low  = 2,
};


class CaptureManager
{
public:
    CaptureManager();
    ~CaptureManager();
    void RequestCapture(int id, CapturePriority priority);
    void RequestCaptureIcon(int id);

private:
    ThreadLoop windowCaptureThreadLoop_;
    ThreadLoop iconCaptureThreadLoop_;
    WindowQueue highPriorityQueue_;
    WindowQueue middlePriorityQueue_;
    WindowQueue lowPriorityQueue_;
    WindowQueue iconQueue_;
};