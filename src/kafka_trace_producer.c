#include "kafka_trace_producer.h"

#include <stdio.h>
#include <string.h>

typedef struct TraceDeliveryContext {
  int completed;
  int success;
  char error_message[256];
} TraceDeliveryContext;

static void trace_kafka_delivery_report_callback(
    rd_kafka_t* producer,
    const rd_kafka_message_t* message,
    void* opaque) {
  TraceDeliveryContext* context = (TraceDeliveryContext*)message->_private;
  (void)producer;
  (void)opaque;

  if (context == NULL) {
    return;
  }

  context->completed = 1;
  context->success = (message->err == RD_KAFKA_RESP_ERR_NO_ERROR);
  if (!context->success) {
    snprintf(context->error_message, sizeof(context->error_message), "%s",
             rd_kafka_message_errstr(message));
  }
}

static void trace_kafka_log_callback(
    const rd_kafka_t* producer,
    int level,
    const char* facility,
    const char* message) {
  (void)producer;
  if (level <= 3) {
    fprintf(stderr, "[librdkafka][%s] %s\n", facility, message);
    fflush(stderr);
  }
}

static int trace_kafka_set_conf_string(
    rd_kafka_conf_t* conf,
    const char* name,
    const char* value,
    char* error_buffer,
    size_t error_buffer_size) {
  rd_kafka_conf_res_t result =
      rd_kafka_conf_set(conf, name, value, error_buffer, error_buffer_size);
  return (result == RD_KAFKA_CONF_OK) ? 0 : -1;
}

static int trace_kafka_set_conf_int(
    rd_kafka_conf_t* conf,
    const char* name,
    int value,
    char* error_buffer,
    size_t error_buffer_size) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%d", value);
  return trace_kafka_set_conf_string(conf, name, buffer, error_buffer, error_buffer_size);
}

static int trace_kafka_set_conf_bool(
    rd_kafka_conf_t* conf,
    const char* name,
    int value,
    char* error_buffer,
    size_t error_buffer_size) {
  return trace_kafka_set_conf_string(
      conf, name, value ? "true" : "false", error_buffer, error_buffer_size);
}

int trace_kafka_producer_init(
    TraceKafkaProducer* producer,
    const TraceConfig* config,
    char* error_buffer,
    size_t error_buffer_size) {
  rd_kafka_conf_t* conf = rd_kafka_conf_new();

  memset(producer, 0, sizeof(*producer));
  producer->config = *config;
  snprintf(producer->topic, sizeof(producer->topic), "%s", config->topic);

  printf("[KafkaProducer] creating Kafka config...\n");

  rd_kafka_conf_set_dr_msg_cb(conf, trace_kafka_delivery_report_callback);
  rd_kafka_conf_set_log_cb(conf, trace_kafka_log_callback);

  if (trace_kafka_set_conf_string(
          conf, "bootstrap.servers", config->bootstrap_servers,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_string(
          conf, "request.required.acks", config->acks,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_bool(
          conf, "enable.idempotence", config->enable_idempotence,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_int(
          conf, "retries", config->retries,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_int(
          conf, "max.in.flight.requests.per.connection", 1,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_int(
          conf, "batch.size", config->batch_size_bytes,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_int(
          conf, "linger.ms", config->linger_ms,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_int(
          conf, "queue.buffering.max.kbytes", config->buffer_memory / 1024,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_string(
          conf, "compression.type", config->compression_type,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_int(
          conf, "request.timeout.ms", config->request_timeout_ms,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_int(
          conf, "message.timeout.ms", config->message_timeout_ms,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_bool(
          conf, "socket.keepalive.enable", 1,
          error_buffer, error_buffer_size) != 0 ||
      trace_kafka_set_conf_string(
          conf, "client.id", "c-kafka-trace-producer",
          error_buffer, error_buffer_size) != 0) {
    rd_kafka_conf_destroy(conf);
    return -1;
  }

  printf("[KafkaProducer] calling rd_kafka_new...\n");
  producer->producer =
      rd_kafka_new(RD_KAFKA_PRODUCER, conf, error_buffer, error_buffer_size);
  printf("[KafkaProducer] rd_kafka_new returned\n");

  if (producer->producer == NULL) {
    rd_kafka_conf_destroy(conf);
    return -1;
  }

  printf("[KafkaProducer] initialized: servers=%s topic=%s acks=%s idempotence=%s compression=%s\n",
         config->bootstrap_servers,
         producer->topic,
         config->acks,
         config->enable_idempotence ? "true" : "false",
         config->compression_type);

  return 0;
}

int trace_kafka_producer_send_sync(
    TraceKafkaProducer* producer,
    const char* key,
    const char* value,
    char* error_buffer,
    size_t error_buffer_size) {
  TraceDeliveryContext context;
  const void* key_data = (key != NULL && key[0] != '\0') ? key : NULL;
  size_t key_size = (key_data != NULL) ? strlen(key) : 0;
  const void* value_data = (value != NULL && value[0] != '\0') ? value : NULL;
  size_t value_size = (value_data != NULL) ? strlen(value) : 0;
  rd_kafka_resp_err_t error;

  if (producer->producer == NULL || producer->closed) {
    snprintf(error_buffer, error_buffer_size, "producer is closed");
    return -1;
  }

  memset(&context, 0, sizeof(context));

  error = rd_kafka_producev(
      producer->producer,
      RD_KAFKA_V_TOPIC(producer->topic),
      RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
      RD_KAFKA_V_KEY((void*)key_data, key_size),
      RD_KAFKA_V_VALUE((void*)value_data, value_size),
      RD_KAFKA_V_OPAQUE(&context),
      RD_KAFKA_V_END);

  if (error != RD_KAFKA_RESP_ERR_NO_ERROR) {
    producer->failed_count += 1;
    snprintf(error_buffer, error_buffer_size, "produce failed immediately: %s",
             rd_kafka_err2str(error));
    return -1;
  }

  while (!context.completed) {
    rd_kafka_poll(producer->producer, 100);
  }

  if (!context.success) {
    producer->failed_count += 1;
    snprintf(error_buffer, error_buffer_size, "delivery failed: %s",
             context.error_message[0] != '\0' ? context.error_message : "unknown error");
    return -1;
  }

  producer->sent_count += 1;
  return 0;
}

void trace_kafka_producer_flush(TraceKafkaProducer* producer, int timeout_ms) {
  if (producer->producer != NULL) {
    rd_kafka_flush(producer->producer, timeout_ms);
  }
}

void trace_kafka_producer_close(TraceKafkaProducer* producer) {
  if (producer->closed) {
    return;
  }

  producer->closed = 1;
  if (producer->producer != NULL) {
    rd_kafka_flush(producer->producer, producer->config.message_timeout_ms);
    rd_kafka_destroy(producer->producer);
    producer->producer = NULL;
  }

  printf("[KafkaProducer] closed. sent=%lld failed=%lld\n",
         producer->sent_count, producer->failed_count);
}

long long trace_kafka_producer_sent_count(const TraceKafkaProducer* producer) {
  return producer->sent_count;
}

long long trace_kafka_producer_failed_count(const TraceKafkaProducer* producer) {
  return producer->failed_count;
}
