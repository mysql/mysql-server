/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef UDT_COMPLEX_H_INCLUDED
#define UDT_COMPLEX_H_INCLUDED

#include <cstring>

namespace udt_example {

struct serialized_complex {
  unsigned char buffer[16];

  const unsigned char *ptr() { return &buffer[0]; }

  unsigned int length() { return sizeof(buffer); }

  void set(const unsigned char *ptr, unsigned int len) {
    if (len == length()) {
      std::memcpy(&buffer[0], ptr, len);
    }
  }
};

class Complex {
 public:
  Complex() : m_real(0.0), m_imaginary(0.0) {}
  Complex(double r, double i) : m_real(r), m_imaginary(i) {}

  void serialize_from(const serialized_complex &buffer);
  void serialize_to(serialized_complex &buffer);

  static Complex add(const Complex &a, const Complex &b);
  static Complex mul(const Complex &a, const Complex &b);

  double m_real;
  double m_imaginary;
};

}  // namespace udt_example

#endif /* UDT_EXAMPLE_LOG_H_INCLUDED */
