// Author: Ming Zhang
// Adapted from mica
// Copyright (c) 2022

#pragma once

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <streambuf>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Winline"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#pragma GCC diagnostic pop

class JsonConfig {
 public:
  JsonConfig();
  JsonConfig(const JsonConfig& config);
  ~JsonConfig() {}

  // This constructor is public to support emplace_back(), emplace().
  JsonConfig(const std::shared_ptr<rapidjson::Document>& root,
             rapidjson::Value* current, std::string path);

  JsonConfig& operator=(const JsonConfig& config) = delete;

  static JsonConfig empty_array(std::string path);
  static JsonConfig empty_dict(std::string path);

  static JsonConfig load_file(std::string path);
  void dump_file(std::string path) const;

  static JsonConfig load(std::string json_text, std::string path);
  std::string dump() const;

  std::string get_path() const;

  bool exists() const;
  bool is_bool() const;
  bool is_int64() const;
  bool is_uint64() const;
  bool is_double() const;
  bool is_str() const;
  bool is_array() const;
  bool is_dict() const;

  bool get_bool() const;
  int64_t get_int64() const;
  uint64_t get_uint64() const;
  double get_double() const;
  std::string get_str() const;

  bool get_bool(bool default_v) const;
  int64_t get_int64(int64_t default_v) const;
  uint64_t get_uint64(uint64_t default_v) const;
  double get_double(double default_v) const;
  std::string get_str(const std::string& default_v) const;

  size_t size() const;
  const JsonConfig get(size_t index) const;
  JsonConfig get(size_t index);

  std::vector<std::string> keys() const;
  const JsonConfig get(std::string key) const;
  JsonConfig get(std::string key);

  JsonConfig& push_back_bool(bool v);
  JsonConfig& push_back_int64(int64_t v);
  JsonConfig& push_back_uint64(uint64_t v);
  JsonConfig& push_back_double(double v);
  JsonConfig& push_back_string(const std::string& v);
  JsonConfig& push_back_array(const JsonConfig& v);
  JsonConfig& push_back_dict(const JsonConfig& v);

  JsonConfig& insert_bool(std::string key, bool v);
  JsonConfig& insert_int64(std::string key, int64_t v);
  JsonConfig& insert_uint64(std::string key, uint64_t v);
  JsonConfig& insert_double(std::string key, double v);
  JsonConfig& insert_string(std::string key, std::string v);
  JsonConfig& insert_array(std::string key, const JsonConfig& v);
  JsonConfig& insert_dict(std::string key, const JsonConfig& v);

 private:
  std::shared_ptr<rapidjson::Document> root_;
  rapidjson::Value* current_;
  std::string path_;
};

ALWAYS_INLINE
JsonConfig::JsonConfig() : JsonConfig(nullptr, nullptr, "") {}

ALWAYS_INLINE
JsonConfig::JsonConfig(const JsonConfig& config)
    : JsonConfig(config.root_, config.current_, config.path_) {}

ALWAYS_INLINE
JsonConfig JsonConfig::empty_array(std::string path) {
  std::shared_ptr<rapidjson::Document> root =
      std::make_shared<rapidjson::Document>();
  root->SetArray();
  return JsonConfig(root, root.get(), path);
}

ALWAYS_INLINE
JsonConfig JsonConfig::empty_dict(std::string path) {
  std::shared_ptr<rapidjson::Document> root =
      std::make_shared<rapidjson::Document>();
  root->SetObject();
  return JsonConfig(root, root.get(), path);
}

ALWAYS_INLINE
JsonConfig JsonConfig::load_file(std::string path) {
  std::string conf;

  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    fprintf(stderr, "error: could not open %s\n", path.c_str());
    assert(false);
    return JsonConfig(nullptr, nullptr, std::string() + "<" + path + ">");
  }

  ifs.seekg(0, std::ios::end);
  conf.reserve(static_cast<size_t>(ifs.tellg()));
  ifs.seekg(0, std::ios::beg);

  conf.assign((std::istreambuf_iterator<char>(ifs)),
              std::istreambuf_iterator<char>());
  return JsonConfig::load(conf, std::string() + "<" + path + ">");
}

ALWAYS_INLINE
void JsonConfig::dump_file(std::string path) const {
  assert(exists());

  std::string conf = dump();

  std::ofstream ofs(path);
  if (!ofs.is_open()) {
    fprintf(stderr, "error: could not open %s\n", path.c_str());
    assert(false);
    return;
  }

  ofs << conf;
}

