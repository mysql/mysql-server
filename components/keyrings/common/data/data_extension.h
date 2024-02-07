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

#ifndef Data_extension_INCLUDED
#define Data_extension_INCLUDED

#include "data.h" /* data::Data */

namespace keyring_common {

namespace data {
/**
  Data wrapper to include backend specific extensions
*/

template <typename Extension>
class Data_extension final : public Data {
 public:
  /** Constructor */
  Data_extension(const data::Data data, const Extension ext)
      : data::Data(data), ext_(ext) {}
  Data_extension() : Data_extension(data::Data{}, Extension{}) {}
  Data_extension(const data::Data data) : Data_extension(data, Extension{}) {}
  Data_extension(const Extension ext) : Data_extension(data::Data{}, ext) {}

  /** Copy constructor */
  Data_extension(const Data_extension &src)
      : Data_extension(src.get_data(), src.ext_) {}

  /** Assignment operator */
  Data_extension &operator=(const Data_extension &src) {
    Data::operator=(src);
    ext_ = src.ext_;

    return *this;
  }

  /* Get data */
  const data::Data get_data() const override { return Data::get_data(); }

  /* Get Metadata extension details */
  const Extension get_extension() const { return ext_; }

  /* Set data */
  void set_data(const data::Data &data) override { Data::operator=(data); }
  /* Set metadata extension */
  void set_extension(const Extension ext) { ext_ = ext; }

 private:
  /** Backend specific extensions */
  Extension ext_;
};

}  // namespace data

}  // namespace keyring_common

#endif  // !Data_extension_INCLUDED
