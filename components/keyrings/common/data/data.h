/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef DATA_INCLUDED
#define DATA_INCLUDED

#include <string>

namespace keyring_common::data {

/** Data types */
using Type = std::string;
using Sensitive_data = std::string;

/**
  Sensitive data storage
*/

class Data {
 public:
  Data(const Sensitive_data &data, Type type);
  Data();
  explicit Data(Type type);
  Data(const Data &src);
  Data(Data &&src) noexcept;
  Data &operator=(const Data &src);
  Data &operator=(Data &&src) noexcept;

  virtual ~Data();

  virtual Data get_data() const;

  Sensitive_data data() const;

  Type type() const;

  bool valid() const;

  void set_data(const Sensitive_data &data);

  virtual void set_data(const Data &src);

  void set_type(Type type);

  bool operator==(const Data &other) const;

 protected:
  void set_validity();
  /** Sensitive data */
  Sensitive_data data_;
  /** Data type */
  Type type_;
  /** Validity of Data object */
  bool valid_{false};
};

}  // namespace keyring_common::data

#endif  // !DATA_INCLUDED
