
#include "trace_config.h"

#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "properties.h"

namespace trace {

namespace {

int parseInt(const PropertyMap& properties, const std::string& key, int default_value) {
  const std::string raw = getProperty(properties, key, std::to_string(default_value));
  try {
    return std::stoi(raw);
  } catch (const std::exception&) {
    std::cerr << "[WARN] invalid integer for " << key << ": " << raw
              << ", using default " << default_value << std::endl;
    return default_value;
  }
}

std::int64_t parseInt64(
    const PropertyMap& properties,
    const std::string& key,
    std::int64_t default_value) {
  const std::string raw = getProperty(properties, key, std::to_string(default_value));
  try {
    return std::stoll(raw);
  } catch (const std::exception&) {
    std::cerr << "[WARN] invalid integer for " << key << ": " << raw
              << ", using default " << default_value << std::endl;
    return default_value;
  }
}

bool parseBool(const PropertyMap& properties, const std::string& key, bool default_value) {
  const std::string raw = getProperty(properties, key, default_value ? "true" : "false");
  if (raw == "true" || raw == "1" || raw == "yes" || raw == "on") {
    return true;
  }
  if (raw == "false" || raw == "0" || raw == "no" || raw == "off") {
    return false;
  }

  std::cerr << "[WARN] invalid boolean for " << key << ": " << raw
            << ", using default " << (default_value ? "true" : "false") << std::endl;
  return default_value;
}

}  // namespace

TraceConfig TraceConfig::load(const std::string& path) {
  TraceConfig config;
  const PropertyMap properties = loadProperties(path);

  config.bootstrap_servers =
      getProperty(properties, "kafka.bootstrap.servers", config.bootstrap_servers);
  config.topic = getProperty(properties, "kafka.topic", config.topic);
  config.period_ms = parseInt(properties, "trace.period.ms", config.period_ms);
  config.batch_size = parseInt(properties, "trace.batch.size", config.batch_size);
  config.max_frames = parseInt64(properties, "trace.max.frames", config.max_frames);
  config.send_interval_ms =
      parseInt64(properties, "trace.send.interval.ms", config.send_interval_ms);
  config.task_id = getProperty(properties, "trace.task.id", config.task_id);
  config.acks = getProperty(properties, "kafka.acks", config.acks);
  config.retries = parseInt(properties, "kafka.retries", config.retries);
  config.enable_idempotence =
      parseBool(properties, "kafka.enable.idempotence", config.enable_idempotence);
  config.compression_type =
      getProperty(properties, "kafka.compression.type", config.compression_type);
  config.batch_size_bytes =
      parseInt(properties, "kafka.batch.size.bytes", config.batch_size_bytes);
  config.linger_ms = parseInt(properties, "kafka.linger.ms", config.linger_ms);
  config.buffer_memory =
      parseInt(properties, "kafka.buffer.memory", config.buffer_memory);
  config.request_timeout_ms =
      parseInt(properties, "kafka.request.timeout.ms", config.request_timeout_ms);
  config.message_timeout_ms =
      parseInt(properties, "kafka.message.timeout.ms", config.message_timeout_ms);

  return config;
}

std::int64_t TraceConfig::estimateBatches() const {
  if (batch_size <= 0) {
    return 0;
  }
  return (max_frames + batch_size - 1) / batch_size;
}

void TraceConfig::dump() const {
  const std::int64_t total_batches = estimateBatches();
  const double estimated_seconds =
      static_cast<double>(total_batches * send_interval_ms) / 1000.0;

  std::cout << "=== Trace Config ===" << std::endl;
  std::cout << "  kafka.bootstrap.servers = " << bootstrap_servers << std::endl;
  std::cout << "  kafka.topic             = " << topic << std::endl;
  std::cout << "  kafka.acks              = " << acks << std::endl;
  std::cout << "  kafka.idempotence       = " << (enable_idempotence ? "true" : "false")
            << std::endl;
  std::cout << "  kafka.compression.type  = " << compression_type << std::endl;
  std::cout << "  trace.max.frames        = " << max_frames << std::endl;
  std::cout << "  trace.batch.size        = " << batch_size << std::endl;
  std::cout << "  trace.period.ms         = " << period_ms << std::endl;
  std::cout << "  trace.send.interval.ms  = " << send_interval_ms << std::endl;
  std::cout << "  ---- estimate ----" << std::endl;
  std::cout << "  total batches          = " << total_batches << std::endl;
  std::cout << std::fixed << std::setprecision(1)
            << "  estimated time         = " << estimated_seconds << " s ("
            << (estimated_seconds / 60.0) << " min)" << std::endl;
  std::cout << "====================" << std::endl;
}

}  // namespace trace
  