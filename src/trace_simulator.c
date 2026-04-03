#define _POSIX_C_SOURCE 200809L

#include "trace_simulator.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

typedef struct TraceStringBuilder {
  char* data;
  size_t length;
  size_t capacity;
} TraceStringBuilder;

static const double k_pi = 3.14159265358979323846;

static long long trace_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000LL + (long long)ts.tv_nsec / 1000000LL;
}

static unsigned int trace_seed_value(const TraceConfig* config) {
  unsigned int seed = (unsigned int)time(NULL);
  const unsigned char* cursor = (const unsigned char*)config->task_id;
  while (*cursor != '\0') {
    seed = seed * 131u + (unsigned int)(*cursor++);
  }
#ifndef _WIN32
  seed ^= (unsigned int)getpid();
#endif
  return seed;
}

static unsigned int trace_next_rand(unsigned int* state) {
  *state = (*state * 1103515245u) + 12345u;
  return *state;
}

static double trace_round3(double value) {
  return round(value * 1000.0) / 1000.0;
}

static double trace_noise(TraceSimulator* simulator, double scale) {
  double unit = (double)(trace_next_rand(&simulator->rng_state) & 0xffffu) / 65535.0;
  return ((unit * 2.0) - 1.0) * scale * 0.02;
}

static double trace_simulate_axis1_position(TraceSimulator* simulator, int64_t ts, int period_ms) {
  double t = (double)ts * (double)period_ms / 1000.0;
  return trace_round3(100.0 * sin(2.0 * k_pi * 0.5 * t) + trace_noise(simulator, 0.5));
}

static double trace_simulate_axis1_velocity(TraceSimulator* simulator, int64_t ts, int period_ms) {
  double t = (double)ts * (double)period_ms / 1000.0;
  return trace_round3(k_pi * 100.0 * cos(2.0 * k_pi * 0.5 * t) + trace_noise(simulator, 1.0));
}

static double trace_simulate_axis1_torque(TraceSimulator* simulator, int64_t ts, int period_ms) {
  double t = (double)ts * (double)period_ms / 1000.0;
  return trace_round3(50.0 + 20.0 * sin(2.0 * k_pi * 1.0 * t) + trace_noise(simulator, 0.5));
}

static double trace_simulate_axis2_position(TraceSimulator* simulator, int64_t ts, int period_ms) {
  double t = (double)ts * (double)period_ms / 1000.0;
  return trace_round3(80.0 * sin(2.0 * k_pi * 0.3 * t + 1.0) + trace_noise(simulator, 0.5));
}

static double trace_simulate_axis2_velocity(TraceSimulator* simulator, int64_t ts, int period_ms) {
  double t = (double)ts * (double)period_ms / 1000.0;
  return trace_round3(80.0 * 0.3 * 2.0 * k_pi * cos(2.0 * k_pi * 0.3 * t + 1.0) +
                      trace_noise(simulator, 0.5));
}

static double trace_simulate_motor_rpm(TraceSimulator* simulator, int64_t ts, int period_ms) {
  double t = (double)ts * (double)period_ms / 1000.0;
  return trace_round3(1500.0 + 300.0 * sin(2.0 * k_pi * 0.1 * t) + trace_noise(simulator, 5.0));
}

static double trace_simulate_motor_temp(TraceSimulator* simulator, int64_t ts, int period_ms) {
  double t = (double)ts * (double)period_ms / 1000.0;
  return trace_round3(65.0 + 10.0 * sin(2.0 * k_pi * 0.02 * t) + trace_noise(simulator, 0.2));
}

static double trace_simulate_servo_current(TraceSimulator* simulator, int64_t ts, int period_ms) {
  double t = (double)ts * (double)period_ms / 1000.0;
  return trace_round3(5.0 + 2.0 * sin(2.0 * k_pi * 0.8 * t) + trace_noise(simulator, 0.1));
}

static double trace_simulate_servo_voltage(TraceSimulator* simulator, int64_t ts, int period_ms) {
  double t = (double)ts * (double)period_ms / 1000.0;
  return trace_round3(48.0 + 0.5 * sin(2.0 * k_pi * 0.05 * t) + trace_noise(simulator, 0.05));
}

static double trace_simulate_pressure(TraceSimulator* simulator, int64_t ts, int period_ms) {
  double t = (double)ts * (double)period_ms / 1000.0;
  return trace_round3(5.0 + 1.5 * sin(2.0 * k_pi * 0.2 * t) + trace_noise(simulator, 0.05));
}

static int trace_string_builder_reserve(TraceStringBuilder* builder, size_t needed) {
  char* new_data;
  size_t new_capacity = builder->capacity;

  if (needed <= builder->capacity) {
    return 0;
  }

  while (new_capacity < needed) {
    new_capacity *= 2;
  }

  new_data = (char*)realloc(builder->data, new_capacity);
  if (new_data == NULL) {
    return -1;
  }

  builder->data = new_data;
  builder->capacity = new_capacity;
  return 0;
}

