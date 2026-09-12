// Copyright 2026 Google LLC

#ifndef SCANN_INTERFACE_H_
#define SCANN_INTERFACE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#define KMEANS_API __attribute__((visibility("default")))

namespace kmeans_wrapper {

class KMEANS_API KMeansConfigWrapper {
 public:
  virtual ~KMeansConfigWrapper() = default;
};



// Represents a Datapoint and DatapointPtr
class KMEANS_API DatapointWrapper {
 public:
  virtual ~DatapointWrapper() = default;
  virtual const float *float_values() const = 0;
  virtual float *mutable_float_values() = 0;
  virtual size_t dimensionality() const = 0;
};

// Represents a DenseDataset
class KMEANS_API DatasetWrapper {
 public:
  virtual ~DatasetWrapper() = default;
  virtual void set_dimensionality(int dim) = 0;
  virtual size_t size() const = 0;
  virtual bool Append(DatapointWrapper *dp) = 0;
  virtual bool NormalizeUnitL2() = 0;
};

// Represents a Partitioner
class KMEANS_API PartitionerWrapper {
 public:
  enum TokenizationMode { DATABASE = 0, QUERY = 1 };

  virtual ~PartitionerWrapper() = default;
  virtual KMEANS_API std::unique_ptr<PartitionerWrapper> Clone() const = 0;
  virtual void set_tokenization_mode(TokenizationMode mode) = 0;

  virtual void tokenize(DatapointWrapper *dp, std::vector<int32_t> *parts) = 0;
  virtual void SerializeToString(std::string *out) const = 0;
  virtual int32_t query_spilling_max_centers() const = 0;
};

class KMEANS_API KMeansTreeLikePartitionerWrapper : public PartitionerWrapper {
 public:
  virtual void TokensForDatapointWithSpilling(
      DatapointWrapper *dp, int32_t num_results,
      std::vector<std::pair<uint32_t, float>> *results) = 0;
};

// Factory / Utility functions
KMEANS_API std::unique_ptr<KMeansConfigWrapper> ParseKMeansConfigBinary(
    const uint8_t *proto_bytes, size_t length);

KMEANS_API std::unique_ptr<PartitionerWrapper> CreatePartitioner(
    const uint8_t *partitioner_bytes, size_t length,
    KMeansConfigWrapper *config);
class KMEANS_API ThreadPoolWrapper {
 public:
  virtual ~ThreadPoolWrapper() = default;
};
KMEANS_API std::unique_ptr<ThreadPoolWrapper> CreateThreadPool(int num_threads);

KMEANS_API std::unique_ptr<PartitionerWrapper> TrainPartitioner(
    DatasetWrapper *dataset, KMeansConfigWrapper *config, ThreadPoolWrapper* pool);

KMEANS_API bool MaybeAddTopLevelPartitioner(PartitionerWrapper *part,
                                           const uint8_t *config_bytes,
                                           size_t length);

KMEANS_API std::unique_ptr<DatasetWrapper> CreateDataset();
KMEANS_API std::unique_ptr<DatapointWrapper> CreateDatapointFromPtr(
    const float *values, size_t size);

KMEANS_API float DenseDotProductRaw(DatapointWrapper *q, const int8_t *dp_values, size_t size);
KMEANS_API float SquaredL2Norm(DatapointWrapper *dp);

KMEANS_API std::vector<float> ComputeMaxQuantizationMultipliers(
    DatasetWrapper *dataset);
KMEANS_API void NormalizeUnitL2(DatapointWrapper *dp);
KMEANS_API void ScalarQuantizeFloatDatapoint(const float *src, size_t dim,
                                            const float *multipliers,
                                            int8_t *ret);

}  // namespace kmeans_wrapper

#endif  // SCANN_INTERFACE_H_
