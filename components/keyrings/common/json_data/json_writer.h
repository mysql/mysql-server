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

#ifndef JSON_WRITER_INCLUDED
#define JSON_WRITER_INCLUDED

#include <string>

#define RAPIDJSON_HAS_STDSTRING 1

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "components/keyrings/common/data/data.h"
#include "components/keyrings/common/data/meta.h"

#include "json_ds.h"

namespace keyring_common {
namespace json_data {

class Json_writer {
 public:
  Json_writer(const std::string data = {}, const std::string version = "1.0",
              const std::string version_key = "version",
              const std::string array_key = "elements");

  /** Destructor */
  virtual ~Json_writer() = default;

  bool set_data(const std::string data);

  const std::string to_string() const;

  /**
    Add an element

    @param [in] metadata Data identifier
    @param [in] data     Data

    @returns status of insertion
      @retval false Success
      @retval true  Failure
  */
  virtual bool add_element(const meta::Metadata &metadata,
                           const data::Data &data, Json_data_extension &);

  /**
    Remove an element

    @param [in] metadata Data identifier

    @returns status of removal
      @retval false Success
      @retval true  Failure
  */
  virtual bool remove_element(const meta::Metadata &metadata,
                              const Json_data_extension &);

  /** Number of elements stored */
  size_t num_elements() const;

  /** Validity of the document */
  bool valid() const { return valid_; }

 private:
  /** Data in JSON DOM format */
  rapidjson::Document document_;
  /** Version information */
  const std::string version_key_;
  /** Elements array name */
  const std::string array_key_;
  /** Document validity */
  bool valid_;
};

}  // namespace json_data
}  // namespace keyring_common

#endif  // !JSON_WRITER_INCLUDED
