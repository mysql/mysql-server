// Copyright (c) 2025, Oracle and/or its affiliates.

#include "vector-common/vector_operations.h"
#include <cmath>

namespace vector_operations {

double l2_distance(const float *v1, const float *v2, uint32_t dimensions) {
  double sum = 0.0;
  for (uint32_t i = 0; i < dimensions; i++) {
    double diff = v1[i] - v2[i];
    sum += diff * diff;
  }
  return std::sqrt(sum);
}

double dot_product(const float *v1, const float *v2, uint32_t dimensions) {
  double sum = 0.0;
  for (uint32_t i = 0; i < dimensions; i++) {
    sum += v1[i] * v2[i];
  }
  return sum;
}

double cosine_similarity(const float *v1, const float *v2, uint32_t dimensions) {
  double dot = dot_product(v1, v2, dimensions);
  
  double mag1 = 0.0, mag2 = 0.0;
  for (uint32_t i = 0; i < dimensions; i++) {
    mag1 += v1[i] * v1[i];
    mag2 += v2[i] * v2[i];
  }
  
  double magnitude = std::sqrt(mag1) * std::sqrt(mag2);
  if (magnitude < 1e-10) return 0.0;
  
  return dot / magnitude;
}

double cosine_distance(const float *v1, const float *v2, uint32_t dimensions) {
  return 1.0 - cosine_similarity(v1, v2, dimensions);
}

}
