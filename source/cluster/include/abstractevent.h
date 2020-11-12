#ifndef ABSTRACTEVENT_H
#define ABSTRACTEVENT_H

#include <chrono>

namespace MuonPi {

/**
 * @brief The AbstractEvent class
 * Abstract class, which is used as an interface for single events and combined events
 */
class AbstractEvent
{
public:
    /**
     * @brief AbstractEvent
     * @param id The id of the event
     * @param time The time of the event
     */
    AbstractEvent(std::uint64_t id, std::chrono::steady_clock::time_point time) noexcept;

    virtual ~AbstractEvent() noexcept;
    /**
     * @brief time
     * @return The time when the event happened.
     */
    [[nodiscard]] auto time() const -> std::chrono::steady_clock::time_point;

    /**
     * @brief id
     * @return The id of this event
     */
    [[nodiscard]] auto id() const -> std::uint64_t;

private:
    std::uint64_t m_id {};
    std::chrono::steady_clock::time_point m_time {};
};
}

#endif // ABSTRACTEVENT_H
