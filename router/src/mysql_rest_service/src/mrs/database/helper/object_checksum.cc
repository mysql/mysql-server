/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include <my_rapidjson_size_t.h>
#include <rapidjson/document.h>

#include "helper/json/rapid_json_to_text.h"
#include "mrs/database/entry/object.h"
#include "mrs/database/helper/object_checksum.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

Sha256Digest::Sha256Digest() : digest_(Digest::Type::Sha256) {}

void Sha256Digest::update(std::string_view data) {
  digest_.update(data.data(), data.size());
  all.append(data);
}

std::string Sha256Digest::finalize() {
  std::string res;
  res.resize(digest_.digest_size(Digest::Type::Sha256));
  digest_.finalize(res);
  return res;
}

namespace {

class JsonCopyBuilder {
 public:
  JsonCopyBuilder() { doc_.SetObject(); }

  void on_field(const std::string &name, rapidjson::Value value,
                std::string_view) {
    if (skip_depth_ > 0) return;

    std::get<2>(stack_.back())
        .AddMember(make_string(name), value, doc_.GetAllocator());
  }

  void on_elem(rapidjson::Value value, std::string_view) {
    if (skip_depth_ > 0) return;

    std::get<2>(stack_.back()).PushBack(value, doc_.GetAllocator());
  }

  void on_start_object(std::shared_ptr<entry::ObjectField> field,
                       bool enabled) {
    if (skip_depth_ > 0) {
      skip_depth_++;
      return;
    }
    if (!enabled) {
      skip_depth_ = 1;
      return;
    }

    rapidjson::Value object(rapidjson::kObjectType);

    if (field)
      stack_.emplace_back(make_string(field->name), field, std::move(object));
    else
      stack_.emplace_back(rapidjson::Value(), field, std::move(object));
  }

  void on_start_array_object(bool enabled) {
    if (skip_depth_ > 0) {
      skip_depth_++;
      return;
    }
    if (!enabled) {
      skip_depth_ = 1;
      return;
    }
    rapidjson::Value object(rapidjson::kObjectType);

    stack_.emplace_back(rapidjson::Value(), std::get<1>(stack_.back()),
                        std::move(object));
  }

  void on_start_literal_object(std::string_view field, bool enabled) {
    if (skip_depth_ > 0) {
      skip_depth_++;
      return;
    }
    if (!enabled) {
      skip_depth_ = 1;
      return;
    }
    rapidjson::Value object(rapidjson::kObjectType);

    stack_.emplace_back(make_string(field), nullptr, std::move(object));
  }

  void on_end_object() {
    if (skip_depth_ > 0) {
      skip_depth_--;
      return;
    }
    assert(!stack_.empty());

    auto object = std::move(stack_.back());
    stack_.pop_back();

    if (stack_.empty()) {
      doc_.Swap(std::get<2>(object));
    } else {
      auto &parent = stack_.back();

      if (std::get<2>(parent).IsObject())
        std::get<2>(parent).AddMember(std::get<0>(object), std::get<2>(object),
                                      doc_.GetAllocator());
      else
        std::get<2>(parent).PushBack(std::get<2>(object), doc_.GetAllocator());
    }
  }

  void on_start_array(std::shared_ptr<entry::ObjectField> field, bool enabled) {
    if (skip_depth_ > 0) {
      skip_depth_++;
      return;
    }
    if (!enabled) {
      skip_depth_ = 1;
      return;
    }
    rapidjson::Value array(rapidjson::kArrayType);

    if (field)
      stack_.emplace_back(make_string(field->name), field, std::move(array));
    else
      stack_.emplace_back(rapidjson::Value(), field, std::move(array));
  }

  void on_start_literal_array(std::string_view field, bool enabled) {
    if (skip_depth_ > 0) {
      skip_depth_++;
      return;
    }
    if (!enabled) {
      skip_depth_ = 1;
      return;
    }
    rapidjson::Value array(rapidjson::kArrayType);

    stack_.emplace_back(make_string(field), nullptr, std::move(array));
  }

  void on_end_array() {
    if (skip_depth_ > 0) {
      skip_depth_--;
      return;
    }
    assert(!stack_.empty());

    auto object = std::move(stack_.back());
    stack_.pop_back();

    assert(!stack_.empty());
    auto &parent = stack_.back();

    if (std::get<2>(parent).IsObject())
      std::get<2>(parent).AddMember(std::get<0>(object), std::get<2>(object),
                                    doc_.GetAllocator());
    else
      std::get<2>(parent).PushBack(std::get<2>(object), doc_.GetAllocator());
  }

