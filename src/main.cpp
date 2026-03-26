
#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <string>

#include "kafka_trace_producer.h"
#include "trace_config.h"
#include "trace_simulator.h"

namespace {

std::atomic<bool> g_stop_requested{false};

void handleSignal(int) {
  g_stop_requested.store(true);
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  const std::string config_path = (argc > 1) ? argv[1] : "config/application.properties";

  std::cout << "=== Native Kafka Trace Producer ===" << std::endl;
  std::cout << std::endl;

  try {
    trace::TraceConfig config = trace::TraceConfig::load(config_path);
    config.dump();

    trace::KafkaTraceProducer producer(config);
    trace::TraceSimulator simulator(config, producer);

    std::cout << std::endl;
    std::cout << ">>> sending " << config.max_frames << " frames to Kafka ..." << std::endl;
    std::cout << std::endl;

    const int exit_code = simulator.run(g_stop_requested);
    producer.close();

    std::cout << std::endl;
    std::cout << "program exit" << std::endl;
    return exit_code;
  } catch (const std::exception& error) {
    std::cerr << "[ERROR] " << error.what() << std::endl;
    return 1;
  }
}
  