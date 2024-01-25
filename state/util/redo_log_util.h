//
// Created by xuyanshi on 1/22/24.
//

#pragma once

#include "common.h"
// #include "state_store/redo_log.h"

static uint64_t LogHashFunc(const void *data, size_t size) {
  const uint64_t fnv_prime = 1099511628211ULL;
  uint64_t hash = 14695981039346656037ULL;

  const uint8_t *byte_data = static_cast<const uint8_t *>(data);

  for (size_t i = 0; i < size; ++i) {
    hash ^= byte_data[i];
    hash *= fnv_prime;
  }

  return hash;
}

// static void GetHashCodeForTxn(RedoLogItem *logItem) { return; }