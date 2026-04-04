#pragma once
#include <chrono>

class MokoTime
{
public:
    static void Tick()
    {
        auto now = std::chrono::high_resolution_clock::now();
        s_deltaTime = std::chrono::duration<float>(now - s_lastTime).count();
        s_lastTime = now;
    }

    static float GetDeltaTime()
    {
        return s_deltaTime;
    }

private:
    inline static std::chrono::high_resolution_clock::time_point s_lastTime =
        std::chrono::high_resolution_clock::now();

    inline static float s_deltaTime = 0.0f;
};