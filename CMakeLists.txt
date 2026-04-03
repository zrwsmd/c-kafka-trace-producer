cmake_minimum_required(VERSION 3.16)

project(c_kafka_trace_producer VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(GNUInstallDirs)
find_package(Threads REQUIRED)

set(RDKAFKA_ROOT "" CACHE PATH "Path to a local librdkafka install prefix")

if(CMAKE_CROSSCOMPILING AND NOT RDKAFKA_ROOT)
  message(FATAL_ERROR
    "Cross-compilation requires RDKAFKA_ROOT to point at an ARM64 librdkafka install prefix.")
endif()

function(resolve_rdkafka_target out_target)
  if(RDKAFKA_ROOT)
    set(_include_dir "${RDKAFKA_ROOT}/include")

    set(_library_candidates
      "${RDKAFKA_ROOT}/lib/librdkafka.so"
      "${RDKAFKA_ROOT}/lib64/librdkafka.so"
      "${RDKAFKA_ROOT}/lib/librdkafka.a"
      "${RDKAFKA_ROOT}/lib64/librdkafka.a"
    )

    set(_library_file "")
    foreach(candidate IN LISTS _library_candidates)
      if(EXISTS "${candidate}")
        set(_library_file "${candidate}")
        break()
      endif()
    endforeach()

    if(NOT EXISTS "${_include_dir}/librdkafka/rdkafka.h")
      message(FATAL_ERROR "Could not find librdkafka headers under ${_include_dir}")
    endif()

    if(NOT _library_file)
      message(FATAL_ERROR "Could not find librdkafka library under ${RDKAFKA_ROOT}/lib or lib64")
    endif()

    add_library(RdKafka::rdkafka UNKNOWN IMPORTED)
    set_target_properties(RdKafka::rdkafka PROPERTIES
      IMPORTED_LOCATION "${_library_file}"
      INTERFACE_INCLUDE_DIRECTORIES "${_include_dir}"
    )
    set(${out_target} RdKafka::rdkafka PARENT_SCOPE)
    return()
  endif()

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(RDKAFKA REQUIRED IMPORTED_TARGET rdkafka)
  set(${out_target} PkgConfig::RDKAFKA PARENT_SCOPE)
endfunction()

resolve_rdkafka_target(RDKAFKA_TARGET)

set(TRACE_COMMON_LIBS
  ${RDKAFKA_TARGET}
  Threads::Threads
  m
  ${CMAKE_DL_LIBS}
  rt
)

add_library(trace_common
  src/properties.c
  src/trace_config.c
  src/kafka_trace_producer.c
  src/trace_simulator.c
)

target_include_directories(trace_common
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(trace_common
  PUBLIC
    ${TRACE_COMMON_LIBS}
)

add_executable(c-kafka-trace-producer
  src/main.c
)

target_link_libraries(c-kafka-trace-producer PRIVATE trace_common)

add_executable(c-kafka-trace-consumer
  src/simple_consumer.c
)

target_include_directories(c-kafka-trace-consumer
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(c-kafka-trace-consumer
  PRIVATE
    ${TRACE_COMMON_LIBS}
)

add_executable(c-kafka-rdkafka-probe
  src/rdkafka_probe.c
)

target_include_directories(c-kafka-rdkafka-probe
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(c-kafka-rdkafka-probe
  PRIVATE
    ${TRACE_COMMON_LIBS}
)

install(TARGETS c-kafka-trace-producer c-kafka-trace-consumer c-kafka-rdkafka-probe
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES config/application.properties
  DESTINATION config
)
