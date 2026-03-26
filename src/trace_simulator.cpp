
#include "trace_simulator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

namespace trace {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::mt19937& rng() {
  static thread_local std::mt19937 generator(std::random_device{}());
  return generator;
}

}  // namespace

TraceSimulator::TraceSimulator(const TraceConfig& config, KafkaTraceProducer& producer)
    : config_(config), producer_(producer) {}

int TraceSimulator::run(const std::atomic<bool>& stop_requested) {
  const std::int64_t total_batches = config_.estimateBatches();
  const std::int64_t progress_interval = std::max<std::int64_t>(total_batches / 100, 1);

  std::cout << "[TraceSimulator] starting:"
            << " maxFrames=" << config_.max_frames
            << " batchSize=" << config_.batch_size
            << " totalBatches=" << total_batches
            << " sendInterval=" << config_.send_interval_ms << "ms"
            << " topic=" << config_.topic
            << std::endl;
  std::cout << "[TraceSimulator] send loop started: "
            << total_batches << " batches (" << config_.max_frames << " frames)"
            << std::endl;

  std::int64_t ts_counter = 0;
  std::int64_t frame_sent = 0;
  std::int64_t success_batches = 0;
  const auto start = std::chrono::steady_clock::now();

  for (std::int64_t seq = 1; seq <= total_batches; ++seq) {
    if (stop_requested.load()) {
      std::cout << "[TraceSimulator] stop requested, ending send loop" << std::endl;
      break;
    }

    int frame_count = 0;
    const std::string payload = buildPayload(seq, ts_counter, frame_count);
    frame_sent += frame_count;

    try {
      producer_.sendSync(config_.task_id, payload);
      success_batches += 1;
    } catch (const std::exception& error) {
      std::cerr << "[TraceSimulator] FATAL: seq=" << seq
                << " send failed: " << error.what() << std::endl;
      break;
    }

    if (seq % progress_interval == 0 || seq == total_batches) {
      const auto now = std::chrono::steady_clock::now();
      const auto elapsed_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
      const double speed =
          static_cast<double>(seq) * 1000.0 /
          static_cast<double>(std::max<std::int64_t>(elapsed_ms, 1));
      const std::int64_t eta_seconds =
          static_cast<std::int64_t>((total_batches - seq) / std::max(speed, 0.01));

      std::cout << "[" << currentTimestamp() << "] [TraceSimulator] progress: "
                << std::fixed << std::setprecision(1)
                << (static_cast<double>(seq) * 100.0 / static_cast<double>(total_batches))
                << "% (" << seq << "/" << total_batches
                << " batches, " << frame_sent << " frames)"
                << " speed=" << std::setprecision(0) << speed << " batches/s"
                << " elapsed=" << (elapsed_ms / 1000)
                << "s ETA=" << eta_seconds << "s"
                << std::endl;
    }

    if (seq < total_batches && config_.send_interval_ms > 0) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(config_.send_interval_ms));
    }
  }

  const auto end = std::chrono::steady_clock::now();
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  const double elapsed_seconds = static_cast<double>(elapsed_ms) / 1000.0;

  std::cout << std::endl;
  std::cout << "========== TraceSimulator Final Summary ==========" << std::endl;
  std::cout << "frames sent     " << frame_sent << " / " << config_.max_frames << std::endl;
  std::cout << "batches sent    " << success_batches << " / " << total_batches << std::endl;
  std::cout << "send failures   " << producer_.failedCount() << std::endl;
  std::cout << std::fixed << std::setprecision(1)
            << "elapsed         " << elapsed_seconds << " s (" << (elapsed_seconds / 60.0)
            << " min)" << std::endl;
  if (elapsed_ms > 0) {
    std::cout << std::setprecision(0)
              << "avg batch rate  "
              << (static_cast<double>(success_batches) * 1000.0 / static_cast<double>(elapsed_ms))
              << " batches/s" << std::endl;
    std::cout << "avg frame rate  "
              << (static_cast<double>(frame_sent) * 1000.0 / static_cast<double>(elapsed_ms))
              << " frames/s" << std::endl;
  }
  std::cout << "result          "
            << ((success_batches == total_batches) ? "all batches delivered" : "stopped before completion")
            << std::endl;
  std::cout << "==================================================" << std::endl;

  return (success_batches == total_batches) ? 0 : 1;
}

