
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

#include <librdkafka/rdkafka.h>

#include "trace_config.h"

namespace trace {

class KafkaTraceProducer {
 public:
  explicit KafkaTraceProducer(const TraceConfig& config);
  ~KafkaTraceProducer();

  void sendSync(const std::string& key, const std::string& value);
  void flush(int timeout_ms = 30000);
  void close();

  long long sentCount() const;
  long long failedCount() const;

 private:
  struct DeliveryContext {
    std::mutex mutex;
    std::condition_variable cv;
    bool completed = false;
    bool success = false;
    std::string error_message;
  };

  static void deliveryReportCallback(
      rd_kafka_t* producer,
      const rd_kafka_message_t* message,
      void* opaque);

  static void logCallback(
      const rd_kafka_t* producer,
      int level,
      const char* facility,
      const char* message);

  static void setConfOrThrow(
      rd_kafka_conf_t* conf,
      const char* name,
      const std::string& value);

  static void setConfOrThrow(
      rd_kafka_conf_t* conf,
      const char* name,
      bool value);

  static void setConfOrThrow(
      rd_kafka_conf_t* conf,
      const char* name,
      int value);

  TraceConfig config_;
  std::string topic_;
  rd_kafka_t* producer_ = nullptr;
  std::atomic<long long> sent_count_{0};
  std::atomic<long long> failed_count_{0};
  bool closed_ = false;
};

}  // namespace trace
  