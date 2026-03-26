
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

#include <librdkafka/rdkafka.h>

namespace {

std::atomic<bool> g_stop_requested{false};

void handleSignal(int) {
  g_stop_requested.store(true);
}

void setConfOrExit(rd_kafka_conf_t* conf, const char* name, const std::string& value) {
  char error_buffer[512] = {0};
  const rd_kafka_conf_res_t result =
      rd_kafka_conf_set(conf, name, value.c_str(), error_buffer, sizeof(error_buffer));
  if (result != RD_KAFKA_CONF_OK) {
    std::cerr << "[ERROR] failed to set Kafka consumer config '" << name
              << "': " << error_buffer << std::endl;
    std::exit(1);
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  const std::string servers = (argc > 1) ? argv[1] : "47.129.128.147:9092";
  const std::string topic = (argc > 2) ? argv[2] : "trace-data";

  std::cout << "=== Native Kafka Trace Consumer ===" << std::endl;
  std::cout << "  servers: " << servers << std::endl;
  std::cout << "  topic:   " << topic << std::endl;

  char error_buffer[512] = {0};
  rd_kafka_conf_t* conf = rd_kafka_conf_new();
  setConfOrExit(conf, "bootstrap.servers", servers);
  setConfOrExit(conf, "group.id", "trace-consumer-group");
  setConfOrExit(conf, "auto.offset.reset", "earliest");
  setConfOrExit(conf, "enable.auto.commit", "true");

  rd_kafka_t* consumer = rd_kafka_new(RD_KAFKA_CONSUMER, conf, error_buffer, sizeof(error_buffer));
  if (consumer == nullptr) {
    std::cerr << "[ERROR] failed to create consumer: " << error_buffer << std::endl;
    return 1;
  }

  rd_kafka_poll_set_consumer(consumer);

  rd_kafka_topic_partition_list_t* subscription = rd_kafka_topic_partition_list_new(1);
  rd_kafka_topic_partition_list_add(subscription, topic.c_str(), RD_KAFKA_PARTITION_UA);

  const rd_kafka_resp_err_t subscribe_error = rd_kafka_subscribe(consumer, subscription);
  rd_kafka_topic_partition_list_destroy(subscription);
  if (subscribe_error != RD_KAFKA_RESP_ERR_NO_ERROR) {
    std::cerr << "[ERROR] subscribe failed: " << rd_kafka_err2str(subscribe_error) << std::endl;
    rd_kafka_destroy(consumer);
    return 1;
  }

  std::cout << "start consuming... (Ctrl+C to exit)" << std::endl;

  long long batch_count = 0;
  const auto start = std::chrono::steady_clock::now();

  while (!g_stop_requested.load()) {
    rd_kafka_message_t* message = rd_kafka_consumer_poll(consumer, 1000);
    if (message == nullptr) {
      continue;
    }

    if (message->err) {
      if (message->err != RD_KAFKA_RESP_ERR__PARTITION_EOF &&
          message->err != RD_KAFKA_RESP_ERR__TIMED_OUT) {
        std::cerr << "[Consumer] error: " << rd_kafka_message_errstr(message) << std::endl;
      }
      rd_kafka_message_destroy(message);
      continue;
    }

    batch_count += 1;
    if (batch_count % 1000 == 0) {
      const auto now = std::chrono::steady_clock::now();
      const auto elapsed_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
      const double batches_per_second =
          static_cast<double>(batch_count) * 1000.0 /
          static_cast<double>(std::max<long long>(elapsed_ms, 1));

      std::cout << "[Consumer] received " << batch_count
                << " batches (" << static_cast<long long>(batches_per_second) << " batches/s)"
                << " partition=" << message->partition
                << " offset=" << message->offset
                << std::endl;
    }

    rd_kafka_message_destroy(message);
  }

  const auto end = std::chrono::steady_clock::now();
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::cout << std::endl;
  std::cout << "=== Consumer Summary ===" << std::endl;
  std::cout << "batches consumed: " << batch_count << std::endl;
  std::cout << "elapsed: " << (static_cast<double>(elapsed_ms) / 1000.0) << " s" << std::endl;

  rd_kafka_consumer_close(consumer);
  rd_kafka_destroy(consumer);
  return 0;
}
  