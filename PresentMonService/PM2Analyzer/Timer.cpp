#include "Timer.h"

#include <iomanip>
#include <iostream>
#include <sstream>

Utilities::Timer::Timer(const std::string& name)
    : name(name), duration(0), lap(0) {
  startPoint = std::chrono::high_resolution_clock::now();
  lastPoint = startPoint;
}

Utilities::Timer::~Timer() { Print(); }

// Return duration in ms
double Utilities::Timer::getDuration() {
  std::chrono::high_resolution_clock::time_point currentPoint =
      std::chrono::high_resolution_clock::now();
  duration = currentPoint - startPoint;
  return duration.count();
}

// Return laptime in ms
double Utilities::Timer::getLap() {
  std::chrono::high_resolution_clock::time_point currentPoint =
      std::chrono::high_resolution_clock::now();
  lap = currentPoint - lastPoint;
  lastPoint = currentPoint;
  return lap.count();
}

std::string Utilities::Timer::Print() {
  std::ostringstream oss;
  oss << "Timer ";
  oss << name << "->  Lap: ";
  oss << std::setw(10);
  oss << std::fixed;
  oss << std::setprecision(2);
  oss << getLap();

  oss << " Duration: ";
  oss << std::setw(10);
  oss << std::fixed;
  oss << std::setprecision(2);
  oss << getDuration();

  return oss.str();
}
