#pragma once

#include <signal.h>

#include "kafka_trace_producer.h"
#include "trace_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TraceSimulator {
  TraceConfig config;
  TraceKafkaProducer* producer;
  unsigned int rng_state;
} TraceSimulator;

void trace_simulator_init(
    TraceSimulator* simulator,
    const TraceConfig* config,
    TraceKafkaProducer* producer);

int trace_simulator_run(
    TraceSimulator* simulator,
    const volatile sig_atomic_t* stop_requested);

#ifdef __cplusplus
}
#endif
