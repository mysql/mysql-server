// Copyright 2026 Google LLC

#include "sql/vector_opts.h"

#include <sys/types.h>

#include <cassert>
#include <cstdio>
#include <exception>
#include <map>
#include <string>

#include "mysqld_error.h"
#include "sql/key_spec.h"
#include "sql/mysqld_cs.h"
#include "sql/options_parser.h"
#include "sql_string.h"
#include "storage/innobase/include/vector0types.h"

using namespace ib_vector;

/** --------------- Options for vector index -------------------
1) There are no required options
2) num_partitions is only allowed if tree type indexes are configured
3) The default is TREE_SQ
4) If index_type is explicitly specified, you can have a tree index starting
for 1000. Provided you have enough entries for prtition training
6) All integer options must be greater than 0. We need to use int as KMeans has
them defined as int */

/** Valid options for search */
static const std::vector<std::string> valid_search_opts = {
    "num_leaves_to_search", "distance_measure"};

static const std::vector<std::string> valid_distance_opts = {"distance_measure"};

/* Valid options for index */
static const std::vector<std::string> valid_index_opts = {
    "num_partitions", "num_neighbors", "index_type", "distance_measure"};

/** Parse integer options
@param[in]    key       option key
@param[in]    val       option value string
@param[out]   output    integer value ouput
@param[in]    func_name name of the function calling this
@return false if output is valid i.e. > 0 integer, true on failure with my_error
set appropriately. */
bool parse_integer_opts(std::string key, std::string val, int& output,
                        const char* func_name) {
  /* std::string functions can throw error. Most notably out_of_range and
  invalid_argument */
  try {
    output = std::stoul(val);
    /* This is because KMeans has defined all these numeric options as int. We
    don't expect to provide a negative value. Zero is also not a valid value. */
    if (output <= 0) {
      my_error(ER_INVALID_OPTION_VALUE, MYF(0), val.c_str(), key.c_str(),
               func_name);
      return true;
    }
  } catch (const std::exception& e) {
    my_error(ER_INVALID_OPTION_VALUE, MYF(0), val.c_str(), key.c_str(),
             func_name);
    return true;
  }
  return false;
}

