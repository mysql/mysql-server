// Copyright 2026 Google LLC

#ifndef _SQL_SCAM_OPTS_H_
#define _SQL_SCAM_OPTS_H_

#include "storage/innobase/include/vector0cfg.h"
#include "sql/key_spec.h"
#include "my_base.h"

bool validate_options(const std::map<std::string, std::string>& opts_map,
                      const std::vector<std::string>& valid_opts,
                      const char* func_name);
bool parse_integer_opts(std::string key, std::string val, int& output,
                        const char* func_name);

bool get_options_map(const char* opts,
                     std::map<std::string, std::string>& opts_map,
                     const char* func_name);

bool get_distance_measure_from_opts(std::map<std::string, std::string> opts_map,
                                    const char* func_name,
                                    ib_vector::DistMeasure& dist_measure);

distance_measure convert_distance_measure(ib_vector::DistMeasure dist_measure);

/** Parse index option string
@param[in]    opts      options string
@param[out]   cfg       VectorCfg: index type
@paraam[in]   func_name name of the function calling this
@return false on success (cfg is populate), true on failure with my_error
appropriately set */
bool parse_vector_index_options(const char* opts, ib_vector::VectorCfg& cfg,
                                const char* func_name);

/** Parse vector distance option string
@param[in]    opts      options string
@param[out]   cfg       VectorCfg configuration
@paraam[in]   func_name name of the function calling this
@return false on success (cfg is populate), true on failure with my_error
appropriately set */
bool parse_vector_distance_options(const char* opts, ib_vector::VectorCfg& cfg,
                                 const char* func_name);

/** Convert distance measure value to enum
@param[in]    value     distance measure string
@return distance_measure distane measure enum */
enum distance_measure get_distance_measure_enum(std::string value);

/** Convert distance measure enum to string
@param[in]    enumValue   distance measure enum
@return distance measure in string format */
std::string get_distance_measure_name(enum distance_measure enumValue);

/** Convert quantizer value to enum
@param[in]    value     quantizer string
@return quantizer enum for the input string*/
enum ha_key_sub_alg get_quantizer_enum(std::string value);

/** Convert quantizer enum to string
@param[in]    enumValue   quantizer enum
@return quantizer in string format */
std::string get_quantizer_name(enum ha_key_sub_alg enumValue);

/** Convert index algorithm enum to string
@param[in]    enumValue   index algorithm enum
@return index algorithm in string format */
std::string get_index_algorithm_name(enum ha_key_alg enumValue);

#else
#define parse_vector_index_options(a, b, c) true
#define parse_vector_distance_options(a, b, c) true
namespace ib_vector {
struct VectorCfg {
  VectorCfg(bool) {}
  VectorCfg() {}
};
}  // namespace ib_vector

#endif  // _SQL_SCAM_OPTS_H_
