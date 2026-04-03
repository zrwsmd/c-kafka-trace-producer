#include <signal.h>
#include <stdio.h>

#include "kafka_trace_producer.h"
#include "trace_config.h"
#include "trace_simulator.h"

static volatile sig_atomic_t g_stop_requested = 0;

static void trace_handle_signal(int signal_number) {
  (void)signal_number;
  g_stop_requested = 1;
}

int main(int argc, char** argv) {
  const char* config_path = (argc > 1) ? argv[1] : "config/application.properties";
  TraceConfig config;
  TraceKafkaProducer producer;
  TraceSimulator simulator;
  char error_buffer[512];
  int exit_code;

  signal(SIGINT, trace_handle_signal);
  signal(SIGTERM, trace_handle_signal);

  printf("=== Native Kafka Trace Producer ===\n\n");

  trace_config_load(config_path, &config);
  trace_config_dump(&config);

  if (trace_kafka_producer_init(&producer, &config, error_buffer, sizeof(error_buffer)) != 0) {
    fprintf(stderr, "[ERROR] %s\n", error_buffer);
    return 1;
  }

  trace_simulator_init(&simulator, &config, &producer);

  printf("\n>>> sending %lld frames to Kafka ...\n\n",
         (long long)config.max_frames);

  exit_code = trace_simulator_run(&simulator, &g_stop_requested);
  trace_kafka_producer_close(&producer);

  printf("\nprogram exit\n");
  return exit_code;
}