  void swap(rapidjson::Document *doc) { doc_.Swap(*doc); }

  rapidjson::Value make_string(std::string_view s) {
    rapidjson::Value value;
    value.SetString(s.data(), s.length(), doc_.GetAllocator());
    return value;
  }

 private:
  rapidjson::Document doc_;
  std::list<std::tuple<rapidjson::Value, std::shared_ptr<entry::ObjectField>,
                       rapidjson::Value>>
      stack_;
  int skip_depth_ = 0;
};

class ChecksumBuilder {
 public:
  explicit ChecksumBuilder(IDigester *digest) : digest_(digest) {}

  void on_field(const std::string &name, const rapidjson::Value &value,
                std::string_view data) {
    log_debug("on-field:%s skip_depth:%i", name.c_str(), (int)skip_depth_);
    if (skip_depth_ > 0) {
      return;
    }
    if (value.IsString()) {
      digest_->update("\"");
      digest_->update(name);
      digest_->update("\":\"");
      digest_->update(data);
      digest_->update("\"");
    } else {
      digest_->update("\"");
      digest_->update(name);
      digest_->update("\":");
      digest_->update(data);
    }
  }

  void on_elem(const rapidjson::Value &value, std::string_view data) {
    log_debug("on-elem skip_depth:%i", (int)skip_depth_);
    if (skip_depth_ > 0) return;
    if (value.IsString()) {
      digest_->update("\"");
      digest_->update(data);
      digest_->update("\"");
    } else {
      digest_->update(data);
    }
  }

  void on_start_object(std::shared_ptr<entry::ObjectField> field,
                       bool enabled) {
    log_debug("on-start_obj skip_depth:%i enabled:%s", (int)skip_depth_,
              (enabled ? "true" : "false"));
    if (skip_depth_ > 0) {
      skip_depth_++;
      return;
    }
    if (!enabled) {
      skip_depth_ = 1;
      return;
    }
    if (field) {
      digest_->update("\"");
      digest_->update(field->name);
      digest_->update("\":{");
    } else {
      digest_->update("{");
    }
  }

  void on_start_array_object(bool enabled) {
    log_debug("on-start_arr skip_depth:%i enabled:%s", (int)skip_depth_,
              (enabled ? "true" : "false"));
    if (skip_depth_ > 0) {
      skip_depth_++;
      return;
    }
    if (!enabled) {
      skip_depth_ = 1;
      return;
    }
    digest_->update("{");
  }

  void on_start_literal_object(std::string_view field, bool enabled) {
    log_debug("on-start_lit_obj skip_depth:%i enabled:%s", (int)skip_depth_,
              (enabled ? "true" : "false"));
    if (skip_depth_ > 0) {
      skip_depth_++;
      return;
    }
    if (!enabled) {
      skip_depth_ = 1;
      return;
    }
    if (field.empty()) {
      digest_->update("{");
    } else {
      digest_->update("\"");
      digest_->update(field);
      digest_->update("\":{");
    }
  }

  void on_end_object() {
    log_debug("on-end_obj skip_depth:%i", (int)skip_depth_);
    if (skip_depth_ > 0) {
      skip_depth_--;
      return;
    }
    digest_->update("}");
  }

  void on_start_array(std::shared_ptr<entry::ObjectField> field, bool enabled) {
    log_debug("on-start_array skip_depth:%i enabled:%s", (int)skip_depth_,
              (enabled ? "true" : "false"));
    if (skip_depth_ > 0) {
      skip_depth_++;
      return;
    }
    if (!enabled) {
      skip_depth_ = 1;
      return;
    }
    if (field) {
      digest_->update("\"");
      digest_->update(field->name);
      digest_->update("\":[");
    } else {
      digest_->update("[");
    }
  }

  void on_start_literal_array(std::string_view field, bool enabled) {
    log_debug("on-start_lit_array skip_depth:%i enabled:%s", (int)skip_depth_,
              (enabled ? "true" : "false"));

    if (skip_depth_ > 0) {
      skip_depth_++;
      return;
    }
    if (!enabled) {
      skip_depth_ = 1;
      return;
    }
    if (field.empty()) {
      digest_->update("[");
    } else {
      digest_->update("\"");
      digest_->update(field);
      digest_->update("\":[");
    }
  }

  void on_end_array() {
    log_debug("on-end_array skip_depth:%i", (int)skip_depth_);
    if (skip_depth_ > 0) {
      skip_depth_--;
      return;
    }
    digest_->update("]");
  }