static int trace_string_builder_appendf(TraceStringBuilder* builder, const char* format, ...) {
  int written;
  va_list args;
  va_list args_copy;

  while (1) {
    va_start(args, format);
    va_copy(args_copy, args);
    written = vsnprintf(builder->data + builder->length,
                        builder->capacity - builder->length, format, args_copy);
    va_end(args_copy);
    va_end(args);

    if (written < 0) {
      return -1;
    }

    if ((size_t)written < builder->capacity - builder->length) {
      builder->length += (size_t)written;
      return 0;
    }

    if (trace_string_builder_reserve(
            builder, builder->length + (size_t)written + 1) != 0) {
      return -1;
    }
  }
}

static int trace_string_builder_append_json_escaped(
    TraceStringBuilder* builder,
    const char* input) {
  const unsigned char* cursor = (const unsigned char*)input;
  while (*cursor != '\0') {
    switch (*cursor) {
      case '\\':
        if (trace_string_builder_appendf(builder, "\\\\") != 0) {
          return -1;
        }
        break;
      case '"':
        if (trace_string_builder_appendf(builder, "\\\"") != 0) {
          return -1;
        }
        break;
      case '\n':
        if (trace_string_builder_appendf(builder, "\\n") != 0) {
          return -1;
        }
        break;
      case '\r':
        if (trace_string_builder_appendf(builder, "\\r") != 0) {
          return -1;
        }
        break;
      case '\t':
        if (trace_string_builder_appendf(builder, "\\t") != 0) {
          return -1;
        }
        break;
      default:
        if (trace_string_builder_appendf(builder, "%c", *cursor) != 0) {
          return -1;
        }
        break;
    }
    ++cursor;
  }

  return 0;
}

static void trace_format_timestamp(char* buffer, size_t buffer_size) {
  time_t now = time(NULL);
  struct tm time_info;
#if defined(_WIN32)
  localtime_s(&time_info, &now);
#else
  localtime_r(&now, &time_info);
#endif
  strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &time_info);
}

static char* trace_simulator_build_payload(
    TraceSimulator* simulator,
    int64_t seq,
    int64_t* ts_counter,
    int* frame_count) {
  TraceStringBuilder builder;
  int index;

  builder.capacity = (size_t)(256 + simulator->config.batch_size * 256);
  if (builder.capacity < 1024) {
    builder.capacity = 1024;
  }
  builder.length = 0;
  builder.data = (char*)malloc(builder.capacity);
  if (builder.data == NULL) {
    return NULL;
  }

  builder.data[0] = '\0';
  if (trace_string_builder_appendf(&builder, "{\"taskId\":\"") != 0 ||
      trace_string_builder_append_json_escaped(&builder, simulator->config.task_id) != 0 ||
      trace_string_builder_appendf(
          &builder, "\",\"seq\":%lld,\"period\":%d,\"frames\":[",
          (long long)seq, simulator->config.period_ms) != 0) {
    free(builder.data);
    return NULL;
  }

  *frame_count = 0;
  for (index = 0; index < simulator->config.batch_size; ++index) {
    int64_t next_ts = *ts_counter + 1;
    if (next_ts > simulator->config.max_frames) {
      break;
    }

    *ts_counter = next_ts;
    if (*frame_count > 0 && trace_string_builder_appendf(&builder, ",") != 0) {
      free(builder.data);
      return NULL;
    }

    if (trace_string_builder_appendf(
            &builder,
            "{\"ts\":%lld,\"axis1_position\":%.3f,\"axis1_velocity\":%.3f,"
            "\"axis1_torque\":%.3f,\"axis2_position\":%.3f,\"axis2_velocity\":%.3f,"
            "\"motor_rpm\":%.3f,\"motor_temp\":%.3f,\"servo_current\":%.3f,"
            "\"servo_voltage\":%.3f,\"pressure_bar\":%.3f}",
            (long long)*ts_counter,
            trace_simulate_axis1_position(
                simulator, *ts_counter, simulator->config.period_ms),
            trace_simulate_axis1_velocity(
                simulator, *ts_counter, simulator->config.period_ms),
            trace_simulate_axis1_torque(
                simulator, *ts_counter, simulator->config.period_ms),
            trace_simulate_axis2_position(
                simulator, *ts_counter, simulator->config.period_ms),
            trace_simulate_axis2_velocity(
                simulator, *ts_counter, simulator->config.period_ms),
            trace_simulate_motor_rpm(
                simulator, *ts_counter, simulator->config.period_ms),
            trace_simulate_motor_temp(
                simulator, *ts_counter, simulator->config.period_ms),
            trace_simulate_servo_current(
                simulator, *ts_counter, simulator->config.period_ms),
            trace_simulate_servo_voltage(
                simulator, *ts_counter, simulator->config.period_ms),
            trace_simulate_pressure(
                simulator, *ts_counter, simulator->config.period_ms)) != 0) {
      free(builder.data);
      return NULL;
    }

    *frame_count += 1;
  }

  if (trace_string_builder_appendf(&builder, "]}") != 0) {
    free(builder.data);
    return NULL;
  }

  return builder.data;
}

