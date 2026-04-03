#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <librdkafka/rdkafka.h>

static volatile sig_atomic_t g_stop_requested = 0;

static void trace_consumer_handle_signal(int signal_number) {
  (void)signal_number;
  g_stop_requested = 1;
}

static long long trace_consumer_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000LL + (long long)ts.tv_nsec / 1000000LL;
}

static void trace_consumer_set_conf_or_exit(
    rd_kafka_conf_t* conf,
    const char* name,
    const char* value) {
  char error_buffer[512] = {0};
  rd_kafka_conf_res_t result =
      rd_kafka_conf_set(conf, name, value, error_buffer, sizeof(error_buffer));
  if (result != RD_KAFKA_CONF_OK) {
    fprintf(stderr, "[ERROR] failed to set Kafka consumer config '%s': %s\n",
            name, error_buffer);
    exit(1);
  }
}

int main(int argc, char** argv) {
  const char* servers = (argc > 1) ? argv[1] : "47.129.128.147:9092";
  const char* topic = (argc > 2) ? argv[2] : "trace-data";
  char error_buffer[512] = {0};
  rd_kafka_conf_t* conf;
  rd_kafka_t* consumer;
  rd_kafka_topic_partition_list_t* subscription;
  rd_kafka_resp_err_t subscribe_error;
  long long batch_count = 0;
  long long start_ms;

  signal(SIGINT, trace_consumer_handle_signal);
  signal(SIGTERM, trace_consumer_handle_signal);

  printf("=== Native Kafka Trace Consumer ===\n");
  printf("  servers: %s\n", servers);
  printf("  topic:   %s\n", topic);

  conf = rd_kafka_conf_new();
  trace_consumer_set_conf_or_exit(conf, "bootstrap.servers", servers);
  trace_consumer_set_conf_or_exit(conf, "group.id", "trace-consumer-group");
  trace_consumer_set_conf_or_exit(conf, "auto.offset.reset", "earliest");
  trace_consumer_set_conf_or_exit(conf, "enable.auto.commit", "true");

  consumer = rd_kafka_new(RD_KAFKA_CONSUMER, conf, error_buffer, sizeof(error_buffer));
  if (consumer == NULL) {
    fprintf(stderr, "[ERROR] failed to create consumer: %s\n", error_buffer);
    return 1;
  }

  rd_kafka_poll_set_consumer(consumer);

  subscription = rd_kafka_topic_partition_list_new(1);
  rd_kafka_topic_partition_list_add(subscription, topic, RD_KAFKA_PARTITION_UA);

  subscribe_error = rd_kafka_subscribe(consumer, subscription);
  rd_kafka_topic_partition_list_destroy(subscription);
  if (subscribe_error != RD_KAFKA_RESP_ERR_NO_ERROR) {
    fprintf(stderr, "[ERROR] subscribe failed: %s\n", rd_kafka_err2str(subscribe_error));
    rd_kafka_destroy(consumer);
    return 1;
  }

  printf("start consuming... (Ctrl+C to exit)\n");
  start_ms = trace_consumer_now_ms();

  while (!g_stop_requested) {
    rd_kafka_message_t* message = rd_kafka_consumer_poll(consumer, 1000);
    if (message == NULL) {
      continue;
    }

    if (message->err) {
      if (message->err != RD_KAFKA_RESP_ERR__PARTITION_EOF &&
          message->err != RD_KAFKA_RESP_ERR__TIMED_OUT) {
        fprintf(stderr, "[Consumer] error: %s\n", rd_kafka_message_errstr(message));
      }
      rd_kafka_message_destroy(message);
      continue;
    }

    batch_count += 1;
    if (batch_count % 1000 == 0) {
      long long elapsed_ms = trace_consumer_now_ms() - start_ms;
      double batches_per_second =
          (double)batch_count * 1000.0 / (double)((elapsed_ms > 0) ? elapsed_ms : 1);

      printf("[Consumer] received %lld batches (%.0f batches/s) partition=%d offset=%" PRId64 "\n",
             batch_count,
             batches_per_second,
             message->partition,
             message->offset);
    }

    rd_kafka_message_destroy(message);
  }

  printf("\n=== Consumer Summary ===\n");
  printf("batches consumed: %lld\n", batch_count);
  printf("elapsed: %.1f s\n",
         (double)(trace_consumer_now_ms() - start_ms) / 1000.0);

  rd_kafka_consumer_close(consumer);
  rd_kafka_destroy(consumer);
  return 0;
}
