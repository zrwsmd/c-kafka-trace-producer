#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TraceProperty {
  char* key;
  char* value;
} TraceProperty;

typedef struct TracePropertyMap {
  TraceProperty* entries;
  size_t count;
  size_t capacity;
} TracePropertyMap;

void trace_property_map_init(TracePropertyMap* map);
void trace_property_map_destroy(TracePropertyMap* map);

int trace_load_properties(const char* path, TracePropertyMap* map);
char* trace_trim_inplace(char* value);
const char* trace_get_property(
    const TracePropertyMap* map,
    const char* key,
    const char* default_value);

#ifdef __cplusplus
}
#endif
