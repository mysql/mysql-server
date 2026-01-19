// Optimized vector operations with single-pass algorithms
// Fixes Code Review Issue #7: Performance optimization

#include "vector-common/vector_operations.h"
#include <cmath>

namespace vector_operations {

double l2_distance(const float *v1, const float *v2, uint32_t dimensions) {
  double sum = 0.0;
  for (uint32_t i = 0; i < dimensions; i++) {
    double diff = static_cast<double>(v1[i]) - static_cast<double>(v2[i]);
    sum += diff * diff;
  }
  return std::sqrt(sum);
}

double dot_product(const float *v1, const float *v2, uint32_t dimensions) {
  double sum = 0.0;
  for (uint32_t i = 0; i < dimensions; i++) {
    sum += static_cast<double>(v1[i]) * static_cast<double>(v2[i]);
  }
  return sum;
}

// OPTIMIZED: Single-pass computation for cosine similarity
// Previously iterated 3 times (dot_product + 2x magnitude), now single pass
double cosine_similarity(const float *v1, const float *v2, uint32_t dimensions) {
  double dot = 0.0;
  double mag1 = 0.0;
  double mag2 = 0.0;
  
  // Single loop computes all three values
  for (uint32_t i = 0; i < dimensions; i++) {
    double a = static_cast<double>(v1[i]);
    double b = static_cast<double>(v2[i]);
    dot += a * b;
    mag1 += a * a;
    mag2 += b * b;
  }
  
  double magnitude = std::sqrt(mag1) * std::sqrt(mag2);
  if (magnitude < 1e-10) return 0.0;  // Avoid division by zero
  
  return dot / magnitude;
}

double cosine_distance(const float *v1, const float *v2, uint32_t dimensions) {
  return 1.0 - cosine_similarity(v1, v2, dimensions);
}

}  // namespace vector_operations
