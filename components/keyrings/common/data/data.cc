/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <functional>

#include "data.h"

namespace keyring_common {
namespace data {

/** Constructor to create a data object */
Data::Data(const Sensitive_data data, Type type) : data_(data), type_(type) {
  set_validity();
}

/* Following constructors imply no data */
Data::Data() : Data("", "") {}
Data::Data(Type type) : Data("", type) {}

/** Copy constructor */
Data::Data(const Data &src) : Data(src.data_, src.type_) {}

/** Move constructor */
Data::Data(Data &&src) noexcept {
  std::swap(src.data_, data_);
  std::swap(src.type_, type_);
  std::swap(src.valid_, valid_);
}

/* Assignment operator */
Data &Data::operator=(const Data &src) = default;

Data &Data::operator=(Data &&src) noexcept {
  std::swap(src.data_, data_);
  std::swap(src.type_, type_);
  std::swap(src.valid_, valid_);

  return *this;
}

/** Destructor */
Data::~Data() { valid_ = false; }

/** Return self */
const Data Data::get_data() const { return *this; }

/** Get data */
Sensitive_data Data::data() const { return data_; }

/** Get data's type */
Type Data::type() const { return type_; }

/** Status of object's validity */
bool Data::valid() const { return valid_; }

/** Set data */
void Data::set_data(const Sensitive_data &data) {
  data_ = data;
  set_validity();
}

/** Set data */
void Data::set_data(const Data &src) { *this = src; }

/** Set type */
void Data::set_type(Type type) {
  type_ = type;
  set_validity();
}

/** Comparison */
bool Data::operator==(const Data &other) {
  return data_ == other.data_ && type_ == other.type_ && valid_ == other.valid_;
}

/** Set validity of the data object */
void Data::set_validity() { valid_ = type_ != ""; }

}  // namespace data
}  // namespace keyring_common
