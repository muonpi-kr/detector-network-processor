#ifndef STATESUPERVISOR_H
#define STATESUPERVISOR_H

#include "threadrunner.h"
#include "detector.h"

#include <cinttypes>
#include <vector>
#include <map>
#include <chrono>

namespace MuonPi {


class StateSupervisor
{
public:
    void time_status(std::chrono::milliseconds timeout);
    void detector_status(std::size_t hash, Detector::Status status);

    [[nodiscard]] auto step() -> int;

private:
    std::map<std::size_t, Detector::Status> m_detectors;
    std::chrono::milliseconds m_timeout;
};

}

#endif // STATESUPERVISOR_H
