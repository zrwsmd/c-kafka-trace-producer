
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "kafka_trace_producer.h"
#include "trace_config.h"

namespace trace {

class TraceSimulator {
 public:
  TraceSimulator(const TraceConfig& config, KafkaTraceProducer& producer);
  int run(const std::atomic<bool>& stop_requested);

 private:
  std::string buildPayload(std::int64_t seq, std::int64_t& ts_counter, int& frame_count) const;
  static std::string jsonEscape(const std::string& input);
  static std::string currentTimestamp();
  static double noise(double scale);
  static double round3(double value);

  double simulateAxis1Position(std::int64_t ts, int period_ms) const;
  double simulateAxis1Velocity(std::int64_t ts, int period_ms) const;
  double simulateAxis1Torque(std::int64_t ts, int period_ms) const;
  double simulateAxis2Position(std::int64_t ts, int period_ms) const;
  double simulateAxis2Velocity(std::int64_t ts, int period_ms) const;
  double simulateMotorRpm(std::int64_t ts, int period_ms) const;
  double simulateMotorTemp(std::int64_t ts, int period_ms) const;
  double simulateServoCurrent(std::int64_t ts, int period_ms) const;
  double simulateServoVoltage(std::int64_t ts, int period_ms) const;
  double simulatePressure(std::int64_t ts, int period_ms) const;

  TraceConfig config_;
  KafkaTraceProducer& producer_;
};

}  // namespace trace
  