 private:
  int skip_depth_ = 0;

  IDigester *digest_;
};

class PathTracker {
 public:
  PathTracker() = delete;

  PathTracker(const char separator, bool no_root)
      : separator_(separator), no_root_(no_root) {
    if (!no_root) path_.push_back(separator_);
  }

  bool empty() const {
    return path_.empty() || (path_.size() == 1 && !no_root_);
  }

  std::string_view path() const { return path_; }

  std::string_view current() const {
    auto last = path_.rfind(separator_);
    if (last == std::string::npos || last == 0) return path_;
    return std::string_view{path_.data() + last + 1};
  }

  std::string_view prefix() const {
    auto last = path_.rfind(separator_);
    if (last == std::string::npos || last == 0) return "";
    return std::string_view{path_.data(), last};
  }

  PathTracker &pushd(std::string_view elem) {
    assert(!elem.empty());
    assert(elem.find(separator_) == std::string::npos);

    if (!empty()) path_.push_back(separator_);
    path_.append(elem);

    return *this;
  }

  PathTracker &popd() {
    if (empty()) throw std::logic_error("empty path");

    auto last = path_.rfind(separator_);
    if (last == std::string::npos || last == 0)
      path_.clear();
    else
      path_.resize(last);

    return *this;
  }

 private:
  const char separator_;
  const bool no_root_;

