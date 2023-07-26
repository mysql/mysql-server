/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <my_rapidjson_size_t.h>

#include <rapidjson/document.h>
#include <list>
#include <memory>
#include "mrs/database/entry/object.h"
#include "mrs/database/helper/object_checksum.h"
#include "router/src/http/src/digest.h"

namespace mrs {
namespace database {

namespace {
struct ChecksumHandler
    : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, ChecksumHandler> {
  explicit ChecksumHandler(std::shared_ptr<entry::Object> object)
      : object_({object}), digest_(Digest::Type::Sha256) {}

  std::list<std::shared_ptr<entry::Object>> object_;
  std::shared_ptr<entry::ObjectField> current_field_;
  Digest digest_;
  int level = 0;

  std::string finalize() {
    std::string res;
    res.resize(digest_.digest_size(Digest::Type::Sha256));
    digest_.finalize(res);

    return res;
  }

  bool Null() {
    if (!current_field_ || current_field_->no_check) return true;
    digest_.update("null");
    return true;
  }

  bool Bool(bool b) {
    if (!current_field_ || current_field_->no_check) return true;
    digest_.update(b ? "true" : "false");
    return true;
  }

  bool Int(int i) {
    if (!current_field_ || current_field_->no_check) return true;
    digest_.update(reinterpret_cast<const char *>(&i), sizeof(i));
    return true;
  }

  bool Uint(unsigned u) {
    if (!current_field_ || current_field_->no_check) return true;
    digest_.update(reinterpret_cast<const char *>(&u), sizeof(u));
    return true;
  }

  bool Int64(int64_t i) {
    if (!current_field_ || current_field_->no_check) return true;
    digest_.update(reinterpret_cast<const char *>(&i), sizeof(i));
    return true;
  }

  bool Uint64(uint64_t u) {
    if (!current_field_ || current_field_->no_check) return true;
    digest_.update(reinterpret_cast<const char *>(&u), sizeof(u));
    return true;
  }

  bool Double(double d) {
    if (!current_field_ || current_field_->no_check) return true;
    digest_.update(reinterpret_cast<const char *>(&d), sizeof(d));
    return true;
  }

  bool String(const char *str, rapidjson::SizeType length,
              [[maybe_unused]] bool copy) {
    if (!current_field_ || current_field_->no_check) return true;
    digest_.update(str, length);
    return true;
  }

  bool StartObject() {
    // Possible cases:
    // - starting the root
    // - starting an object in an array
    // - starting an ignored nested object
    // - starting a valid nested object

    if (current_field_) {
      if (!current_field_->no_check) {
        // ignores everything in this object
        object_.push_back(nullptr);
      } else {
        auto ref =
            std::dynamic_pointer_cast<entry::ReferenceField>(current_field_);
        assert(ref);
        if (!ref)
          throw std::logic_error("JSON object field has unexpected value type");

        object_.push_back(ref->nested_object);
      }
    } else {
      if (object_.size() == 1) {
        // is root
        object_.push_back(object_.back());
      } else {
        // is ignored object
        object_.push_back(nullptr);
      }
    }

    return true;
  }

  bool Key(const char *str, rapidjson::SizeType length,
           [[maybe_unused]] bool copy) {
    auto object = object_.back();
    if (!object) {
      // ignoring object items
      current_field_ = {};
      return true;
    }

    current_field_ = object->get_field({str, length});

    if (current_field_) {
      if (!current_field_->no_check) digest_.update(str, length);
    } else {
      if (strncmp("links", str, length) != 0 &&
          strncmp("_metadata", str, length) != 0) {
        throw std::logic_error("JSON object field not found");
      }
    }

    return true;
  }

  bool EndObject([[maybe_unused]] rapidjson::SizeType memberCount) {
    object_.pop_back();
    current_field_ = {};
    return true;
  }

  bool StartArray() { return true; }

  bool EndArray([[maybe_unused]] rapidjson::SizeType elementCount) {
    return true;
  }
};

std::string string_to_hex(std::string_view s) {
  constexpr char hexmap[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  std::string encoded;

  encoded.reserve(s.size() * 2);

  for (const auto cur_char : s) {
    encoded.push_back(hexmap[(cur_char & 0xF0) >> 4]);
    encoded.push_back(hexmap[cur_char & 0x0F]);
  }

  return encoded;
}

}  // namespace

std::string compute_checksum(std::shared_ptr<entry::Object> object,
                             std::string_view doc) {
  // note: checksum is calculated by following fields in order of appearance in
  // the JSON document, thus it is only suitable for use in documents generated
  // by JsonQueryBuilder, which builds JSON in entry::Object order

  ChecksumHandler handler(object);
  rapidjson::Reader reader;
  rapidjson::MemoryStream ms(doc.data(), doc.length());
  reader.Parse(ms, handler);

  return string_to_hex(handler.finalize());
}

}  // namespace database
}  // namespace mrs
