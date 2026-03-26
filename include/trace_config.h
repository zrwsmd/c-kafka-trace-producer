
#pragma once

#include <cstdint>
#include <string>

namespace trace {

struct TraceConfig {
  std::string bootstrap_servers = "47.129.128.147:9092";
  std::string topic = "trace-data";

  int period_ms = 1;
  int batch_size = 1000;
  std::int64_t max_frames = 1000000;
  std::int64_t send_interval_ms = 100;
  std::string task_id = "trace_001";

  std::string acks = "all";
  int retries = 5;
  bool enable_idempotence = true;
  std::string compression_type = "none";
  int batch_size_bytes = 65536;
  int linger_ms = 10;
  int buffer_memory = 67108864;
  int request_timeout_ms = 60000;
  int message_timeout_ms = 120000;

  static TraceConfig load(const std::string& path);
  std::int64_t estimateBatches() const;
  void dump() const;
};

}  // namespace trace
  