std::string TraceSimulator::buildPayload(
    std::int64_t seq,
    std::int64_t& ts_counter,
    int& frame_count) const {
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out << std::setprecision(3);
  out << "{";
  out << "\"taskId\":\"" << jsonEscape(config_.task_id) << "\",";
  out << "\"seq\":" << seq << ",";
  out << "\"period\":" << config_.period_ms << ",";
  out << "\"frames\":[";

  frame_count = 0;
  for (int index = 0; index < config_.batch_size; ++index) {
    const std::int64_t next_ts = ts_counter + 1;
    if (next_ts > config_.max_frames) {
      break;
    }
    ts_counter = next_ts;

    if (frame_count > 0) {
      out << ",";
    }

    out << "{";
    out << "\"ts\":" << ts_counter << ",";
    out << "\"axis1_position\":" << simulateAxis1Position(ts_counter, config_.period_ms) << ",";
    out << "\"axis1_velocity\":" << simulateAxis1Velocity(ts_counter, config_.period_ms) << ",";
    out << "\"axis1_torque\":" << simulateAxis1Torque(ts_counter, config_.period_ms) << ",";
    out << "\"axis2_position\":" << simulateAxis2Position(ts_counter, config_.period_ms) << ",";
    out << "\"axis2_velocity\":" << simulateAxis2Velocity(ts_counter, config_.period_ms) << ",";
    out << "\"motor_rpm\":" << simulateMotorRpm(ts_counter, config_.period_ms) << ",";
    out << "\"motor_temp\":" << simulateMotorTemp(ts_counter, config_.period_ms) << ",";
    out << "\"servo_current\":" << simulateServoCurrent(ts_counter, config_.period_ms) << ",";
    out << "\"servo_voltage\":" << simulateServoVoltage(ts_counter, config_.period_ms) << ",";
    out << "\"pressure_bar\":" << simulatePressure(ts_counter, config_.period_ms);
    out << "}";

    frame_count += 1;
  }

  out << "]}";
  return out.str();
}

std::string TraceSimulator::jsonEscape(const std::string& input) {
  std::ostringstream out;
  for (const char ch : input) {
    switch (ch) {
      case '\\':
        out << "\\\\";
        break;
      case '\"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << ch;
        break;
    }
  }
  return out.str();
}

std::string TraceSimulator::currentTimestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm timeinfo {};
#if defined(_WIN32)
  localtime_s(&timeinfo, &now);
#else
  localtime_r(&now, &timeinfo);
#endif
  char buffer[32] = {0};
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return std::string(buffer);
}

double TraceSimulator::noise(double scale) {
  std::uniform_real_distribution<double> distribution(-scale * 0.02, scale * 0.02);
  return distribution(rng());
}

double TraceSimulator::round3(double value) {
  return std::round(value * 1000.0) / 1000.0;
}

double TraceSimulator::simulateAxis1Position(std::int64_t ts, int period_ms) const {
  const double t = static_cast<double>(ts) * static_cast<double>(period_ms) / 1000.0;
  return round3(100.0 * std::sin(2.0 * kPi * 0.5 * t) + noise(0.5));
}

double TraceSimulator::simulateAxis1Velocity(std::int64_t ts, int period_ms) const {
  const double t = static_cast<double>(ts) * static_cast<double>(period_ms) / 1000.0;
  return round3(kPi * 100.0 * std::cos(2.0 * kPi * 0.5 * t) + noise(1.0));
}

double TraceSimulator::simulateAxis1Torque(std::int64_t ts, int period_ms) const {
  const double t = static_cast<double>(ts) * static_cast<double>(period_ms) / 1000.0;
  return round3(50.0 + 20.0 * std::sin(2.0 * kPi * 1.0 * t) + noise(0.5));
}

double TraceSimulator::simulateAxis2Position(std::int64_t ts, int period_ms) const {
  const double t = static_cast<double>(ts) * static_cast<double>(period_ms) / 1000.0;
  return round3(80.0 * std::sin(2.0 * kPi * 0.3 * t + 1.0) + noise(0.5));
}

double TraceSimulator::simulateAxis2Velocity(std::int64_t ts, int period_ms) const {
  const double t = static_cast<double>(ts) * static_cast<double>(period_ms) / 1000.0;
  return round3(80.0 * 0.3 * 2.0 * kPi * std::cos(2.0 * kPi * 0.3 * t + 1.0) + noise(0.5));
}

double TraceSimulator::simulateMotorRpm(std::int64_t ts, int period_ms) const {
  const double t = static_cast<double>(ts) * static_cast<double>(period_ms) / 1000.0;
  return round3(1500.0 + 300.0 * std::sin(2.0 * kPi * 0.1 * t) + noise(5.0));
}

double TraceSimulator::simulateMotorTemp(std::int64_t ts, int period_ms) const {
  const double t = static_cast<double>(ts) * static_cast<double>(period_ms) / 1000.0;
  return round3(65.0 + 10.0 * std::sin(2.0 * kPi * 0.02 * t) + noise(0.2));
}

double TraceSimulator::simulateServoCurrent(std::int64_t ts, int period_ms) const {
  const double t = static_cast<double>(ts) * static_cast<double>(period_ms) / 1000.0;
  return round3(5.0 + 2.0 * std::sin(2.0 * kPi * 0.8 * t) + noise(0.1));
}

double TraceSimulator::simulateServoVoltage(std::int64_t ts, int period_ms) const {
  const double t = static_cast<double>(ts) * static_cast<double>(period_ms) / 1000.0;
  return round3(48.0 + 0.5 * std::sin(2.0 * kPi * 0.05 * t) + noise(0.05));
}

double TraceSimulator::simulatePressure(std::int64_t ts, int period_ms) const {
  const double t = static_cast<double>(ts) * static_cast<double>(period_ms) / 1000.0;
  return round3(5.0 + 1.5 * std::sin(2.0 * kPi * 0.2 * t) + noise(0.05));
}

}  // namespace trace
  