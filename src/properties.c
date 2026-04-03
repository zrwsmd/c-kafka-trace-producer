#include "properties.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* trace_strdup(const char* value) {
  size_t length = strlen(value) + 1;
  char* copy = (char*)malloc(length);
  if (copy != NULL) {
    memcpy(copy, value, length);
  }
  return copy;
}

void trace_property_map_init(TracePropertyMap* map) {
  map->entries = NULL;
  map->count = 0;
  map->capacity = 0;
}

void trace_property_map_destroy(TracePropertyMap* map) {
  size_t index;
  for (index = 0; index < map->count; ++index) {
    free(map->entries[index].key);
    free(map->entries[index].value);
  }
  free(map->entries);
  map->entries = NULL;
  map->count = 0;
  map->capacity = 0;
}

char* trace_trim_inplace(char* value) {
  char* end;
  while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') {
    ++value;
  }

  if (*value == '\0') {
    return value;
  }

  end = value + strlen(value) - 1;
  while (end > value &&
         (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
    *end-- = '\0';
  }

  return value;
}

static int trace_property_map_reserve(TracePropertyMap* map, size_t needed) {
  TraceProperty* entries;
  size_t new_capacity;

  if (needed <= map->capacity) {
    return 0;
  }

  new_capacity = (map->capacity == 0) ? 16 : map->capacity * 2;
  while (new_capacity < needed) {
    new_capacity *= 2;
  }

  entries = (TraceProperty*)realloc(map->entries, new_capacity * sizeof(*entries));
  if (entries == NULL) {
    return -1;
  }

  map->entries = entries;
  map->capacity = new_capacity;
  return 0;
}

static int trace_property_map_set(TracePropertyMap* map, const char* key, const char* value) {
  size_t index;
  char* key_copy;
  char* value_copy;

  for (index = 0; index < map->count; ++index) {
    if (strcmp(map->entries[index].key, key) == 0) {
      value_copy = trace_strdup(value);
      if (value_copy == NULL) {
        return -1;
      }
      free(map->entries[index].value);
      map->entries[index].value = value_copy;
      return 0;
    }
  }

  if (trace_property_map_reserve(map, map->count + 1) != 0) {
    return -1;
  }

  key_copy = trace_strdup(key);
  value_copy = trace_strdup(value);
  if (key_copy == NULL || value_copy == NULL) {
    free(key_copy);
    free(value_copy);
    return -1;
  }

  map->entries[map->count].key = key_copy;
  map->entries[map->count].value = value_copy;
  map->count += 1;
  return 0;
}

int trace_load_properties(const char* path, TracePropertyMap* map) {
  FILE* input = fopen(path, "r");
  char line[1024];

  if (input == NULL) {
    return -1;
  }

  while (fgets(line, sizeof(line), input) != NULL) {
    char* cleaned = trace_trim_inplace(line);
    char* delimiter;
    char* key;
    char* value;

    if (cleaned[0] == '\0' || cleaned[0] == '#' || cleaned[0] == ';') {
      continue;
    }

    delimiter = strchr(cleaned, '=');
    if (delimiter == NULL) {
      continue;
    }

    *delimiter = '\0';
    key = trace_trim_inplace(cleaned);
    value = trace_trim_inplace(delimiter + 1);
    if (key[0] == '\0') {
      continue;
    }

    if (trace_property_map_set(map, key, value) != 0) {
      fclose(input);
      return -1;
    }
  }

  fclose(input);
  return 0;
}

const char* trace_get_property(
    const TracePropertyMap* map,
    const char* key,
    const char* default_value) {
  size_t index;
  for (index = 0; index < map->count; ++index) {
    if (strcmp(map->entries[index].key, key) == 0) {
      return map->entries[index].value;
    }
  }
  return default_value;
}
