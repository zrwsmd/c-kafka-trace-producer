
#include "kafka_trace_producer.h"

#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace trace {

KafkaTraceProducer::KafkaTraceProducer(const TraceConfig& config)
    : config_(config), topic_(config.topic) {
  char error_buffer[512] = {0};
  rd_kafka_conf_t* conf = rd_kafka_conf_new();

  std::cout << "[KafkaProducer] creating Kafka config..." << std::endl;

  rd_kafka_conf_set_dr_msg_cb(conf, &KafkaTraceProducer::deliveryReportCallback);
  rd_kafka_conf_set_log_cb(conf, &KafkaTraceProducer::logCallback);

  setConfOrThrow(conf, "bootstrap.servers", config.bootstrap_servers);
  setConfOrThrow(conf, "request.required.acks", config.acks);
  setConfOrThrow(conf, "enable.idempotence", config.enable_idempotence);
  setConfOrThrow(conf, "retries", config.retries);
  setConfOrThrow(conf, "max.in.flight.requests.per.connection", 1);
  setConfOrThrow(conf, "batch.size", config.batch_size_bytes);
  setConfOrThrow(conf, "linger.ms", config.linger_ms);
  setConfOrThrow(conf, "queue.buffering.max.kbytes", config.buffer_memory / 1024);
  setConfOrThrow(conf, "compression.type", config.compression_type);
  setConfOrThrow(conf, "request.timeout.ms", config.request_timeout_ms);
  setConfOrThrow(conf, "message.timeout.ms", config.message_timeout_ms);
  setConfOrThrow(conf, "socket.keepalive.enable", true);
  setConfOrThrow(conf, "client.id", std::string("c-kafka-trace-producer"));

  std::cout << "[KafkaProducer] calling rd_kafka_new..." << std::endl;
  producer_ = rd_kafka_new(RD_KAFKA_PRODUCER, conf, error_buffer, sizeof(error_buffer));
  std::cout << "[KafkaProducer] rd_kafka_new returned" << std::endl;
  if (producer_ == nullptr) {
    rd_kafka_conf_destroy(conf);
    throw std::runtime_error(std::string("failed to create Kafka producer: ") + error_buffer);
  }

  std::cout << "[KafkaProducer] initialized: servers=" << config.bootstrap_servers
            << " topic=" << topic_
            << " acks=" << config.acks
            << " idempotence=" << (config.enable_idempotence ? "true" : "false")
            << " compression=" << config.compression_type
            << std::endl;
}

KafkaTraceProducer::~KafkaTraceProducer() {
  try {
    close();
  } catch (const std::exception&) {
  }
}

void KafkaTraceProducer::sendSync(const std::string& key, const std::string& value) {
  if (producer_ == nullptr || closed_) {
    throw std::runtime_error("producer is closed");
  }

  DeliveryContext context;
  void* key_data = key.empty() ? nullptr : const_cast<char*>(key.data());
  void* value_data = value.empty() ? nullptr : const_cast<char*>(value.data());
  const rd_kafka_resp_err_t error = rd_kafka_producev(
      producer_,
      RD_KAFKA_V_TOPIC(topic_.c_str()),
      RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
      RD_KAFKA_V_KEY(key_data, key.size()),
      RD_KAFKA_V_VALUE(value_data, value.size()),
      RD_KAFKA_V_OPAQUE(&context),
      RD_KAFKA_V_END);

  if (error != RD_KAFKA_RESP_ERR_NO_ERROR) {
    failed_count_.fetch_add(1);
    throw std::runtime_error(std::string("produce failed immediately: ") + rd_kafka_err2str(error));
  }

  std::unique_lock<std::mutex> lock(context.mutex);
  while (!context.completed) {
    lock.unlock();
    rd_kafka_poll(producer_, 100);
    lock.lock();
  }

  if (!context.success) {
    failed_count_.fetch_add(1);
    throw std::runtime_error(
        std::string("delivery failed: ") +
        (context.error_message.empty() ? "unknown error" : context.error_message));
  }

  sent_count_.fetch_add(1);
}

void KafkaTraceProducer::flush(int timeout_ms) {
  if (producer_ == nullptr) {
    return;
  }
  rd_kafka_flush(producer_, timeout_ms);
}

void KafkaTraceProducer::close() {
  if (closed_) {
    return;
  }

  closed_ = true;
  if (producer_ != nullptr) {
    rd_kafka_flush(producer_, config_.message_timeout_ms);
    rd_kafka_destroy(producer_);
    producer_ = nullptr;
  }

  std::cout << "[KafkaProducer] closed. sent=" << sent_count_.load()
            << " failed=" << failed_count_.load() << std::endl;
}

long long KafkaTraceProducer::sentCount() const {
  return sent_count_.load();
}

long long KafkaTraceProducer::failedCount() const {
  return failed_count_.load();
}

void KafkaTraceProducer::deliveryReportCallback(
    rd_kafka_t*,
    const rd_kafka_message_t* message,
    void*) {
  auto* context = static_cast<DeliveryContext*>(message->_private);
  if (context == nullptr) {
    return;
  }

  {
    std::lock_guard<std::mutex> guard(context->mutex);
    context->completed = true;
    context->success = (message->err == RD_KAFKA_RESP_ERR_NO_ERROR);
    if (!context->success) {
      context->error_message = rd_kafka_message_errstr(message);
    }
  }

  context->cv.notify_one();
}

void KafkaTraceProducer::logCallback(
    const rd_kafka_t*,
    int level,
    const char* facility,
    const char* message) {
  if (level <= 3) {
    std::fprintf(stderr, "[librdkafka][%s] %s\n", facility, message);
    std::fflush(stderr);
  }
}

void KafkaTraceProducer::setConfOrThrow(
    rd_kafka_conf_t* conf,
    const char* name,
    const std::string& value) {
  char error_buffer[512] = {0};
  const rd_kafka_conf_res_t result = rd_kafka_conf_set(
      conf,
      name,
      value.c_str(),
      error_buffer,
      sizeof(error_buffer));

  if (result != RD_KAFKA_CONF_OK) {
    throw std::runtime_error(
        std::string("failed to set Kafka config '") + name + "': " + error_buffer);
  }
}

void KafkaTraceProducer::setConfOrThrow(
    rd_kafka_conf_t* conf,
    const char* name,
    bool value) {
  setConfOrThrow(conf, name, std::string(value ? "true" : "false"));
}

void KafkaTraceProducer::setConfOrThrow(
    rd_kafka_conf_t* conf,
    const char* name,
    int value) {
  setConfOrThrow(conf, name, std::to_string(value));
}

}  // namespace trace
  