  std::string path_;
};

struct ChecksumHandler
    : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, ChecksumHandler> {
  enum class ContainerType { OBJECT, ARRAY };

  ChecksumHandler(std::shared_ptr<entry::Object> object, IDigester *digest)
      : object_({object}), path_('.', true), digest_(digest) {}

  // creates a filtered copy of the traversed object
  ChecksumHandler(std::shared_ptr<entry::Object> object,
                  const ObjectFieldFilter *filter, IDigester *digest)
      : object_({object}), path_('.', true), digest_(digest), filter_(filter) {}

  // {object, is_array}
  std::list<std::shared_ptr<entry::Object>> object_;
  std::shared_ptr<entry::ObjectField> current_field_;
  bool current_field_is_builtin_ = false;
  PathTracker path_;
  std::list<ContainerType> context_;

  JsonCopyBuilder copy_;
  ChecksumBuilder digest_;

  std::optional<std::string> json_literal_field_;
  size_t json_literal_nesting_ = 0;
  bool json_literal_nocheck_ = false;
  bool json_literal_include_ = true;

  const ObjectFieldFilter *filter_ = nullptr;

  void swap(rapidjson::Document *doc) { copy_.swap(doc); }

  bool Null() {
    push_value(rapidjson::Value(rapidjson::kNullType), {"null", 4});

    return true;
  }

  bool Bool(bool b) {
    push_value(
        rapidjson::Value(b ? rapidjson::kTrueType : rapidjson::kFalseType),
        {b ? "true" : "false", b ? 4U : 5U});
    return true;
  }

  bool Int(int i) {
    rapidjson::Value value;
    value.Set(i);
    push_value(std::move(value),
               {reinterpret_cast<const char *>(&i), sizeof(i)});
    return true;
  }

  bool Uint(unsigned u) {
    rapidjson::Value value;
    value.Set(u);
    push_value(std::move(value),
               {reinterpret_cast<const char *>(&u), sizeof(u)});
    return true;
  }

  bool Int64(int64_t i) {
    rapidjson::Value value;
    value.Set(i);
    push_value(std::move(value),
               {reinterpret_cast<const char *>(&i), sizeof(i)});
    return true;
  }

  bool Uint64(uint64_t u) {
    rapidjson::Value value;
    value.Set(u);
    push_value(std::move(value),
               {reinterpret_cast<const char *>(&u), sizeof(u)});
    return true;
  }

  bool Double(double d) {
    rapidjson::Value value;
    value.Set(d);
    push_value(std::move(value),
               {reinterpret_cast<const char *>(&d), sizeof(d)});
    return true;
  }

  bool String(const char *str, rapidjson::SizeType length,
              [[maybe_unused]] bool copy) {
    std::string_view tmp{str, length};
    push_value(copy_.make_string(tmp), tmp);
    return true;
  }

  bool StartObject() {
    ContainerType parent_type =
        context_.empty() ? ContainerType::OBJECT : context_.back();
    context_.push_back(ContainerType::OBJECT);

    // Possible cases:
    // - starting the root
    // - starting an object in an array
    // - starting an ignored nested object
    // - starting a valid nested object

    if (json_literal_nesting_ > 0) {
      if (parent_type == ContainerType::ARRAY) {
        copy_.on_start_literal_object("", include_field());
        digest_.on_start_literal_object("", check_field());
      } else {
        copy_.on_start_literal_object(json_literal_field_.value_or(""),
                                      include_field());
        digest_.on_start_literal_object(json_literal_field_.value_or(""),
                                        check_field());
      }
      json_literal_nesting_++;
      return true;
    }

    if (current_field_) {
      auto ref =
          std::dynamic_pointer_cast<entry::ReferenceField>(current_field_);
      if (!ref) {
        // we can have an Object in a plain data field if the field is for a
        // JSON type column
        json_literal_nesting_++;

        json_literal_nocheck_ = current_field_->no_check;
        json_literal_include_ = current_field_->enabled;

        copy_.on_start_literal_object(current_field_->name, include_field());
        digest_.on_start_literal_object(current_field_->name, check_field());
      } else {
        object_.push_back(ref->nested_object);

        copy_.on_start_object(ref, include_field());
        digest_.on_start_object(ref, check_field());

        path_.pushd(current_field_->name);
      }
      current_field_ = {};
    } else if (current_field_is_builtin_) {
      json_literal_nesting_++;

      json_literal_nocheck_ = true;
      json_literal_include_ = true;
      copy_.on_start_literal_object(*json_literal_field_, include_field());
      digest_.on_start_literal_object(*json_literal_field_, check_field());
    } else {
      if (object_.size() == 1) {
        // root
        copy_.on_start_object(nullptr, true);
        digest_.on_start_object(nullptr, true);
      } else {
        // nested object list
        copy_.on_start_array_object(include_field());
        digest_.on_start_array_object(check_field());
      }
    }

    return true;
  }

  bool Key(const char *str, rapidjson::SizeType length,
           [[maybe_unused]] bool copy) {
    current_field_is_builtin_ = false;
    current_field_ = {};

    if (json_literal_nesting_ > 0) {
      json_literal_field_ = {str, length};
      return true;
    }
    if (is_builtin_field({str, length})) {
      current_field_is_builtin_ = true;
      json_literal_field_ = {str, length};
      return true;
    }

    auto object = object_.back();
    if (!object) {
      assert(0);
      // ignore literal object items
      current_field_ = {};
      return true;
    }

    current_field_ = object->get_field({str, length});
    if (!current_field_) {
      throw std::logic_error(std::string("JSON object field '")
                                 .append(str, length)
                                 .append("' not found"));
    }

    return true;
  }

  bool EndObject([[maybe_unused]] rapidjson::SizeType memberCount) {
    context_.pop_back();

    copy_.on_end_object();
    digest_.on_end_object();

    current_field_ = {};

    if (json_literal_nesting_ > 0) {
      json_literal_nesting_--;
      json_literal_field_ = {};
      return true;
    }

    // if we're ending an object inside an array, then don't pop the context
    // stacks because all objects in the array are expected to have the same
    // type
    auto current = (context_.empty() ? ContainerType::OBJECT : context_.back());
    if (current != ContainerType::ARRAY) {
      object_.pop_back();

      if (!path_.empty()) path_.popd();
    }

    return true;
  }

  bool StartArray() {
    assert(!context_.empty());
    ContainerType parent_type = context_.back();
    context_.push_back(ContainerType::ARRAY);

    if (json_literal_nesting_ > 0) {
      if (parent_type == ContainerType::ARRAY) {
        copy_.on_start_literal_array("", include_field());
        digest_.on_start_literal_array("", check_field());
      } else {
        copy_.on_start_literal_array(json_literal_field_.value_or(""),
                                     include_field());
        digest_.on_start_literal_array(json_literal_field_.value_or(""),
                                       check_field());
      }
      json_literal_nesting_++;
      return true;
    }

    if (current_field_) {
      auto ref =
          std::dynamic_pointer_cast<entry::ReferenceField>(current_field_);

      if (!ref) {
        // we can have an Object in a plain data field if the field is for a
        // JSON type column
        json_literal_nesting_++;

        json_literal_nocheck_ = current_field_->no_check;
        json_literal_include_ = current_field_->enabled;

        copy_.on_start_literal_array(current_field_->name, include_field());
        digest_.on_start_literal_array(current_field_->name, check_field());
      } else {
        object_.push_back(ref->nested_object);
        copy_.on_start_array(ref, include_field());
        digest_.on_start_array(ref, check_field());

        path_.pushd(current_field_->name);
      }

      current_field_ = {};
    } else if (current_field_is_builtin_) {
      json_literal_nesting_++;

      json_literal_nocheck_ = true;
      json_literal_include_ = true;
      copy_.on_start_literal_array(*json_literal_field_, include_field());
      digest_.on_start_literal_array(*json_literal_field_, check_field());
    } else {
      assert(0);
    }

    return true;
  }

  bool EndArray([[maybe_unused]] rapidjson::SizeType elementCount) {
    context_.pop_back();

    copy_.on_end_array();
    digest_.on_end_array();

    current_field_ = {};
    if (json_literal_nesting_ > 0) {
      json_literal_nesting_--;
      json_literal_field_ = {};
      return true;
    }

    object_.pop_back();
    path_.popd();

    return true;
  }

  bool is_builtin_field(std::string_view name) {
    return name == "links" || name == "_metadata";
  }

  bool check_field() const {
    bool result = (!current_field_ || !current_field_->no_check) &&
                  (json_literal_nesting_ == 0 || !json_literal_nocheck_);

    if (!result) {
      log_debug("check_field current_field_=%s",
                (current_field_ ? "true" : "false"));
      if (current_field_) {
        log_debug("check_field current_field_->no_check = %s",
                  (current_field_ ? "true" : "false"));
      }
      log_debug("check_field json_literal_nesting_=%i",
                (int)json_literal_nesting_);
      log_debug("check_field json_literal_nocheck_=%s",
                (json_literal_nocheck_ ? "yes" : "no"));
    }

    return result;
  }

  bool include_field() const {
    return (!current_field_ || current_field_->enabled) &&
           (!filter_ || !current_field_ ||
            filter_->is_included(path_.path(), current_field_->name)) &&
           (json_literal_nesting_ == 0 || json_literal_include_);
  }

  const std::string &field_name() const {
    if (json_literal_nesting_ > 0) return *json_literal_field_;
    assert(current_field_);
    return current_field_->name;
  }

  void push_value(rapidjson::Value value, std::string_view repr) {
    if (check_field()) {
      if (context_.back() == ContainerType::ARRAY) {
        digest_.on_elem(value, repr);
      } else {
        digest_.on_field(field_name(), value, repr);
      }
    }
    if (include_field()) {
      if (context_.back() == ContainerType::ARRAY) {
        copy_.on_elem(std::move(value), repr);
      } else {
        copy_.on_field(field_name(), std::move(value), repr);
      }
    }
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

void digest_object(std::shared_ptr<entry::Object> object, std::string_view doc,
                   IDigester *digest) {
  ChecksumHandler handler(object, digest);
  rapidjson::Reader reader;
  rapidjson::MemoryStream ms(doc.data(), doc.length());
  reader.Parse(ms, handler);
}

std::string compute_checksum(std::shared_ptr<entry::Object> object,
                             std::string_view doc) {
  // note: checksum is calculated by following fields in order of appearance in
  // the JSON document, thus it is only suitable for use in documents generated
  // by JsonQueryBuilder, which builds JSON in entry::Object order

  Sha256Digest digest;

  digest_object(object, doc, &digest);

  return string_to_hex(digest.finalize());
}

void process_document_etag_and_filter(
    std::shared_ptr<entry::Object> object, const ObjectFieldFilter &filter,
    const std::map<std::string, std::string> &metadata, std::string *doc) {
  std::string checksum;
  rapidjson::Document new_doc;

  log_debug("calc:%s", doc->c_str());

  Sha256Digest digest;
  ChecksumHandler handler(object, &filter, &digest);
  {
    rapidjson::Reader reader;
    rapidjson::MemoryStream ms(doc->data(), doc->length());
    reader.Parse(ms, handler);

    checksum = string_to_hex(digest.finalize());

    handler.swap(&new_doc);
  }

  rapidjson::Value metadata_object(rapidjson::kObjectType);
  {
    rapidjson::Value etag;
    etag.SetString(checksum.c_str(), new_doc.GetAllocator());
    metadata_object.AddMember("etag", etag, new_doc.GetAllocator());
  }
  for (const auto &f : metadata) {
    rapidjson::Value name;
    rapidjson::Value field;
    name.SetString(f.first.c_str(), new_doc.GetAllocator());
    field.SetString(f.second.c_str(), new_doc.GetAllocator());
    metadata_object.AddMember(name, field, new_doc.GetAllocator());
  }
  new_doc.AddMember("_metadata", metadata_object, new_doc.GetAllocator());

  *doc = helper::json::to_string(new_doc);
}

}  // namespace database
}  // namespace mrs
