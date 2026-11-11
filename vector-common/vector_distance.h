/* Copyright (c) 2026, Percona and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#pragma once

#include <cstddef>
#include <cstdint>

/** Set SIMD function pointers before first use; safe to call repeatedly. */
void init_vector_distance_functions();

/**
  SIMD tier identifiers.  Values match the tier table in vector_distance.cc.
  Every tier returns bit-identical results; they differ only in speed.
  Tiers that do not apply to the host platform (e.g. SSE42 on aarch64) are
  always reported as unavailable by vector_distance_tier_available().
*/
enum class VectorDistanceTier {
  Scalar = 0,  ///< plain C++ scalar
  Sse42,       ///< x86_64 — SSE4.2, 128-bit
  Avx2,        ///< x86_64 — AVX2, 256-bit
  Avx512f,     ///< x86_64 — AVX-512F, 512-bit
  Neon,        ///< aarch64 — NEON, 128-bit
};

/**
  Return true when @p tier can be used on the current CPU and build.
  Scalar always returns true. Architecture-specific tiers return false on the
  wrong platform.
*/
bool vector_distance_tier_available(VectorDistanceTier tier);

/**
  Set the dispatch pointers directly to @p tier without call_once protection.
  Intended for tests and benchmarks that want to force a specific tier on a
  host that may support a higher one. Use VectorDistanceTier::Scalar to force
  scalar kernels. Callers must verify vector_distance_tier_available() first.
*/
void init_vector_distance_functions_tier(VectorDistanceTier tier);

/** Tier selected by the last init_vector_distance_functions*() call. */
VectorDistanceTier vector_distance_tier();

/**
  Stable English label for logs/tests, e.g. "AVX-512F", "AVX2",
  "software scalar".
*/
const char *vector_distance_tier_label(VectorDistanceTier tier);

/**
  Write a human-readable dispatch summary into @p buf (NUL-terminated).
  Returns the number of bytes written, excluding the terminating NUL.
*/
size_t vector_distance_dispatch_description(char *buf, size_t buf_len);

/**
  Compute squared Euclidean distance: sum((a[i]-b[i])²).
  No sqrt — intended for ranking and as the shared kernel for SQL EUCLIDEAN
  (where sqrt is applied in Item_func_vector_distance::val_real).
  Accepts any byte alignment.
  Accumulates in float32 lanes and recomputes in double when they may have
  overflowed or underflowed; bit-identical on every tier.
*/
double vector_distance_euclidean_squared(const char *a, const char *b,
                                         uint32_t dims);

/**
  Compute cosine distance between two float vectors encoded as raw bytes.
  Returns +Inf when either vector is all-zeros (undefined cosine — true
  zero-denominator); returns NaN when input elements are NaN/Inf (bad-data
  propagation). The caller must distinguish these two cases.
  Other results are clamped to [0, 2].
  Accumulates in float32 lanes and recomputes in double when they may have
  overflowed or underflowed; bit-identical on every tier.
*/
double vector_distance_cosine(const char *a, const char *b, uint32_t dims);

/**
  Compute dot product (inner product) between two float vectors encoded as raw
  bytes. Returns sum(a[i]*b[i]). Higher values indicate greater similarity.
  Always finite for finite inputs — no NaN edge cases.
  Accumulates in float32 lanes and recomputes in double when they may have
  overflowed or underflowed; bit-identical on every tier.
*/
double vector_distance_dot(const char *a, const char *b, uint32_t dims);

/**
  Compute Manhattan (L1) distance between two float vectors encoded as raw
  bytes. Returns sum(|a[i] - b[i]|). Always >= 0 for finite inputs. No NaN
  edge cases.
  Accumulates in float32 lanes and recomputes in double when they may have
  overflowed or underflowed; bit-identical on every tier.
*/
double vector_distance_manhattan(const char *a, const char *b, uint32_t dims);
