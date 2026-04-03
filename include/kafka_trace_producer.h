#pragma once

#include <stddef.h>

#include <librdkafka/rdkafka.h>

#include "trace_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TraceKafkaProducer {
  TraceConfig config;
  char topic[128];
  rd_kafka_t* producer;
  long long sent_count;
  long long failed_count;
  int closed;
} TraceKafkaProducer;

int trace_kafka_producer_init(
    TraceKafkaProducer* producer,
    const TraceConfig* config,
    char* error_buffer,
    size_t error_buffer_size);

int trace_kafka_producer_send_sync(
    TraceKafkaProducer* producer,
    const char* key,
    const char* value,
    char* error_buffer,
    size_t error_buffer_size);

void trace_kafka_producer_flush(TraceKafkaProducer* producer, int timeout_ms);
void trace_kafka_producer_close(TraceKafkaProducer* producer);

long long trace_kafka_producer_sent_count(const TraceKafkaProducer* producer);
long long trace_kafka_producer_failed_count(const TraceKafkaProducer* producer);

#ifdef __cplusplus
}
#endif
