﻿#ifndef STATION_COINCIDENCE_H
#define STATION_COINCIDENCE_H

#include "messages/event.h"
#include "messages/trigger.h"

#include <muonpi/threadrunner.h>
#include <muonpi/units.h>

#include <muonpi/sink/base.h>

#include <muonpi/analysis/histogram.h>
#include <muonpi/analysis/uppermatrix.h>

#include <string>

namespace muonpi {

namespace supervision {
    class station;
}

/**
 * @brief The station_coincidence class. It stores histograms between all possible detector pairs.
 */
class station_coincidence : public sink::base<event_t>, public sink::base<trigger::detector>, public thread_runner {
public:
    struct configuration {
        std::chrono::system_clock::duration histogram_sample_time {};
    };
    /**
     * @brief station_coincidence
     * @param data_directory The data directory to use to store the data
     * @param stationsupervisor reference to the supervision::station object
     */
    station_coincidence(std::string data_directory, supervision::station& stationsupervisor, configuration config);

    /**
     * @brief get Reimplemented from sink::base
     * @param event the event to process
     */
    void get(event_t event) override;

    /**
     * @brief get Reimplemented from sink::base
     * @param trig the trigger to process
     */
    void get(trigger::detector trig) override;

protected:
    [[nodiscard]] auto step() -> int override;

    [[nodiscard]] auto post_run() -> int override;

private:
    void save();
    void reset();
    void add_station(const userinfo_t& userinfo, const location_t& location);

    supervision::station& m_stationsupervisor;

    std::string m_data_directory {};

    constexpr static std::size_t s_bins { 2000 }; //<! total number of bins to use per pair
    constexpr static double s_total_width { 2.0 * 100000.0 };

    std::atomic<bool> m_saving { false };

    using histogram_t = histogram<std::int32_t, std::uint32_t>;
    struct data_t {
        std::size_t first {};
        std::size_t second {};
        float distance {};
        histogram_t hist { s_bins };
        std::uint8_t online { 2 };
        std::chrono::system_clock::time_point last_online { std::chrono::system_clock::now() };
        std::int32_t uptime { 0 };
    };
    std::vector<std::pair<userinfo_t, location_t>> m_stations {};
    upper_matrix<data_t> m_data {};
    std::chrono::system_clock::time_point m_last_save { std::chrono::system_clock::now() };

    configuration m_config {};
};

}

#endif // STATION_COINCIDENCE_H
