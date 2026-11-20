/* Copyright (c) 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "udt_complex.h"

#include <cassert>
#include "my_byteorder.h"

// #include <rpc/xdr.h>

namespace udt_example {

#ifdef LATER
void Complex::serialize(enum xdr_op op, serialized_complex &buffer) {
  {
    XDR xdrs;

    xdrmem_create(&xdrs, &buffer.buffer[0], sizeof(buffer.buffer), op);
    xdr_double(&xdrs, &m_real);
    xdr_double(&xdrs, &m_imaginary);
  }

  void Complex::serialize_from(serialized_complex & buffer) {
    serialize(XDR_DECODE);
  }

  void Complex::serialize_to(serialized_complex & buffer) {
    serialize(XDR_ENCODE);
  }
#endif

  void Complex::serialize_from(const serialized_complex &buffer) {
    assert(sizeof(double) == 8);
    const unsigned char *b = &buffer.buffer[0];

    m_real = float8get(b);
    m_imaginary = float8get(b + 8);
  }

  void Complex::serialize_to(serialized_complex & buffer) {
    assert(sizeof(double) == 8);
    unsigned char *b = &buffer.buffer[0];

    float8store(b, m_real);
    float8store(b + 8, m_imaginary);
  }

  Complex Complex::add(const Complex &a, const Complex &b) {
    Complex result;
    result.m_real = a.m_real + b.m_real;
    result.m_imaginary = a.m_imaginary + b.m_imaginary;
    return result;
  }

  Complex Complex::mul(const Complex &a, const Complex &b) {
    Complex result;
    result.m_real = a.m_real * b.m_real - a.m_imaginary * b.m_imaginary;
    result.m_imaginary = a.m_real * b.m_imaginary + b.m_real * a.m_imaginary;
    return result;
  }

}  // namespace udt_example