void trace_simulator_init(
    TraceSimulator* simulator,
    const TraceConfig* config,
    TraceKafkaProducer* producer) {
  memset(simulator, 0, sizeof(*simulator));
  simulator->config = *config;
  simulator->producer = producer;
  simulator->rng_state = trace_seed_value(config);
}

int trace_simulator_run(
    TraceSimulator* simulator,
    const volatile sig_atomic_t* stop_requested) {
  int64_t total_batches = trace_config_estimate_batches(&simulator->config);
  int64_t progress_interval = total_batches / 100;
  int64_t ts_counter = 0;
  int64_t frame_sent = 0;
  int64_t success_batches = 0;
  long long start_ms = trace_now_ms();
  int64_t seq;
  char error_buffer[512];

  if (progress_interval < 1) {
    progress_interval = 1;
  }

  printf("[TraceSimulator] starting: maxFrames=%lld batchSize=%d totalBatches=%lld "
         "sendInterval=%lldms topic=%s\n",
         (long long)simulator->config.max_frames,
         simulator->config.batch_size,
         (long long)total_batches,
         (long long)simulator->config.send_interval_ms,
         simulator->config.topic);
  printf("[TraceSimulator] send loop started: %lld batches (%lld frames)\n",
         (long long)total_batches,
         (long long)simulator->config.max_frames);

  for (seq = 1; seq <= total_batches; ++seq) {
    char* payload;
    int frame_count = 0;
    long long now_ms;
    double speed;
    long long eta_seconds;

    if (*stop_requested) {
      printf("[TraceSimulator] stop requested, ending send loop\n");
      break;
    }

    payload = trace_simulator_build_payload(simulator, seq, &ts_counter, &frame_count);
    if (payload == NULL) {
      fprintf(stderr, "[TraceSimulator] FATAL: seq=%lld payload allocation failed\n",
              (long long)seq);
      break;
    }

    frame_sent += frame_count;
    if (trace_kafka_producer_send_sync(
            simulator->producer,
            simulator->config.task_id,
            payload,
            error_buffer,
            sizeof(error_buffer)) != 0) {
      fprintf(stderr, "[TraceSimulator] FATAL: seq=%lld send failed: %s\n",
              (long long)seq, error_buffer);
      free(payload);
      break;
    }

    free(payload);
    success_batches += 1;

    if (seq % progress_interval == 0 || seq == total_batches) {
      char timestamp[32];
      long long elapsed_ms;

      now_ms = trace_now_ms();
      elapsed_ms = now_ms - start_ms;
      speed = (double)seq * 1000.0 / (double)((elapsed_ms > 0) ? elapsed_ms : 1);
      eta_seconds = (long long)((total_batches - seq) / ((speed > 0.01) ? speed : 0.01));
      trace_format_timestamp(timestamp, sizeof(timestamp));

      printf("[%s] [TraceSimulator] progress: %.1f%% (%lld/%lld batches, %lld frames) "
             "speed=%.0f batches/s elapsed=%llds ETA=%llds\n",
             timestamp,
             ((double)seq * 100.0) / (double)total_batches,
             (long long)seq,
             (long long)total_batches,
             (long long)frame_sent,
             speed,
             elapsed_ms / 1000LL,
             eta_seconds);
    }

    if (seq < total_batches && simulator->config.send_interval_ms > 0) {
      struct timespec delay;
      delay.tv_sec = (time_t)(simulator->config.send_interval_ms / 1000);
      delay.tv_nsec = (long)((simulator->config.send_interval_ms % 1000) * 1000000LL);
      nanosleep(&delay, NULL);
    }
  }

  {
    long long elapsed_ms = trace_now_ms() - start_ms;
    double elapsed_seconds = (double)elapsed_ms / 1000.0;

    printf("\n");
    printf("========== TraceSimulator Final Summary ==========\n");
    printf("frames sent     %lld / %lld\n",
           (long long)frame_sent, (long long)simulator->config.max_frames);
    printf("batches sent    %lld / %lld\n",
           (long long)success_batches, (long long)total_batches);
    printf("send failures   %lld\n",
           trace_kafka_producer_failed_count(simulator->producer));
    printf("elapsed         %.1f s (%.1f min)\n",
           elapsed_seconds, elapsed_seconds / 60.0);
    if (elapsed_ms > 0) {
      printf("avg batch rate  %.0f batches/s\n",
             (double)success_batches * 1000.0 / (double)elapsed_ms);
      printf("avg frame rate  %.0f frames/s\n",
             (double)frame_sent * 1000.0 / (double)elapsed_ms);
    }
    printf("result          %s\n",
           (success_batches == total_batches) ? "all batches delivered"
                                              : "stopped before completion");
    printf("==================================================\n");
  }

  return (success_batches == total_batches) ? 0 : 1;
}
