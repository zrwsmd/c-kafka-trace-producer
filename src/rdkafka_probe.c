#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <librdkafka/rdkafka.h>

static char* trace_probe_strdup(const char* value) {
  size_t length = strlen(value) + 1;
  char* copy = (char*)malloc(length);
  if (copy != NULL) {
    memcpy(copy, value, length);
  }
  return copy;
}

static void trace_probe_trim(char* value) {
  char* start = value;
  char* end;

  while (*start != '\0' && isspace((unsigned char)*start)) {
    ++start;
  }

  if (start != value) {
    memmove(value, start, strlen(start) + 1);
  }

  if (value[0] == '\0') {
    return;
  }

  end = value + strlen(value) - 1;
  while (end >= value && isspace((unsigned char)*end)) {
    *end-- = '\0';
  }
}

static int trace_probe_has_feature(const char* builtin_features, const char* required) {
  char* copy = trace_probe_strdup(builtin_features);
  char* token;
  int found = 0;

  if (copy == NULL) {
    return 0;
  }

  token = strtok(copy, ",");
  while (token != NULL) {
    trace_probe_trim(token);
    if (strcmp(token, required) == 0) {
      found = 1;
      break;
    }
    token = strtok(NULL, ",");
  }

  free(copy);
  return found;
}

static void trace_probe_print_usage(const char* argv0) {
  printf("Usage: %s [--require feature1,feature2,...]\n\n", argv0);
  printf("Examples:\n");
  printf("  %s\n", argv0);
  printf("  %s --require ssl,sasl\n", argv0);
}

int main(int argc, char** argv) {
  const char* required_features = NULL;
  rd_kafka_conf_t* conf;
  char features_buffer[4096];
  size_t features_size = sizeof(features_buffer);
  rd_kafka_conf_res_t get_result;
  char errstr[512];
  rd_kafka_t* rk;
  int index;
  int missing_required = 0;

  for (index = 1; index < argc; ++index) {
    const char* arg = argv[index];
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      trace_probe_print_usage(argv[0]);
      return 0;
    }

    if (strcmp(arg, "--require") == 0) {
      if (index + 1 >= argc) {
        fprintf(stderr, "Missing value for --require\n");
        trace_probe_print_usage(argv[0]);
        return 2;
      }
      required_features = argv[++index];
      continue;
    }

    fprintf(stderr, "Unknown argument: %s\n", arg);
    trace_probe_print_usage(argv[0]);
    return 2;
  }

  printf("=== librdkafka Probe ===\n");
  printf("librdkafka version: %s (%d)\n",
         rd_kafka_version_str(), rd_kafka_version());

  conf = rd_kafka_conf_new();
  if (conf == NULL) {
    fprintf(stderr, "Failed to create rd_kafka_conf\n");
    return 3;
  }

  get_result = rd_kafka_conf_get(conf, "builtin.features", features_buffer, &features_size);
  if (get_result != RD_KAFKA_CONF_OK) {
    snprintf(features_buffer, sizeof(features_buffer), "<unavailable>");
  }

  printf("builtin.features: %s\n", features_buffer);

  rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
  if (rk == NULL) {
    fprintf(stderr, "rd_kafka_new failed: %s\n", errstr);
    return 4;
  }

  printf("rd_kafka_new: ok\n");

  if (required_features != NULL) {
    char* required_copy = trace_probe_strdup(required_features);
    char* token;

    if (required_copy == NULL) {
      rd_kafka_destroy(rk);
      fprintf(stderr, "Failed to allocate memory for required features\n");
      return 6;
    }

    token = strtok(required_copy, ",");
    while (token != NULL) {
      trace_probe_trim(token);
      if (token[0] != '\0') {
        int present = trace_probe_has_feature(features_buffer, token);
        printf("require[%s]: %s\n", token, present ? "ok" : "missing");
        if (!present) {
          missing_required = 1;
        }
      }
      token = strtok(NULL, ",");
    }

    free(required_copy);
  }

  rd_kafka_destroy(rk);

  if (missing_required) {
    return 5;
  }

  printf("probe result: ok\n");
  return 0;
}
