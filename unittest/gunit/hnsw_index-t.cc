/**
  @file unittest/gunit/hnsw_index-t.cc
  
  Unit tests for HNSW Index implementation
*/

#include <gtest/gtest.h>
#include "storage/innobase/vector/vec0hnsw.h"
#include <random>
#include <cmath>

namespace innodb_vector_unittest {

class HnswIndexTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.M = 16;
    config_.M0 = 32;
    config_.ef_construction = 100;
    config_.ef_search = 50;
    config_.max_elements = 10000;
    config_.dimensions = 128;
  }

  innodb_vector::hnsw_config_t config_;
  
  std::vector<float> random_vector(size_t dims) {
    static std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> vec(dims);
    for (auto &v : vec) v = dist(rng);
    return vec;
  }

  double l2_distance(const std::vector<float> &a, const std::vector<float> &b) {
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
      double diff = a[i] - b[i];
      sum += diff * diff;
    }
    return std::sqrt(sum);
  }
};

TEST_F(HnswIndexTest, EmptyIndex) {
  innodb_vector::HnswIndex index(config_);
  EXPECT_EQ(0u, index.size());
  
  auto results = index.search(random_vector(128), 10);
  EXPECT_TRUE(results.empty());
}

TEST_F(HnswIndexTest, SingleInsert) {
  innodb_vector::HnswIndex index(config_);
  auto vec = random_vector(128);
  
  EXPECT_TRUE(index.insert(1, vec));
  EXPECT_EQ(1u, index.size());
}

TEST_F(HnswIndexTest, MultipleInserts) {
  innodb_vector::HnswIndex index(config_);
  
  for (uint64_t i = 0; i < 100; ++i) {
    EXPECT_TRUE(index.insert(i, random_vector(128)));
  }
  
  EXPECT_EQ(100u, index.size());
}

TEST_F(HnswIndexTest, ExternalIdCorrectness) {
  innodb_vector::HnswIndex index(config_);
  
  // Insert with non-sequential, large IDs to ensure we aren't returning internal indices (0, 1, 2...)
  uint64_t id1 = 1001;
  uint64_t id2 = 5005;
  uint64_t id3 = 9999;
  
  auto vec1 = random_vector(128);
  auto vec2 = random_vector(128);
  auto vec3 = random_vector(128);
  
  index.insert(id1, vec1);
  index.insert(id2, vec2);
  index.insert(id3, vec3);
  
  auto results = index.search(vec2, 1);
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(id2, results[0].id) << "Should return external ID " << id2 << ", not internal index";
  
  results = index.search(vec3, 1);
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(id3, results[0].id) << "Should return external ID " << id3;
}

TEST_F(HnswIndexTest, SearchFindsExactMatch) {
  innodb_vector::HnswIndex index(config_);
  
  // Insert vectors with known IDs using a stride/offset
  std::vector<std::vector<float>> vectors;
  uint64_t id_offset = 10000;
  
  for (uint64_t i = 0; i < 100; ++i) {
    auto vec = random_vector(128);
    vectors.push_back(vec);
    index.insert(id_offset + i, vec);
  }
  
  // Search for vector 50 (ID 10050)
  auto results = index.search(vectors[50], 1);
  
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(id_offset + 50, results[0].id);
  EXPECT_NEAR(0.0, results[0].distance, 1e-6);
}

TEST_F(HnswIndexTest, SearchReturnsKNearest) {
  innodb_vector::HnswIndex index(config_);
  
  for (uint64_t i = 0; i < 1000; ++i) {
    // ID = i * 2 to distinguish from index
    index.insert(i * 2, random_vector(128));
  }
  
  auto query = random_vector(128);
  auto results = index.search(query, 10);
  
  EXPECT_EQ(10u, results.size());
  
  // Verify results are sorted by distance
  for (size_t i = 1; i < results.size(); ++i) {
    EXPECT_LE(results[i-1].distance, results[i].distance);
    // Verify ID is even (simple check that we got our IDs back)
    EXPECT_EQ(0u, results[i].id % 2);
  }
}

TEST_F(HnswIndexTest, RecallQuality) {
  // Simple recall test - exact search should have high recall
  innodb_vector::HnswIndex index(config_);
  
  std::vector<std::vector<float>> vectors;
  for (uint64_t i = 0; i < 500; ++i) {
    auto vec = random_vector(128);
    vectors.push_back(vec);
    index.insert(i + 100, vec); // ID offset
  }
  
  // For each random query, check if top-1 is reasonable
  int correct = 0;
  for (int trial = 0; trial < 100; ++trial) {
    uint64_t target_idx = trial % 500;
    uint64_t target_id = target_idx + 100;
    
    auto results = index.search(vectors[target_idx], 1);
    
    if (!results.empty() && results[0].id == target_id) {
      ++correct;
    }
  }
  
  // Expect at least 95% recall for exact matches
  EXPECT_GE(correct, 95);
}

}  // namespace innodb_vector_unittest
