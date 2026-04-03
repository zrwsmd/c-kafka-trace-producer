#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TraceConfig {
  char bootstrap_servers[256];
  char topic[128];

  int period_ms;
  int batch_size;
  int64_t max_frames;
  int64_t send_interval_ms;
  char task_id[128];

  char acks[32];
  int retries;
  int enable_idempotence;
  char compression_type[32];
  int batch_size_bytes;
  int linger_ms;
  int buffer_memory;
  int request_timeout_ms;
  int message_timeout_ms;
} TraceConfig;

void trace_config_init(TraceConfig* config);
int trace_config_load(const char* path, TraceConfig* config);
int64_t trace_config_estimate_batches(const TraceConfig* config);
void trace_config_dump(const TraceConfig* config);

#ifdef __cplusplus
}
#endif
