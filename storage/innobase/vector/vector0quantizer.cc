// Copyright 2026 Google LLC

/** @file vector/vector0quantizer.cc
 Quantizing vector(s) */

#include <cstddef>
#include <cstdint>
#include <cmath>

#include <vector0types.h>
/* Chose to use KMeans's quantization API here for a few reasons:
     1. the VectorQuantizer interface does not have namespace polution
     2. the KMeans's implementation has rich features and has been fine tuned
        for performance
     3. there is no other generic lib in google3
     4. we use static link so memory footprint is not big issue */
#include <kmeans0types.h>
#include <vector0quantizer.h>
#include "kmeans_interface/scann_interface.h"

#include "include/mysqld_error.h"
#include "ut0dbg.h"
#include "ut0log.h"

namespace ib_vector {

/** Set multiplier for scalar quantization
@param[in]      mult              the multiplier, i.e., the vector that holds
                                  the max value of each dimension, used for
                                  quantization */
void VectorQuantizer::set_multipliers(ConstVectorSpan mult) {
  ut_a(!mult.empty());
  m_dim = mult.size();
  m_multipliers.assign(mult.begin(), mult.end());
  backfill_multipliers(true /* backfill inv_multipliers */);
  ut_ad(multipliers_match_test());
}

/** Set inverse_multiplier for scalar quantization
@param[in]      inv_mult          the inverse multiplier to be used for
                                  dequantization. inv_mult=1/mult */
void VectorQuantizer::set_inv_multipliers(ConstVectorSpan inv_mult) {
  ut_a(!inv_mult.empty());
  m_dim = inv_mult.size();
  m_inv_multipliers.assign(inv_mult.begin(), inv_mult.end());
  backfill_multipliers(false /* backfill multipliers */);
  ut_ad(multipliers_match_test());
}

/** Quantize a vector
@param[in]      src               the vector to be quantized
@param[out]     ret               the quantized vector */
void VectorQuantizer::quantize(ConstVectorSpan src, QuantizedSpan ret) {
  ut_a(m_dim == src.size());
  ut_a(m_dim == ret.size());
  ut_a(is_configured());

  switch (m_type) {
  case IndexType::TREE_SQ :
    quantize_scalar_int8(src, ret);
    break;
  default :
    ut_a(false);
    break;
  }
}

/** Dequantize a vector
@param[in]      src               the already quantized vector
@param[out]     ret               the dequantized vector */
void VectorQuantizer::dequantize(const QuantizedVector& src, Vector& ret) {
}

void VectorQuantizer::inverse_quantize(VectorSpan& vector) const {
  ut_a(m_dim == vector.size());
  ut_a(is_configured());
  for (size_t i = 0; i < m_dim; i++) {
    vector[i] *= m_inv_multipliers[i];
  }
}

/** Validate if the quantizer has been properly configured
@return true if necessary configurations are in place for quantization */
bool VectorQuantizer::is_configured() const{
  switch (m_type) {
  case IndexType::TREE_SQ :
    return !m_multipliers.empty() && !m_inv_multipliers.empty();
  default :
    return true;
  }
}

/** Backfill the multipliers or inverse multipliers.
@param[in]      inv               true if backfill inverse multipliers,
                                  false if backfill multipliers */
void VectorQuantizer::backfill_multipliers(bool inv) {
  auto src = (inv) ? &m_multipliers : &m_inv_multipliers;
  auto tgt = (inv) ? &m_inv_multipliers : &m_multipliers;

  size_t dim = src->size();
  ut_a(dim);

  tgt->clear();
  tgt->reserve(dim);

  for (size_t i = 0; i < dim; i++) {
    tgt->emplace_back(1.0f / (*src)[i]);
  }
}

/** Validate the multiplier data
@return true if the multiplier and versed multipler are correct, i.e,
        m_multiplier = 1 / m_inverse_multiplier */
bool VectorQuantizer::multipliers_match_test() {
  size_t dim = m_multipliers.size();
  size_t inv_dim = m_inv_multipliers.size();

  // floating point number encoding:
  //   (-1)_{sign} * (fraction * 2^{-23}) * 2^{exponent}
  // therefore the smallest number, i.e., loss of accuracy would be
  //   (0x800000) * 2^(-23) * 2^(1-127) ~= 0.000000152765017
  const float err = 0.0000001f;
  bool match = false;
  if (dim == inv_dim) {
    for (size_t i = 0; i < dim; i++) {
      if (std::fabs(m_multipliers[i] * m_inv_multipliers[i] - 1.0f) > err) {
        match = false;
        break;
      }
    }
    match = true;
  }

  return match;
}

/** Quantize a vector using scalar quantization (float -> int8)
@param[in]      src               the vector to be quantized
@param[out]     ret               the quantized vector */
void VectorQuantizer::quantize_scalar_int8(ConstVectorSpan src,
                                           QuantizedSpan ret) {
  kmeans_wrapper::ScalarQuantizeFloatDatapoint(src.data(), m_dim,
                                              m_multipliers.data(), ret.data());
}

} /* namespace ib_vector */

