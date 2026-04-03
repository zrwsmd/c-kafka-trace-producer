#include "trace_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "properties.h"

static int trace_parse_int(
    const TracePropertyMap* properties,
    const char* key,
    int default_value) {
  const char* raw = trace_get_property(properties, key, "");
  char* end = NULL;
  long value;

  if (raw[0] == '\0') {
    return default_value;
  }

  value = strtol(raw, &end, 10);
  if (end == raw || *end != '\0') {
    fprintf(stderr, "[WARN] invalid integer for %s: %s, using default %d\n",
            key, raw, default_value);
    return default_value;
  }

  return (int)value;
}

static int64_t trace_parse_int64(
    const TracePropertyMap* properties,
    const char* key,
    int64_t default_value) {
  const char* raw = trace_get_property(properties, key, "");
  char* end = NULL;
  long long value;

  if (raw[0] == '\0') {
    return default_value;
  }

  value = strtoll(raw, &end, 10);
  if (end == raw || *end != '\0') {
    fprintf(stderr, "[WARN] invalid integer for %s: %s, using default %lld\n",
            key, raw, (long long)default_value);
    return default_value;
  }

  return (int64_t)value;
}

static int trace_parse_bool(
    const TracePropertyMap* properties,
    const char* key,
    int default_value) {
  const char* raw = trace_get_property(properties, key, "");

  if (raw[0] == '\0') {
    return default_value;
  }

  if (strcmp(raw, "true") == 0 || strcmp(raw, "1") == 0 ||
      strcmp(raw, "yes") == 0 || strcmp(raw, "on") == 0) {
    return 1;
  }

  if (strcmp(raw, "false") == 0 || strcmp(raw, "0") == 0 ||
      strcmp(raw, "no") == 0 || strcmp(raw, "off") == 0) {
    return 0;
  }

  fprintf(stderr, "[WARN] invalid boolean for %s: %s, using default %s\n",
          key, raw, default_value ? "true" : "false");
  return default_value;
}

void trace_config_init(TraceConfig* config) {
  memset(config, 0, sizeof(*config));

  snprintf(config->bootstrap_servers, sizeof(config->bootstrap_servers),
           "47.129.128.147:9092");
  snprintf(config->topic, sizeof(config->topic), "trace-data");
  config->period_ms = 1;
  config->batch_size = 1000;
  config->max_frames = 1000000;
  config->send_interval_ms = 100;
  snprintf(config->task_id, sizeof(config->task_id), "trace_001");
  snprintf(config->acks, sizeof(config->acks), "all");
  config->retries = 5;
  config->enable_idempotence = 1;
  snprintf(config->compression_type, sizeof(config->compression_type), "none");
  config->batch_size_bytes = 65536;
  config->linger_ms = 10;
  config->buffer_memory = 67108864;
  config->request_timeout_ms = 60000;
  config->message_timeout_ms = 120000;
}

int trace_config_load(const char* path, TraceConfig* config) {
  TracePropertyMap properties;
  const char* value;

  trace_config_init(config);
  trace_property_map_init(&properties);

  if (trace_load_properties(path, &properties) != 0) {
    fprintf(stderr, "[WARN] config file not found: %s, using defaults\n", path);
    trace_property_map_destroy(&properties);
    return 0;
  }

  value = trace_get_property(&properties, "kafka.bootstrap.servers", "");
  if (value[0] != '\0') {
    snprintf(config->bootstrap_servers, sizeof(config->bootstrap_servers), "%s", value);
  }

  value = trace_get_property(&properties, "kafka.topic", "");
  if (value[0] != '\0') {
    snprintf(config->topic, sizeof(config->topic), "%s", value);
  }

  value = trace_get_property(&properties, "trace.task.id", "");
  if (value[0] != '\0') {
    snprintf(config->task_id, sizeof(config->task_id), "%s", value);
  }

  value = trace_get_property(&properties, "kafka.acks", "");
  if (value[0] != '\0') {
    snprintf(config->acks, sizeof(config->acks), "%s", value);
  }

  value = trace_get_property(&properties, "kafka.compression.type", "");
  if (value[0] != '\0') {
    snprintf(config->compression_type, sizeof(config->compression_type), "%s", value);
  }

  config->period_ms = trace_parse_int(&properties, "trace.period.ms", config->period_ms);
  config->batch_size = trace_parse_int(&properties, "trace.batch.size", config->batch_size);
  config->max_frames = trace_parse_int64(&properties, "trace.max.frames", config->max_frames);
  config->send_interval_ms =
      trace_parse_int64(&properties, "trace.send.interval.ms", config->send_interval_ms);
  config->retries = trace_parse_int(&properties, "kafka.retries", config->retries);
  config->enable_idempotence =
      trace_parse_bool(&properties, "kafka.enable.idempotence", config->enable_idempotence);
  config->batch_size_bytes =
      trace_parse_int(&properties, "kafka.batch.size.bytes", config->batch_size_bytes);
  config->linger_ms = trace_parse_int(&properties, "kafka.linger.ms", config->linger_ms);
  config->buffer_memory =
      trace_parse_int(&properties, "kafka.buffer.memory", config->buffer_memory);
  config->request_timeout_ms =
      trace_parse_int(&properties, "kafka.request.timeout.ms", config->request_timeout_ms);
  config->message_timeout_ms =
      trace_parse_int(&properties, "kafka.message.timeout.ms", config->message_timeout_ms);

  trace_property_map_destroy(&properties);
  return 0;
}

int64_t trace_config_estimate_batches(const TraceConfig* config) {
  if (config->batch_size <= 0) {
    return 0;
  }

  return (config->max_frames + config->batch_size - 1) / config->batch_size;
}

void trace_config_dump(const TraceConfig* config) {
  int64_t total_batches = trace_config_estimate_batches(config);
  double estimated_seconds =
      (double)(total_batches * config->send_interval_ms) / 1000.0;

  printf("=== Trace Config ===\n");
  printf("  kafka.bootstrap.servers = %s\n", config->bootstrap_servers);
  printf("  kafka.topic             = %s\n", config->topic);
  printf("  kafka.acks              = %s\n", config->acks);
  printf("  kafka.idempotence       = %s\n",
         config->enable_idempotence ? "true" : "false");
  printf("  kafka.compression.type  = %s\n", config->compression_type);
  printf("  trace.max.frames        = %lld\n", (long long)config->max_frames);
  printf("  trace.batch.size        = %d\n", config->batch_size);
  printf("  trace.period.ms         = %d\n", config->period_ms);
  printf("  trace.send.interval.ms  = %lld\n",
         (long long)config->send_interval_ms);
  printf("  ---- estimate ----\n");
  printf("  total batches          = %lld\n", (long long)total_batches);
  printf("  estimated time         = %.1f s (%.1f min)\n",
         estimated_seconds, estimated_seconds / 60.0);
  printf("====================\n");
}
