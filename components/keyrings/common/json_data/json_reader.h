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

#ifndef JSON_READER_INCLUDED
#define JSON_READER_INCLUDED

#include <memory>
#include <string>
#include <vector>

#define RAPIDJSON_HAS_STDSTRING 1

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>
#include <rapidjson/schema.h>

#include "components/keyrings/common/data/data.h"
#include "components/keyrings/common/data/meta.h"

#include "json_ds.h"

namespace keyring_common {
namespace json_data {

using output_vector =
    std::vector<std::pair<std::pair<meta::Metadata, data::Data>,
                          std::unique_ptr<Json_data_extension>>>;

/**
  Base Json_reader

  Expected format for version 1.0
  {
    "version": "1.0",
    "elements": [
      {
        "user": "<user_name>",
        "data_id": "<name>",
        "data_type": "<data_type>",
        "data": "<hex_of_data>",
        "extension": [
        ]
      },
      ...
      ...
    ]
  }
*/

class Json_reader {
 public:
  Json_reader(const std::string schema, const std::string data,
              const std::string version_key = "version",
              const std::string array_key = "elements");

  Json_reader(const std::string data);

  Json_reader();

  /** Destructor */
  virtual ~Json_reader() = default;

  std::string version() const;

  /**
    Get number of elements in the document

    @returns number elements in the document
  */
  size_t num_elements() const;

  /**
    Fetch element from given position

    @param [in]  index               Element position
    @param [out] metadata            Data identifier
    @param [out] data                Data
    @param [out] json_data_extension Backend specific extension

    @return status of operation
      @retval false Success
      @retval true  Failure
  */
  virtual bool get_element(
      size_t index, meta::Metadata &metadata, data::Data &data,
      std::unique_ptr<Json_data_extension> &json_data_extension) const;

  /**
    Get all elements

    @param [out] output Output vector

    @returns status of extracting elements
      @retval false Success
      @retval true  Failure
  */
  virtual bool get_elements(output_vector &output) const;

  bool valid() const { return valid_; }

 private:
  /** Data in JSON DOM format */
  rapidjson::Document document_;
  /** Version key */
  const std::string version_key_;
  /** user specific elements array key */
  const std::string array_key_;
  /** Validity of the data */
  bool valid_;
};

}  // namespace json_data

}  // namespace keyring_common

#endif  // !JSON_READER_INCLUDED
