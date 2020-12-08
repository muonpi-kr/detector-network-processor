#include "mqtteventsource.h"

#include "event.h"

namespace MuonPi {

MqttEventSource::MqttEventSource(std::shared_ptr<MqttLink::Subscriber> subscriber)
    : m_link { subscriber }
{

}

MqttEventSource::~MqttEventSource() = default;

auto MqttEventSource::step() -> int
{
    return {};
}

}