/** Given an option string, generate a map of options
@param[in]    opts      options string
@param[out]   opts_map  map with option,value pair
@param[in]    func_name name of the function calling this
@return false if all opts are well formed key=value pairs, true on failure with
my_error set appropriately. */
bool get_options_map(const char* opts,
                     std::map<std::string, std::string>& opts_map,
                     const char* func_name) {
  /* Convert the options string to lower case. */
  std::string opts_s(opts);
  std::transform(opts_s.begin(), opts_s.end(), opts_s.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  /* Parse and generate a key, value map */
  String opts_str;
  opts_str.copy(opts_s.data(), opts_s.size(), system_charset_info);
  if (options_parser::parse_string(&opts_str, &opts_map, func_name)) {
    return true;
  }
  return false;
}

/** Validate that options map contains only supported options
@param[in]    opts_map      map with option,value pair
@param[in]    valid_opts    valid options list
@param[in]    func_name     name of the function calling this
@return false if all opts are valid, true on failure with
my_error set appropriately. */
bool validate_options(const std::map<std::string, std::string>& opts_map,
                      const std::vector<std::string>& valid_opts,
                      const char* func_name) {
  /* Validate if we only have the supported options as key. */
  for (const auto& [key, value] : opts_map) {
    if (std::find(valid_opts.begin(), valid_opts.end(), key) ==
        valid_opts.end()) {
      my_error(ER_INVALID_OPTION_KEY, MYF(0), key.c_str(), func_name);
      return true;
    }
  }
  return false;
}

distance_measure convert_distance_measure(DistMeasure dist_measure) {
  switch (dist_measure) {
    case DistMeasure::COSINE:
      return distance_measure::DISTANCE_MEASURE_COSINE;
    case DistMeasure::L2_SQUARED:
      return distance_measure::DISTANCE_MEASURE_L2_SQUARED;
    case DistMeasure::DOT_PRODUCT:
      return distance_measure::DISTANCE_MEASURE_DOT_PRODUCT;
  }
  return distance_measure::DISTANCE_MEASURE_UNSPECIFIED;
}

bool get_distance_measure_from_opts(std::map<std::string, std::string> opts_map,
                                    const char* func_name,
                                    DistMeasure& dist_measure) {
  auto it = opts_map.find("distance_measure");
  if (it != opts_map.end()) {
    auto [key, val] = *it;
    if (val == "cosine") {
      dist_measure = DistMeasure::COSINE;
    } else if (val == "l2_squared") {
      dist_measure = DistMeasure::L2_SQUARED;
    } else if (val == "dot_product") {
      dist_measure = DistMeasure::DOT_PRODUCT;
    } else {
      my_error(ER_INVALID_OPTION_VALUE, MYF(0), val.c_str(), key.c_str(),
               func_name);
      return true;
    }
    return false;
  }
  my_error(ER_INVALID_OPTION_VALUE, MYF(0), "value not found",
           "distance_measure", func_name);
  return true;
}

bool parse_vector_distance_options(const char* opts, VectorCfg& cfg,
                                 const char* func_name) {
  assert(opts);
  assert(func_name);

  std::map<std::string, std::string> opts_map;
  if (get_options_map(opts, opts_map, func_name)) {
    return true;
  }

  /* Validate if we only have the supported options as key. */
  if (validate_options(opts_map, valid_distance_opts, func_name)) {
    return true;
  }

  /* distance measure */
  auto it = opts_map.find("distance_measure");
  if (it != opts_map.end()) {
    auto [key, val] = *it;
    DistMeasure dist_measure;
    if (get_distance_measure_from_opts(opts_map, func_name, dist_measure)) {
      return true;
    }
    cfg.set_cfg(Options::DIST_MEASURE, dist_measure);
  }
    return false;
}

bool parse_vector_index_options(const char* opts, VectorCfg& cfg,
                                const char* func_name) {
  assert(opts);
  assert(func_name);

  std::map<std::string, std::string> opts_map;
  if (get_options_map(opts, opts_map, func_name)) {
    return true;
  }

  /* Validate if we only have the supported options as key. */
  if (validate_options(opts_map, valid_index_opts, func_name)) {
    return true;
  }

  /* index_type */
  auto it = opts_map.find("index_type");
  bool index_type_configured = true;
  if (it != opts_map.end()) {
    auto [key, val] = *it;
    if (val == "tree_sq") {
      cfg.set_cfg(Options::INDEX_TYPE, IndexType::TREE_SQ);
    } else {
      my_error(ER_INVALID_OPTION_VALUE, MYF(0), val.c_str(), key.c_str(),
               func_name);
      return true;
    }
  } else {
    index_type_configured = false;
  }

  /* num_partitions */
  int num_partitions;
  it = opts_map.find("num_partitions");
  /* Partitions are only for trees. Not relevant for brute_force. We don't
  allow partitions to be specified unless it is a tree type index. */
  if (it != opts_map.end()) {
    auto [key, val] = *it;
        if (index_type_configured &&
        (IndexType)cfg[Options::INDEX_TYPE] == IndexType::TREE_SQ) {
      if (parse_integer_opts(key, val, num_partitions, func_name)) {
        return true;
      }
      cfg.set_cfg(Options::NUM_PARTITIONS, num_partitions);
    } else {
      std::string err_msg = "num_partitions is only supported for trees";
      my_error(ER_VECTOR_INDEX_INCOMPATIBLE_OPTION, MYF(0), err_msg.c_str());
      return true;
    }
  }

  /* distance measure */
  it = opts_map.find("distance_measure");
  if (it != opts_map.end()) {
    auto [key, val] = *it;
    if (val == "cosine") {
      cfg.set_cfg(Options::DIST_MEASURE, DistMeasure::COSINE);
    } else if (val == "l2_squared") {
      cfg.set_cfg(Options::DIST_MEASURE, DistMeasure::L2_SQUARED);
    } else if (val == "dot_product") {
      cfg.set_cfg(Options::DIST_MEASURE, DistMeasure::DOT_PRODUCT);
    } else {
      my_error(ER_INVALID_OPTION_VALUE, MYF(0), val.c_str(), key.c_str(),
               func_name);
      return true;
    }
  }
  return false;
}

enum distance_measure get_distance_measure_enum(std::string value) {
  if (value == "L2_SQUARED")
    return distance_measure::DISTANCE_MEASURE_L2_SQUARED;
  else if (value == "COSINE")
    return distance_measure::DISTANCE_MEASURE_COSINE;
  else if (value == "DOT_PRODUCT")
    return distance_measure::DISTANCE_MEASURE_DOT_PRODUCT;
  assert(false);
  return distance_measure::DISTANCE_MEASURE_UNSPECIFIED;
}

std::string get_distance_measure_name(enum distance_measure enumValue) {
  switch (enumValue) {
    case distance_measure::DISTANCE_MEASURE_L2_SQUARED:
      return "L2_SQUARED";
    case distance_measure::DISTANCE_MEASURE_COSINE:
      return "COSINE";
    case distance_measure::DISTANCE_MEASURE_DOT_PRODUCT:
      return "DOT_PRODUCT";
    case distance_measure::DISTANCE_MEASURE_UNSPECIFIED:
    default:
      assert(false);
      return "Unknown";
  }
}

enum ha_key_sub_alg get_quantizer_enum(std::string value) {
  if (value == "SQ8")
    return ha_key_sub_alg::HA_KEY_SUB_ALG_TREE_SQ;
  return ha_key_sub_alg::HA_KEY_SUB_ALG_UNSPECIFIED;
}

std::string get_quantizer_name(enum ha_key_sub_alg enumValue) {
  switch (enumValue) {
    case ha_key_sub_alg::HA_KEY_SUB_ALG_UNSPECIFIED:
      return "UNSPECIFIED";
    case ha_key_sub_alg::HA_KEY_SUB_ALG_TREE_SQ:
      return "SQ8";
    default:
      assert(false);
      return "Unknown";
  }
}

std::string get_index_algorithm_name(enum ha_key_alg enumValue) {
  switch (enumValue) {
      case ha_key_alg::HA_KEY_ALG_SE_SPECIFIC:
        return "SE_SPECIFIC";
      case ha_key_alg::HA_KEY_ALG_BTREE:
        return "BTREE";
      case ha_key_alg::HA_KEY_ALG_RTREE:
        return "RTREE";
      case ha_key_alg::HA_KEY_ALG_HASH:
        return "HASH";
      case ha_key_alg::HA_KEY_ALG_FULLTEXT:
        return "FULLTEXT";
      case ha_key_alg::HA_KEY_ALG_KMEANS:
        return "TREE";
    default:
      assert(false);
      return "Unknown";
  }
}
