#pragma once

#include <string>
#include <chrono>

namespace Utilities {

    class Timer {
    public:

        Timer(const std::string& name = "Initial");
        ~Timer();

        void Lap();
        double getDuration();
        double getLap();
        std::string Print();

    private:
        std::string name;
        std::chrono::high_resolution_clock::time_point  startPoint;
        std::chrono::high_resolution_clock::time_point  lastPoint;
        std::chrono::duration<double, std::milli> duration;
        std::chrono::duration<double, std::milli> lap;
    };
};


