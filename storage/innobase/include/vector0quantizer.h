// Copyright 2026 Google LLC

/** @file include/vector0quantizer.h
 Quantizing vector(s) */

#ifndef _INCLUDE_VECTOR0QUANTIZER_H_
#define _INCLUDE_VECTOR0QUANTIZER_H_

#include "vector0types.h"

#include "absl/types/span.h"

namespace ib_vector {

/** VectorQuantizer is used to quantize vecotr(s), based on the quantization
algorithm. This is generic math computation, irrespect to detailed vector
index type.

Currently supported quantization tye:

Scalar Quantization:
  - Quantize a flat number to fixed point (integer) number.
  - It requires a multiplier vector, which is a vector holding the max value of
    each dimension across all the training data.
  - The input vector is quantized by multiplying the multiplier vector.
  - The output vector is the quantized vector.
  - The dequantized vector is obtained by multiplying the inverse multiplier
    vector.
*/
class VectorQuantizer {
 public:
  // constructor
  explicit VectorQuantizer(IndexType type) : m_type(type) {}

  // setters & getters

  /** Retrieve the quantizer type (algorithm)
  @return the quantization type */
  IndexType type() const { return m_type; }

  /** Retrieve the multipliers
  @return the multipliers */
  ConstVectorSpan multipliers() const {
    return absl::MakeConstSpan(m_multipliers);
  }

  /** Retrieve the inverse multipliers
  @return the multipliers */
  ConstVectorSpan inverse_multipliers() const {
    return absl::MakeConstSpan(m_inv_multipliers);
  }

  /** Set multiplier for scalar quantization
  @param[in]    mult              the multiplier, i.e., the vector that holds
                                  the max value of each dimension, used for
                                  quantization */
  void set_multipliers(ConstVectorSpan mult);

  /** Set inverse_multiplier for scalar quantization
  @param[in]    inv_mult          the inverse multiplier to be used for
                                  dequantization. inv_mult=1/mult */
  void set_inv_multipliers(ConstVectorSpan inv_mult);

  // computation

  /** Quantize a vector
  @param[in]    src               the vector to be quantized
  @param[out]   ret               the quantized vector */
  void quantize(ConstVectorSpan src, QuantizedSpan ret);

  /** Dequantize a vector
  @param[in]    src               the already quantized vector
  @param[out]   ret               the dequantized vector */
  void dequantize(const QuantizedVector& src, Vector& ret);

  /** Inverse quantize a vector
  @param[in,out]    vector        the vector to be inverse quantized
  @remarks Inverse quantization is usually applied on a query vector,
           mainly to cancel out the quantization made on partition data
           points.
               QD1 (quantized datapoint) = M (multipliers) * D1 (datapoint)
               ID2 (inverse quantized) = I (inverse multipliers) * D2
               QD1 * ID2 = D1 * D2, since M * I = 1 */
  void inverse_quantize(VectorSpan& vector) const;

 protected:
  /** Validate if the quantizer has been properly configured
  @return true if necessary configurations are in place for quantization */
  bool is_configured() const;

  /** Backfill the multipliers or inverse multipliers.
  @param[in]    inv               true if backfill inverse multipliers,
                                  false if backfill multipliers */
  void backfill_multipliers(bool inv = true);

  /** Validate the multiplier data
  @return true if the multiplier and versed multipler are correct, i.e,
          m_multiplier = 1 / m_inverse_multiplier */
  bool multipliers_match_test();

  /** Quantize a vector using scalar quantization (float -> int8)
  @param[in]    src               the vector to be quantized
  @param[out]   ret               the quantized vector */
  void quantize_scalar_int8(ConstVectorSpan src, QuantizedSpan ret);

 private:
  /** Type of quantization */
  const IndexType m_type;

  /** The multiplier, which is a vector storing the max value of each
  dimension across all the (training) data. This is used to do scalar
  quantization. */
  Vector m_multipliers;

  /** The inverse multiplier that is used for dequantizing a scalar
  quantized vector. */
  Vector m_inv_multipliers;

  /** Dimensionality of the multipliers and input vectors that this object
  can process. */
  size_t m_dim;
}; /* VectorQuantizer */

} /* namespace ib_vector */

#endif /* _INCLUDE_VECTOR0QUANTIZER_H_ */
