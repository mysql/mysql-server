\page PageLibsMysqlMath Library: Math

<!---
Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.
//
This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms, as
designated in a particular file or component or in included license
documentation. The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.
//
This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
the GNU General Public License, version 2.0, for more details.
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
-->

<!--
MySQL Math Library
==================
-->

Code documentation: @ref GroupLibsMysqlMath.

This is a header-only library, containing mathematical functions. Currently it
has the following headers:

- bounded_arithmetic.h: multiplication and addition, capped at a max value.

- int_pow.h: Functions to compute powers and logarithms for integers
  efficiently:

  - `int_pow(a, b)` computes `a` to the power `b`.
    For constexpr arguments, reduces to a compile-time constant.
    Otherwise, logarithmic in the exponent. The exact number of multiplications
    is equal to the 2-logarithm of the exponent, plus the number of 1-bits in
    the exponent.

  - `int_log<N>(a)` computes the base-`N` logarithm of `a`. For constexpr
    arguments, reduces to a constant. Otherwise, logarithmic in the `base`
    logarithm of std::numeric_limits<Value_t>::max(). The exact number of
    divisions is the floor of `log2(int_log_max<Value_t, base>())` and in each
    division, the denominator is a compile-time constant which is a power of
    `base`, so that the compiler may use denominator-specific optimizations such
    as shift-right instead of division operations.

  - `int_log_max<T>()` reduces to a compile-time constant, which is the floor
    of the logarithm of the maximum value representable in type T.

- summation.h: Two algorithms to compute the sums of sequences: `kahan_sum` uses
  the Kahan summation algorithm to compute the sum of floating-point numbers
  with very low numeric error. `sequence_sum_difference` computes the difference
  of the sums of two sequences of nonnegative integers, guaranteeing exact
  results when the sums are close to each other, even if each sum is huge.