ALWAYS_INLINE
JsonConfig JsonConfig::load(std::string json_text, std::string path) {
  std::shared_ptr<rapidjson::Document> root =
      std::make_shared<rapidjson::Document>();
  root->Parse<rapidjson::ParseFlag::kParseDefaultFlags |
              rapidjson::ParseFlag::kParseCommentsFlag>(json_text.c_str());

  if (root->HasParseError()) {
    fprintf(stderr, "error parsing config: %s (offset=%zu)\n",
            rapidjson::GetParseError_En(root->GetParseError()),
            root->GetErrorOffset());
    return JsonConfig(nullptr, nullptr, path);
  }

  return JsonConfig(root, root.get(), path);
}

ALWAYS_INLINE
std::string JsonConfig::dump() const {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
#pragma GCC diagnostic pop

  current_->Accept(writer);
  const char* output = buffer.GetString();
  return std::string(output);
}

ALWAYS_INLINE
std::string JsonConfig::get_path() const { return path_; }

ALWAYS_INLINE
bool JsonConfig::exists() const { return current_ != nullptr; }

ALWAYS_INLINE
bool JsonConfig::is_bool() const { return exists() && current_->IsBool(); }

ALWAYS_INLINE
bool JsonConfig::is_int64() const { return exists() && current_->IsInt64(); }

ALWAYS_INLINE
bool JsonConfig::is_uint64() const { return exists() && current_->IsUint64(); }

ALWAYS_INLINE
bool JsonConfig::is_double() const { return exists() && current_->IsDouble(); }

ALWAYS_INLINE
bool JsonConfig::is_str() const { return exists() && current_->IsString(); }

ALWAYS_INLINE
bool JsonConfig::is_array() const { return exists() && current_->IsArray(); }

ALWAYS_INLINE
bool JsonConfig::is_dict() const { return exists() && current_->IsObject(); }

ALWAYS_INLINE
bool JsonConfig::get_bool() const {
  if (!exists()) {
    fprintf(stderr, "error: %s does not exist\n", path_.c_str());
    assert(false);
    return false;
  }
  if (!is_bool()) {
    fprintf(stderr, "error: %s is not a boolean value\n", path_.c_str());
    assert(false);
    return false;
  }
  return current_->GetBool();
}

ALWAYS_INLINE
int64_t JsonConfig::get_int64() const {
  if (!exists()) {
    fprintf(stderr, "error: %s does not exist\n", path_.c_str());
    assert(false);
    return 0;
  }
  if (!is_int64()) {
    fprintf(stderr, "error: %s is not an Int64 number\n", path_.c_str());
    assert(false);
    return 0;
  }
  return current_->GetInt64();
}

ALWAYS_INLINE
uint64_t JsonConfig::get_uint64() const {
  if (!exists()) {
    fprintf(stderr, "error: %s does not exist\n", path_.c_str());
    assert(false);
    return 0;
  }
  if (!is_uint64()) {
    fprintf(stderr, "error: %s is not an Uint64 number\n", path_.c_str());
    assert(false);
    return 0;
  }
  return current_->GetUint64();
}

ALWAYS_INLINE
double JsonConfig::get_double() const {
  if (!exists()) {
    fprintf(stderr, "error: %s does not exist\n", path_.c_str());
    assert(false);
    return 0.;
  }
  if (!is_double()) {
    fprintf(stderr, "error: %s is not a floating point number\n",
            path_.c_str());
    assert(false);
    return 0.;
  }
  return current_->GetDouble();
}

ALWAYS_INLINE
std::string JsonConfig::get_str() const {
  if (!exists()) {
    fprintf(stderr, "error: %s does not exist\n", path_.c_str());
    assert(false);
    return "";
  }
  if (!is_str()) {
    fprintf(stderr, "error: %s is not a string\n", path_.c_str());
    assert(false);
    return "";
  }
  return std::string(current_->GetString(), current_->GetStringLength());
}

ALWAYS_INLINE
bool JsonConfig::get_bool(bool default_v) const {
  if (exists())
    return get_bool();
  else
    return default_v;
}

ALWAYS_INLINE
int64_t JsonConfig::get_int64(int64_t default_v) const {
  if (exists())
    return get_int64();
  else
    return default_v;
}

