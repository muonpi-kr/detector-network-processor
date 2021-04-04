#include "application.h"

#include "defaults.h"

#include "utility/log.h"
#include "utility/restservice.h"

#include "analysis/coincidencefilter.h"
#include "supervision/station.h"
#include "utility/triggerhandler.h"

#include "link/mqtt.h"

#include "source/mqtt.h"

#include "sink/ascii.h"
#include "sink/database.h"
#include "sink/mqtt.h"

#include <memory>

namespace muonpi {

std::function<void(int)> application::s_shutdown_handler;

void wrapper_signal_handler(int signal)
{
    application::s_shutdown_handler(signal);
}


auto application::setup(std::vector<std::string> arguments) -> bool
{
    log::manager::singleton()->add_sink(std::make_shared<log::syslog_sink>());

    if (!m_parameters.start(std::move(arguments))) {
        return false;
    }
    if (m_parameters["d"]) {
        log::manager::singleton()->add_sink(std::make_shared<log::stream_sink>(std::cerr));
    }
    log::info() << "muondetector-cluster " + Version::string();
    if (m_parameters["l"]) {
        Config::files.credentials = m_parameters["l"].value;
    }
    if (m_parameters["c"]) {
        Config::files.config = m_parameters["c"].value;
    }

    if (m_parameters["s"]) {
        if (!m_parameters["l"]) {
            std::cout << "No credentials location given, using default.\n";
        }

        configuration credents { credentials(m_parameters["s"].value) };

        if (!credents.read()) {
            std::cout << "Could not read input file.\n";
            return false;
        }

        credents.set_encrypted(true);
        credents.set_filename(Config::files.credentials);

        if (credents.write()) {
            std::cout << "Wrote credentials file.\n";
            return false;
        }
        std::cout << "Could not write credentials file.\n";
        return false;
    }

    configuration cfg { config(Config::files.config) };

    if (!m_parameters["l"]) {
        cfg << configuration::definition { "credentials_file", &Config::files.credentials };
    }
    if (!cfg.read()) {
        log::error() << "Could not read configuration file.\n";
        return false;
    }

    configuration credents { credentials(Config::files.credentials, true) };

    if (!credents.read()) {
        log::error() << "Could not read credentials file.\n";
        return false;
    }

    return true;
}

auto application::run() -> int
{
    link::mqtt source_mqtt_link { Config::source_mqtt };
    if (!source_mqtt_link.wait_for(link::mqtt::Status::Connected)) {
        return -1;
    }

    link::mqtt sink_mqtt_link { Config::sink_mqtt };
    if (!sink_mqtt_link.wait_for(link::mqtt::Status::Connected)) {
        return -1;
    }

    sink::collection<event_t> collection_event_sink {};
    sink::collection<cluster_log_t> collection_clusterlog_sink {};
    sink::collection<detector_summary_t> collection_detectorsummary_sink {};
    sink::collection<trigger::detector> collection_trigger_sink {};
    sink::collection<detector_log_t> collection_detectorlog_sink {};

    if (m_parameters["d"]) {
        m_ascii_event_sink = std::make_unique<sink::ascii<event_t>> ( std::cout );
        m_ascii_clusterlog_sink = std::make_unique<sink::ascii<cluster_log_t>> ( std::cout );
        m_ascii_detectorsummary_sink = std::make_unique<sink::ascii<detector_summary_t>> ( std::cout );

        collection_event_sink.emplace(*m_ascii_event_sink);
        collection_clusterlog_sink.emplace(*m_ascii_clusterlog_sink);
        collection_detectorsummary_sink.emplace(*m_ascii_detectorsummary_sink);
    } else {
        m_trigger_sink = std::make_unique<sink::mqtt<trigger::detector>> ( sink_mqtt_link.publish("muonpi/trigger") );
        collection_trigger_sink.emplace(*m_trigger_sink);

        if (!Config::meta.local_cluster) {
            m_db_link = std::make_unique<link::database> ( Config::influx );

            m_event_sink = std::make_unique<sink::database<event_t>> ( *m_db_link );
            m_clusterlog_sink = std::make_unique<sink::database<cluster_log_t>> ( *m_db_link );
            m_detectorsummary_sink = std::make_unique<sink::database<detector_summary_t>> ( *m_db_link );

            m_broadcast_event_sink = std::make_unique<sink::mqtt<event_t>> ( sink_mqtt_link.publish("muonpi/events") );

            m_detectorlog_sink = std::make_unique<sink::database<detector_log_t>> ( *m_db_link );

            collection_event_sink.emplace(*m_broadcast_event_sink);

        } else {
            m_event_sink = std::make_unique<sink::mqtt<event_t>> ( sink_mqtt_link.publish("muonpi/l1data"), true );
            m_clusterlog_sink = std::make_unique<sink::mqtt<cluster_log_t>> ( sink_mqtt_link.publish("muonpi/cluster") );
            m_detectorsummary_sink = std::make_unique<sink::mqtt<detector_summary_t>> ( sink_mqtt_link.publish("muonpi/cluster") );
            m_detectorlog_sink = std::make_unique<sink::mqtt<detector_log_t>> ( sink_mqtt_link.publish("muonpi/log/") );
        }
        collection_event_sink.emplace(*m_event_sink);
        collection_clusterlog_sink.emplace(*m_clusterlog_sink);
        collection_detectorsummary_sink.emplace(*m_detectorsummary_sink);
        collection_detectorlog_sink.emplace(*m_detectorlog_sink);
    }

    m_supervisor = std::make_unique<supervision::state> ( collection_clusterlog_sink );
    coincidence_filter coincidencefilter { collection_event_sink, *m_supervisor };
    supervision::timebase timebasesupervisor { coincidencefilter, coincidencefilter };
    supervision::station stationsupervisor { collection_detectorsummary_sink, collection_trigger_sink, timebasesupervisor, timebasesupervisor, *m_supervisor };

    trigger_handler triggerhandler { stationsupervisor, Config::ldap, Config::trigger };
    rest::service rest_service { Config::rest };
    rest_service.add_handler(triggerhandler);

    source::mqtt<event_t> event_source { stationsupervisor, source_mqtt_link.subscribe("muonpi/data/#") };
    source::mqtt<event_t> l1_source { stationsupervisor, source_mqtt_link.subscribe("muonpi/l1data/#") };
    source::mqtt<detector_info_t<location_t>> detector_location_source { stationsupervisor, source_mqtt_link.subscribe("muonpi/log/#") };

    source::mqtt<detector_log_t> detectorlog_source { collection_detectorlog_sink, source_mqtt_link.subscribe("muonpi/log/#") };

    m_supervisor->add_thread(rest_service);
    m_supervisor->add_thread(stationsupervisor);
    m_supervisor->add_thread(coincidencefilter);
    m_supervisor->add_thread(sink_mqtt_link);
    m_supervisor->add_thread(source_mqtt_link);
    m_supervisor->add_thread(collection_event_sink);
    m_supervisor->add_thread(collection_detectorsummary_sink);
    m_supervisor->add_thread(collection_clusterlog_sink);
    m_supervisor->add_thread(collection_trigger_sink);
    m_supervisor->add_thread(collection_detectorlog_sink);

    s_shutdown_handler = [this](int signal) { signal_handler(signal); };

    std::signal(SIGINT, wrapper_signal_handler);
    std::signal(SIGTERM, wrapper_signal_handler);
    std::signal(SIGHUP, wrapper_signal_handler);

    coincidencefilter.start_synchronuos();

    return coincidencefilter.wait();
}

void application::signal_handler(int signal)
{
    if ((signal == SIGINT) || (signal == SIGTERM) || (signal == SIGHUP)) {
        log::notice() << "Received signal: " + std::to_string(signal) + ". Exiting.";
        m_supervisor->stop();
    }
}

auto application::credentials(std::string filename, bool encrypted) -> configuration
{
    configuration credentials { std::move(filename), encrypted };
    credentials
        << configuration::definition { "source_mqtt_user", &Config::source_mqtt.login.username }
        << configuration::definition { "source_mqtt_password", &Config::source_mqtt.login.password }
        << configuration::definition { "source_mqtt_station_id", &Config::source_mqtt.login.station_id }

        << configuration::definition { "sink_mqtt_user", &Config::sink_mqtt.login.username }
        << configuration::definition { "sink_mqtt_password", &Config::sink_mqtt.login.password }
        << configuration::definition { "sink_mqtt_station_id", &Config::sink_mqtt.login.station_id }

        << configuration::definition { "influx_user", &Config::influx.login.username }
        << configuration::definition { "influx_password", &Config::influx.login.password }
        << configuration::definition { "influx_database", &Config::influx.database }

        << configuration::definition { "ldap_bind_dn", &Config::ldap.login.bind_dn }
        << configuration::definition { "ldap_password", &Config::ldap.login.password };

    return credentials;
}

auto application::config(std::string filename) -> configuration
{
    configuration cfg { std::move(filename) };
    cfg
        << configuration::definition { "source_mqtt_host", &Config::source_mqtt.host }
        << configuration::definition { "source_mqtt_port", &Config::source_mqtt.port }

        << configuration::definition { "sink_mqtt_host", &Config::sink_mqtt.host }
        << configuration::definition { "sink_mqtt_port", &Config::sink_mqtt.port }

        << configuration::definition { "influx_host", &Config::influx.host }
        << configuration::definition { "influx_cluster_id", &Config::influx.cluster_id }

        << configuration::definition { "ldap_host", &Config::ldap.server }

        << configuration::definition { "rest_port", &Config::rest.port }
        << configuration::definition { "rest_bind_address", &Config::rest.address }
        << configuration::definition { "rest_trigger_file", &Config::trigger.save_file }
        << configuration::definition { "rest_cert", &Config::rest.cert }
        << configuration::definition { "rest_privkey", &Config::rest.privkey }
        << configuration::definition { "rest_fullchain", &Config::rest.fullchain }

        << configuration::definition { "run_local_cluster", &Config::meta.local_cluster }
        << configuration::definition { "max_geohash_length", &Config::meta.max_geohash_length };

    return cfg;
}

auto application::parameter() -> parameters
{
    parameters params { "muondetector-custer", "Calculate coincidences for the muonpi network" };

    params
        << parameters::definition { "c", "config", "Specify a configuration file to use", true }
        << parameters::definition { "l", "credentials", "Specify a credentials file to use", true }
        << parameters::definition { "s", "setup", "Setup the Credentials file from a plaintext file given with this option. The file will be written to the location given in the -l parameter in an encrypted format.", true }
        << parameters::definition { "d", "debug", "Additionally to the normal sinks use ascii sinks for debugging. Also enables the log output to stderr." };

    return params;
}

}
