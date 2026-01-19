// Fixed unit test with correct namespace comment (Issue #6)
// Changed "arrow_operations_unittest" -> "vector_operations_unittest"

#include <gtest/gtest.h>
#include "vector-common/vector_operations.h"
#include <cmath>

namespace vector_operations_unittest {

TEST(VectorOperations, L2Distance) {
  float v1[] = {0.0f, 0.0f, 0.0f};
  float v2[] = {3.0f, 4.0f, 0.0f};
  
  double result = vector_operations::l2_distance(v1, v2, 3);
  EXPECT_DOUBLE_EQ(5.0, result);
}

TEST(VectorOperations, CosineDistance) {
  float v1[] = {1.0f, 0.0f, 0.0f};
  float v2[] = {0.0f, 1.0f, 0.0f};
  
  // Orthogonal vectors: distance should be 1.0
  double result = vector_operations::cosine_distance(v1, v2, 3);
  EXPECT_NEAR(1.0, result, 1e-6);
  
  float v3[] = {1.0f, 2.0f, 3.0f};
  // Same vector: distance should be 0.0
  double result_same = vector_operations::cosine_distance(v3, v3, 3);
  EXPECT_NEAR(0.0, result_same, 1e-6);
}

TEST(VectorOperations, DotProduct) {
  float v1[] = {1.0f, 2.0f, 3.0f};
  float v2[] = {4.0f, 5.0f, 6.0f};
  
  double result = vector_operations::dot_product(v1, v2, 3);
  EXPECT_DOUBLE_EQ(32.0, result);
}

TEST(VectorOperations, CosineSimilarity) {
  float v1[] = {1.0f, 0.0f, 0.0f};
  float v2[] = {1.0f, 0.0f, 0.0f};
  
  // Same direction: similarity should be 1.0
  double result = vector_operations::cosine_similarity(v1, v2, 3);
  EXPECT_NEAR(1.0, result, 1e-6);
}

}  // namespace vector_operations_unittest
