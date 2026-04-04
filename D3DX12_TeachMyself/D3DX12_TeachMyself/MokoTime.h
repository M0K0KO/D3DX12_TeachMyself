#pragma once
#include <chrono>

class MokoTime
{
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
public:
    static void Tick()
    {
        auto now = Clock::now();
        s_deltaTime = std::chrono::duration<float>(now - s_lastTime).count();
        s_lastTime = now;
    }

    static TimePoint Now()
    {
        return Clock::now();
    }

    static float GetDeltaTime()
    {
        return s_deltaTime;
    }

    static float ElapsedMS(TimePoint start, TimePoint end)
    {
        return std::chrono::duration<float, std::milli>(end - start).count();
    }

    static float ElapsedMS(TimePoint start)
    {
        return ElapsedMS(start, Clock::now());
    }

private:
    inline static Clock::time_point s_lastTime = Clock::now();
    inline static float s_deltaTime = 0.0f;
};