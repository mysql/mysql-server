// Copyright 2026 Google LLC

/** @file include/vector0types.h
 Wrapper class to package vector related types */

#ifndef _INCLUDE_VECTOR0TYPES_H_
#define _INCLUDE_VECTOR0TYPES_H_

#include <cstddef>
#include <cstdint>
#include "absl/types/span.h"
#include <string>
#include <vector>

#include "ut0dbg.h"

namespace ib_vector {

/** Crude vector data (per dimension) */
using VectorDataT = float;

/** Vector data after (scalar) quantization */
using QuantizedDataT = int8_t;

/** Data type (precision) used to measure distance between vectors */
using VectorDistT = float;

/** A vector with its original values */
using Vector = std::vector<VectorDataT>;

/** A quantized vector (using scalar - FP8 - quantization) */
using QuantizedVector = std::vector<QuantizedDataT>;

/** A vector using absl::Span representation, i.e., no ownership */
using VectorSpan = absl::Span<VectorDataT>;

/** A const (immutable members) vector span */
using ConstVectorSpan = absl::Span<const VectorDataT>;

/** A span of a quantized vector */
using QuantizedSpan = absl::Span<QuantizedDataT>;

/** A const span of a quantized vector */
using ConstQuantizedSpan = absl::Span<const QuantizedDataT>;

/** Nearest neighbor query result, as a list of PK-dist pairs */
using Neighbors = std::vector<std::pair<std::string, VectorDistT>>;

/** Data type for stream ID, identifying a group of get_neighbor() calls to
    serve a single query */
using StreamIdT = int64_t;

/** Virtual type for query cache, to be expanded by concrete implementations */
struct QueryData {
  Neighbors cached_results;
  void* saved_sub_table_handle = nullptr;
  size_t num_returned_results = 0;
  size_t num_stream_calls = 0;
  virtual ~QueryData() = default;
};

/** Supported KMeans index types */
enum class IndexType : uint8_t {
  /* Partition the data and quantize (compress) is using FP8 (4 bytes float gets
  compressed to int_8). This is default. It is a KMeans Tree of two levels (root
  and leafs). Persistence will be supported. */
  TREE_SQ
};

/** Convert an IndexType to a string
@param[in]          index_type    the index type to convert
@return a string representation of the index type */
inline const char* IndexTypeToString(IndexType index_type) {
  switch (index_type) {
    case IndexType::TREE_SQ:
      return "TREE_SQ";
    default:
      return "UNKNOWN";
  }
}

/** Convert a string to an IndexType
@param[in]          str           the string to convert
@return the IndexType representation of the string */
inline IndexType StringToIndexType(std::string str) {
  if (str == "TREE_SQ") {
    return IndexType::TREE_SQ;
  } else {
    // TODO: add an UNKNOWN value to the enum and let the caller decide.
    ut_error;
  }
}

/** Supported KMeans distance measurement algorithms */
enum class DistMeasure : uint8_t {
  DOT_PRODUCT,
  COSINE,
  L2_SQUARED,
};

/** Convert a DistMeasure to a string
@param[in]          dist_measure  the distance measure to convert
@return a string representation of the distance measure understanble by the
         KMeans library.
@remakrs These strings are !NOT! to be changed, since they are going to be used
         in a proto message consued by the KMeans library. */
inline const char* DistMeasureToString(DistMeasure dist_measure) {
  switch (dist_measure) {
    case DistMeasure::DOT_PRODUCT:
      return "DotProductDistance";
    case DistMeasure::COSINE:
      return "CosineDistance";
    case DistMeasure::L2_SQUARED:
      return "SquaredL2Distance";
    default:
      return "UNKNOWN";
  }
}

/** Convert a DistMeasure to a string as provided by the user.
@param[in]          dist_measure  the distance measure to convert
@return a string representation of the distance measure */
inline const char* DistMeasureToUserString(DistMeasure dist_measure) {
  switch (dist_measure) {
    case DistMeasure::DOT_PRODUCT:
      return "DOT_PRODUCT";
    case DistMeasure::COSINE:
      return "COSINE";
    case DistMeasure::L2_SQUARED:
      return "L2_SQUARED";
    default:
      return "UNKNOWN";
  }
}

/** Convert a string to a DistMeasure
@param[in]          str           the string to convert
@return the DistMeasure representation of the string
@remakrs These strings are !NOT! to be changed, since they are going to be used
         in a proto message consued by the KMeans library. */
inline DistMeasure StringToDistMeasure(std::string str) {
  if (str == "DotProductDistance") {
    return DistMeasure::DOT_PRODUCT;
  } else if (str == "CosineDistance") {
    return DistMeasure::COSINE;
  } else if (str == "SquaredL2Distance") {
    return DistMeasure::L2_SQUARED;
  } else {
    // TODO: add an UNKNOWN value to the enum and let the caller decide.
    ut_error;
  }
}

/** Index states. */
enum IndexState: uint8_t {
  INDEX_READY_TO_USE,
  INDEX_PENDING_TRAIN,
  INDEX_PENDING_BUILD,
  INDEX_PENDING_LOAD,
  INDEX_NOT_USABLE,
};

inline const char* IndexStateToString(IndexState index_state) {
  switch (index_state) {
    case IndexState::INDEX_READY_TO_USE:
      return "INDEX_READY_TO_USE";
    case IndexState::INDEX_PENDING_TRAIN:
      return "INDEX_PENDING_TRAIN";
    case IndexState::INDEX_PENDING_BUILD:
      return "INDEX_PENDING_BUILD";
    case IndexState::INDEX_PENDING_LOAD:
      return "INDEX_PENDING_LOAD";
    case IndexState::INDEX_NOT_USABLE:
      return "INDEX_NOT_USABLE";
    default:
      return "UNKNOWN";
  }
}

/** Return codes from index calls. */
enum index_err_t : uint8_t {
  SUCCESS,
  NOT_PERMITTED,
  NOT_FOUND,
  ALREADY_EXISTS,
  INDEX_EMPTY,
  NOT_ENOUGH_DATA_POINTS,
  NOT_ENOUGH_MEMORY,
  NOT_IMPLEMENTED,
  KMEANS_ERROR,
};

inline const char* IndexErrToString(index_err_t index_err) {
  switch (index_err) {
    case index_err_t::SUCCESS:
      return "SUCCESS";
    case index_err_t::NOT_PERMITTED:
      return "operation not permitted";
    case index_err_t::NOT_FOUND:
      return "entry not found";
    case index_err_t::ALREADY_EXISTS:
      return "entry already exists";
    case index_err_t::INDEX_EMPTY:
      return "index is empty";
    case index_err_t::NOT_ENOUGH_DATA_POINTS:
      return "not enough data points";
    case index_err_t::NOT_ENOUGH_MEMORY:
      return "not enough memory";
    case index_err_t::NOT_IMPLEMENTED:
      return "not implemented";
    case index_err_t::KMEANS_ERROR:
      return "KMeans internal error";
    default:
      return "UNKNOWN";
  }
}

/** To get state of an index for information schema table. */
struct VectorIndexStats {
  std::string name_{""};
  std::string table_name_{""};
  std::string type_{"UNKNOWN"};
  std::string dist_measure_{"UNKNOWN"};
  std::string index_state_{"UNKNOWN"};
  uint32_t dimension_{0};
  uint32_t partitions_{0};
  uint32_t query_partitions_{0};
  uint32_t tree_size_{0};

  uint64_t queries_{0};
  uint64_t mutations_{0};
  std::string status_{"UNKNOWN"};
};

/** To get state of index memory for information schema table. */
struct VectorIndexMemoryInfo {
  bool disabled_{false};
  uint64_t total_memory_{0};
  uint64_t index_memory_{0};
  uint64_t training_memory_{0};
  uint64_t num_loaded_indexes_{0};
};

} /* namespace ib_vector */

#endif /* _INCLUDE_VECTOR0TYPES_H_ */
