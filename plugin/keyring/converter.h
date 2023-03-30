/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef PLUGIN_KEYRING_CONVERTER_H_
#define PLUGIN_KEYRING_CONVERTER_H_

#include <stddef.h>
#include <string>

namespace keyring {

/**
  utilities for conversion of keyring file format between machine architectures
 */
class Converter {
 public:
  enum class Arch {
    UNKNOWN,  //!< unable to determine
    LE_32,    //!< little endian, 32 bit
    LE_64,    //!< little endian, 64 bit
    BE_32,    //!< big endian, 32 bit
    BE_64     //!< big endian, 64 bit
  };

  static Arch get_native_arch();
  static size_t get_width(Arch arch);
  static size_t native_value(const char *length);
  static size_t convert(char const *src, char *dst, Arch src_t, Arch dst_t);
  static bool convert_data(char const *data, size_t data_size, Arch src,
                           Arch dst, std::string &out);

 private:
  enum class Endian { UNKNOWN, LITTLE, BIG };
  static Endian get_endian(Arch arch);
  static Arch detect_native_arch();

  const static Arch native_arch;  //!< used for storing native architecture
};

} /* namespace keyring */

#endif /* PLUGIN_KEYRING_CONVERTER_H_ */
