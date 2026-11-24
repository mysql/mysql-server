// Copyright 2026 Google LLC

/** @file include/kmeans0types.h
 Wrapper class to package KMeans related types */

#ifndef _INCLUDE_KMEANS0TYPES_H_
#define _INCLUDE_KMEANS0TYPES_H_

#include <algorithm>
#include <limits>
#include <vector>

#include <iostream>
#include <sstream>
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "kmeans_interface/scann_interface.h"
#include "scann/proto/scann.pb.h"
#include "ut0log.h"

namespace ib_vector {
namespace thread {
  struct Options {};
}

class ThreadPool {
 public:
  struct Options {
    std::string name_prefix;
    thread::Options thread_options;
  };
  ThreadPool(int num_threads, Options opts) {
    impl_ = kmeans_wrapper::CreateThreadPool(num_threads);
  }
  std::unique_ptr<kmeans_wrapper::ThreadPoolWrapper> impl_;
};

struct SingleMachineFactoryOptions {
  std::shared_ptr<ThreadPool> parallelization_pool;
};

using PartitionIdT = int32_t;
using Partitions = std::vector<PartitionIdT>;
template <typename DistT, typename KeyT>
// Note: We maintain this localized implementation rather than importing
// KMeans's native `REPLACE_WITH_RESEARCH_KMEANS::FastTopNeighbors` template directly.
// Importing the KMeans priority queue would require `#include`ing its headers,
// violating the PIMPL boundary and leaking Abseil/TF dependencies into InnoDB.
struct FastTopNeighbors {
  size_t max_neighbors_{10};

  struct Element {
    KeyT key;
    DistT dist;
  };

  std::vector<Element> heap_;

  struct Mutator {
    FastTopNeighbors *parent_{nullptr};

    bool PushNoEpsilonCheck(const KeyT &key, DistT dist) {
      if (!parent_) return false;
      if (parent_->heap_.size() < parent_->max_neighbors_) {
        parent_->heap_.push_back({key, dist});
        std::push_heap(
            parent_->heap_.begin(), parent_->heap_.end(),
            [](const Element &a, const Element &b) { return a.dist < b.dist; });
      } else if (!parent_->heap_.empty() &&
                 dist < parent_->heap_.front().dist) {
        std::pop_heap(
            parent_->heap_.begin(), parent_->heap_.end(),
            [](const Element &a, const Element &b) { return a.dist < b.dist; });
        parent_->heap_.back() = {key, dist};
        std::push_heap(
            parent_->heap_.begin(), parent_->heap_.end(),
            [](const Element &a, const Element &b) { return a.dist < b.dist; });
      }
      return true;
    }
    void GarbageCollect() {}
    DistT epsilon() const {
      if (!parent_ || parent_->heap_.size() < parent_->max_neighbors_) {
        return std::numeric_limits<DistT>::infinity();
      }
      return parent_->heap_.front().dist;
    }
    void Release() {}
  };

  void Init(size_t max_n) {
    max_neighbors_ = max_n ? max_n : 10;
    heap_.clear();
  }
  void AcquireMutator(Mutator *m) {
    if (m) m->parent_ = this;
  }
  Mutator GetMutator() { return Mutator{this}; }
  std::vector<std::pair<KeyT, DistT>> TakeUnsorted() { ut_error; }
  std::vector<std::pair<KeyT, DistT>> TakeSorted() { ut_error; }
  std::pair<std::vector<KeyT>, std::vector<DistT>> FinishSorted() {
    std::sort(
        heap_.begin(), heap_.end(),
        [](const Element &a, const Element &b) { return a.dist < b.dist; });
    std::vector<KeyT> keys;
    std::vector<DistT> dists;
    keys.reserve(heap_.size());
    dists.reserve(heap_.size());
    for (const auto &elem : heap_) {
      keys.push_back(elem.key);
      dists.push_back(elem.dist);
    }
    return {keys, dists};
  }
};
struct KMeansQueryData : public QueryData {
  Partitions cached_parts;
};
struct Partitioner {
  std::unique_ptr<kmeans_wrapper::PartitionerWrapper> impl_;
  Partitioner() = default;
  explicit Partitioner(std::unique_ptr<kmeans_wrapper::PartitionerWrapper> impl)
      : impl_(std::move(impl)) {}
  virtual ~Partitioner() = default;
  kmeans_wrapper::PartitionerWrapper *get_impl() const { return impl_.get(); }
  std::unique_ptr<Partitioner> Clone() const {
    if (!impl_) return std::unique_ptr<Partitioner>(new Partitioner());
    return std::unique_ptr<Partitioner>(new Partitioner(impl_->Clone()));
  }

  void set_tokenization_mode(
      kmeans_wrapper::PartitionerWrapper::TokenizationMode mode) {
    if (impl_) {
      impl_->set_tokenization_mode(mode);
    }
  }

  void SerializeToString(std::string *out) const {
    if (impl_ && out) {
      impl_->SerializeToString(out);
    }
  }
};
template <typename T>
struct TreeBruteForceSecondLevelWrapper : public Partitioner {
  explicit TreeBruteForceSecondLevelWrapper(
      std::unique_ptr<kmeans_wrapper::PartitionerWrapper> impl)
      : Partitioner(std::move(impl)) {}
  std::shared_ptr<Partitioner> base() const {
    return std::shared_ptr<Partitioner>(this->Clone().release());
  }
};
struct Dataset {
  std::unique_ptr<kmeans_wrapper::DatasetWrapper> impl_;
  Dataset() {
    impl_ = kmeans_wrapper::CreateDataset();
  }
  struct Status {
    bool ok() const { return true; }
  };
  Status NormalizeUnitL2() {
    if (impl_) impl_->NormalizeUnitL2();
    return Status{};
  }
  absl::Status Append(kmeans_wrapper::DatapointWrapper* dp) {
    if (impl_) {
      impl_->Append(dp);
    }
    return absl::OkStatus();
  }
  size_t size() const { return impl_ ? impl_->size() : 0; }
  void set_dimensionality(int dim) {
    if (impl_) impl_->set_dimensionality(dim);
  }
  kmeans_wrapper::DatasetWrapper *get_impl() const { return impl_.get(); }
};

using VectorDatapoint = std::unique_ptr<kmeans_wrapper::DatapointWrapper>;
using VectorDatapointPtr = kmeans_wrapper::DatapointWrapper*;

inline std::vector<float> ComputeMaxQuantizationMultipliers(
    const Dataset &dataset) {
  if (dataset.get_impl()) {
    return kmeans_wrapper::ComputeMaxQuantizationMultipliers(dataset.get_impl());
  }
  return {};
}
using UntypedPartitioner = kmeans_wrapper::PartitionerWrapper;

using KMeansConfig = research_scann::ScannConfig;
using PartitioningConfig = research_scann::PartitioningConfig;
using VectorDataT = float;

template <typename T, typename C>
inline absl::Status MaybeAddTopLevelPartitioner(T &p, const C &c) {
  std::vector<uint8_t> cfg_buf(c.ByteSizeLong());
  c.SerializeToArray(cfg_buf.data(), cfg_buf.size());
  if (!kmeans_wrapper::MaybeAddTopLevelPartitioner(p->get_impl(), cfg_buf.data(),
                                                 cfg_buf.size())) {
    return absl::InternalError("Failed to add top level partitioner");
  }
  return absl::OkStatus();
}
} /* namespace ib_vector */

#endif /* _INCLUDE_KMEANS0TYPES_H_ */