ALWAYS_INLINE
uint64_t JsonConfig::get_uint64(uint64_t default_v) const {
  if (exists())
    return get_uint64();
  else
    return default_v;
}

ALWAYS_INLINE
double JsonConfig::get_double(double default_v) const {
  if (exists())
    return get_double();
  else
    return default_v;
}

ALWAYS_INLINE
std::string JsonConfig::get_str(const std::string& default_v) const {
  if (exists())
    return get_str();
  else
    return default_v;
}

ALWAYS_INLINE
size_t JsonConfig::size() const {
  assert(is_array());
  return current_->Size();
}

ALWAYS_INLINE
JsonConfig JsonConfig::get(size_t index) {
  assert(is_array());

  std::ostringstream oss;
  oss << path_ << '[' << index << ']';

  if (index >= current_->Size())
    return JsonConfig(root_, nullptr, oss.str());
  else
    return JsonConfig(root_, &((*current_)[static_cast<unsigned int>(index)]),
                      oss.str());
}

ALWAYS_INLINE
const JsonConfig JsonConfig::get(size_t index) const {
  return const_cast<JsonConfig*>(this)->get(index);
}

ALWAYS_INLINE
std::vector<std::string> JsonConfig::keys() const {
  assert(is_dict());

  std::vector<std::string> keys;
  for (rapidjson::Value::ConstMemberIterator it = current_->MemberBegin();
       it != current_->MemberEnd(); ++it) {
    keys.emplace_back(it->name.GetString(), it->name.GetStringLength());
  }
  return keys;
}

ALWAYS_INLINE
JsonConfig JsonConfig::get(std::string key) {
  assert(is_dict());

  std::ostringstream oss;
  oss << path_ << "[\"" << key << "\"]";

  if (!exists()) return JsonConfig(root_, nullptr, oss.str());

  rapidjson::Value::MemberIterator it = current_->FindMember(key.c_str());
  if (it == current_->MemberEnd()) return JsonConfig(root_, nullptr, oss.str());

  return JsonConfig(root_, &it->value, oss.str());
}

ALWAYS_INLINE
const JsonConfig JsonConfig::get(std::string key) const {
  return const_cast<JsonConfig*>(this)->get(key);
}

ALWAYS_INLINE
JsonConfig& JsonConfig::push_back_bool(bool v) {
  assert(is_array());
  current_->PushBack(v, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::push_back_int64(int64_t v) {
  assert(is_array());
  current_->PushBack(v, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::push_back_uint64(uint64_t v) {
  assert(is_array());
  current_->PushBack(v, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::push_back_double(double v) {
  assert(is_array());
  current_->PushBack(v, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::push_back_array(const JsonConfig& v) {
  assert(is_array());
  assert(v.is_array());
  current_->PushBack(*v.current_, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::push_back_dict(const JsonConfig& v) {
  assert(is_array());
  assert(v.is_dict());
  current_->PushBack(*v.current_, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::insert_bool(std::string key, bool v) {
  assert(is_dict());
  rapidjson::Value v_key(key.c_str(), root_->GetAllocator());
  current_->AddMember(v_key, v, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::insert_int64(std::string key, int64_t v) {
  assert(is_dict());
  rapidjson::Value v_key(key.c_str(), root_->GetAllocator());
  current_->AddMember(v_key, v, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::insert_uint64(std::string key, uint64_t v) {
  assert(is_dict());
  rapidjson::Value v_key(key.c_str(), root_->GetAllocator());
  current_->AddMember(v_key, v, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::insert_double(std::string key, double v) {
  assert(is_dict());
  rapidjson::Value v_key(key.c_str(), root_->GetAllocator());
  current_->AddMember(v_key, v, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::insert_array(std::string key, const JsonConfig& v) {
  assert(is_dict());
  assert(v.is_array());
  rapidjson::Value v_key(key.c_str(), root_->GetAllocator());
  current_->AddMember(v_key, *v.current_, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig& JsonConfig::insert_dict(std::string key, const JsonConfig& v) {
  assert(is_dict());
  assert(v.is_dict());
  rapidjson::Value v_key(key.c_str(), root_->GetAllocator());
  current_->AddMember(v_key, *v.current_, root_->GetAllocator());
  return *this;
}

ALWAYS_INLINE
JsonConfig::JsonConfig(const std::shared_ptr<rapidjson::Document>& root,
                       rapidjson::Value* current, std::string path)
    : root_(root), current_(current), path_(path) {}