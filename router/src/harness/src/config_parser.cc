/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
 * @defgroup ConfigParser Configuration file parser
 *
 * @section Configuration file format
 *
 * The configuration parser parses traditional `.INI` files consisting
 * of sections and options with values but contain some additional
 * features to provide more flexible configuration of the harness.
 */

#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"

#include "utilities.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>

using std::ostringstream;
using std::shared_ptr;

using mysql_harness::utility::find_range_first;
using mysql_harness::utility::matches_glob;
using mysql_harness::utility::strip;

namespace mysql_harness {

// satisfy ODR requirements
constexpr const char *Config::DEFAULT_PATTERN;

static bool isident(const char ch) { return isalnum(ch) || ch == '_'; }

static void inplace_lower(std::string *str) {
  std::transform(str->begin(), str->end(), str->begin(), ::tolower);
}

static std::string lower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);
  return str;
}

static void check_option(const std::string &str) {
  if (!all_of(str.begin(), str.end(), isident))
    throw bad_option("Not a legal option name: '" + str + "'");
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// doxygen has problems to parse the 'const shared_ptr<const ConfigSection>&'
// and treats it as 'const const shared_ptr<ConfigSection>&' can't find a match
ConfigSection::ConfigSection(
    const std::string &name_arg, const std::string &key_arg,
    const std::shared_ptr<const ConfigSection> &defaults)
    : name(name_arg), key(key_arg), defaults_(defaults) {}

ConfigSection::ConfigSection(
    const ConfigSection &other,
    const std::shared_ptr<const ConfigSection> &defaults)
    : name(other.name),
      key(other.key),
      defaults_(defaults),
      options_(other.options_) {}
#endif

void ConfigSection::clear() { options_.clear(); }

// throws bad_section
void ConfigSection::update(const ConfigSection &other) {
#ifndef NDEBUG
  auto old_defaults = defaults_;
#endif

  if (other.name != name || other.key != key) {
    ostringstream buffer;
    buffer << "Trying to update section " << name << ":" << key
           << " using section " << other.name << ":" << other.key;
    throw bad_section(buffer.str());
  }

  for (auto &option : other.options_) options_[option.first] = option.second;

  assert(old_defaults == defaults_);
}

std::string ConfigSection::do_replace(const std::string &value,
                                      int depth) const {
  std::string result;
  bool inside_braces = false;
  std::string::const_iterator mark = value.begin();

  // Simple hack to avoid infinite recursion because of
  // back-references.
  if (depth > kMaxInterpolationDepth)
    throw syntax_error("Max recursion depth for interpolation exceeded.");

  // Scan the string one character at a time and store the result of
  // substituting variable interpolations in the result variable.
  //
  // The scan keeps a mark iterator available that either point to the
  // beginning of the string, the last seen open brace, or just after
  // the last seen closing brace.
  //
  // At any point of the iteration, everything before the mark is
  // already in the result string, and everything at the mark and
  // later is not transfered to the result string.
  for (auto current = value.begin(); current != value.end(); ++current) {
    if (inside_braces && *current == '}') {
      // Inside braces and found the end brace.
      const std::string ident(mark + 1, current);
      auto loc = do_locate(ident);
      if (std::get<1>(loc))
        result.append(do_replace(std::get<0>(loc)->second, depth + 1));
      else
        result.append(mark, current + 1);
      mark = current + 1;
      inside_braces = false;
    } else if (*current == '{') {
      // Start a possible variable interpolation
      result.append(mark, current);
      mark = current;
      inside_braces = true;
    }
  }

  // Append any trailing content of the original string.
  result.append(mark, value.end());

  return result;
}

std::string ConfigSection::get(const std::string &option) const {
  check_option(option);
  auto result = do_locate(option);
  if (std::get<1>(result)) return do_replace(std::get<0>(result)->second);
  throw bad_option("Value for '" + option + "' not found");
}

std::string ConfigSection::get_section_name(const std::string &option) const {
  check_option(option);
  if (!has(option)) {
    return "";
  }
  auto it = options_.find(lower(option));
  if (it != options_.end()) {
    return key.empty() ? name : name + ":" + key;
  } else {
    return defaults_->get_section_name(option);
  }
}

bool ConfigSection::has(const std::string &option) const {
  check_option(option);
  return std::get<1>(do_locate(option));
}

std::pair<ConfigSection::OptionMap::const_iterator, bool>
ConfigSection::do_locate(const std::string &option) const {
  auto it = options_.find(lower(option));
  if (it != options_.end()) return {it, true};

  if (defaults_) return defaults_->do_locate(option);

  // We return a default constructed iterator: any iterator will do.
  return {OptionMap::const_iterator(), false};
}

void ConfigSection::set(const std::string &option, const std::string &value) {
  check_option(option);  // throws bad_option
  options_[lower(option)] = value;
}

void ConfigSection::add(const std::string &option, const std::string &value) {
  auto ret = options_.emplace(OptionMap::value_type(lower(option), value));
  if (!ret.second) throw bad_option("Option '" + option + "' already defined");
}

Config::Config(unsigned int flags) noexcept
    : defaults_(std::make_shared<ConfigSection>("default", "", nullptr)),
      flags_(flags) {}

void Config::copy_guts(const Config &source) noexcept {
  reserved_ = source.reserved_;
  flags_ = source.flags_;
}

bool Config::has(const std::string &section, const std::string &key) const {
  auto it = sections_.find(make_pair(section, key));
  return (it != sections_.end());
}

bool Config::has_any(const std::string &section) const {
  for (auto it : sections_) {
    if (it.first.first == section) return true;
  }
  return false;
}

Config::ConstSectionList Config::get(const std::string &section) const {
  auto rng = find_range_first(sections_, section);
  if (distance(rng.first, rng.second) == 0)
    throw bad_section("Section name '" + section + "' does not exist");
  ConstSectionList result;
  for (auto &&iter = rng.first; iter != rng.second; ++iter)
    result.push_back(&iter->second);
  return result;
}

Config::SectionList Config::get(const std::string &section) {
  auto rng = find_range_first(sections_, section);
  if (distance(rng.first, rng.second) == 0)
    throw bad_section("Section name '" + section + "' does not exist");
  SectionList result;
  for (auto &&iter = rng.first; iter != rng.second; ++iter)
    result.push_back(&iter->second);
  return result;
}

ConfigSection &Config::get(const std::string &section, const std::string &key) {
  // Check if we allow keys and throw an error if keys are not
  // allowed.
  if (!(flags_ & allow_keys))
    throw bad_section("Key '" + key + "' used but keys are not allowed");

  SectionMap::iterator sec = sections_.find(make_pair(section, key));
  if (sec == sections_.end())
    throw bad_section("Section '" + section + "' with key '" + key +
                      "' does not exist");
  return sec->second;
}

const ConfigSection &Config::get(const std::string &section,
                                 const std::string &key) const {
  return const_cast<Config *>(this)->get(section, key);
}

std::string Config::get_default(const std::string &option) const {
  return defaults_->get(option);
}

bool Config::has_default(const std::string &option) const {
  return defaults_->has(option);
}

void Config::set_default(const std::string &option, const std::string &value) {
  defaults_->set(option, value);
}

bool Config::is_reserved(const std::string &word) const {
  auto match = [&word](const std::string &pattern) {
    return matches_glob(word, pattern);
  };

  auto it = find_if(reserved_.begin(), reserved_.end(), match);
  return (it != reserved_.end());
}

// throws syntax_error, bad_section
ConfigSection &Config::add(const std::string &section, const std::string &key) {
  if (is_reserved(section))
    throw syntax_error("Section name '" + section + "' is reserved");

  ConfigSection cnfsec(section, key, defaults_);
  auto result = sections_.emplace(make_pair(section, key), std::move(cnfsec));
  if (!result.second) {
    ostringstream buffer;
    buffer << "Section '" << section << (key.empty() ? "" : (":" + key))
           << "' already exists";
    throw bad_section(buffer.str());
  }

  // Return reference to the newly inserted section.
  return result.first->second;
}

// throws std::invalid_argument, std::runtime_error, syntax_error, ...
void Config::read(const Path &path) {
  if (path.is_directory()) {
    read(path, Config::DEFAULT_PATTERN);  // throws std::invalid_argument,
                                          // possibly others
  } else if (path.is_regular()) {
    Config new_config;
    new_config.copy_guts(*this);
    new_config.do_read_file(path);  // throws std::runtime_error, syntax_error
    update(new_config);
  } else {
    ostringstream buffer;
    buffer << "Path '" << path << "' ";
    if (path.type() == Path::FileType::FILE_NOT_FOUND)
      buffer << "does not exist";
    else
      buffer << "is not a directory or a file";
    throw std::runtime_error(buffer.str());
  }
}

// throws std::invalid_argument, std::runtime_error, syntax_error, ...
void Config::read(const Path &path, const std::string &pattern) {
  Directory dir(path);  // throws std::invalid_argument
  Config new_config;
  new_config.copy_guts(*this);
  for (auto &&iter = dir.glob(pattern); iter != dir.end(); ++iter) {
    Path entry(*iter);
    if (entry.is_regular())  // throws std::invalid_argument
      new_config.do_read_file(
          entry);  // throws std::runtime_error, syntax_error
  }
  update(new_config);
}

void Config::read(std::istream &input) {
  do_read_stream(input);  // throws syntax_error, maybe bad_section
}

void Config::do_read_file(const Path &path) {
  std::ifstream ifs(path.c_str(), std::ifstream::in);
  if (ifs.fail()) {
    ostringstream buffer;
    buffer << "Unable to open file " << path << " for reading";
    throw std::runtime_error(buffer.str());
  }
  do_read_stream(ifs);  // throws syntax_error, maybe bad_section
}

void Config::do_read_stream(std::istream &input) {
  ConfigSection *current = NULL;
  std::string line;
  while (getline(input, line)) {
    strip(&line);

    // Skip empty lines and comment lines.
    if (line.size() == 0 || line[0] == '#' || line[0] == ';') continue;

    // Check for section start and parse it if so.
    if (line[0] == '[') {
      // Check that it is only allowed characters
      if (line.back() != ']') {
        std::string message("Malformed section header: '" + line + "'");
        throw syntax_error(message);
      }

      // Remove leading and trailing brackets
      line.erase(0, 1);
      line.erase(line.size() - 1);

      // Extract the key, if configured to allow keys. Otherwise, the
      // key will be the empty string and the section name is all
      // within the brackets.
      std::string section_name(line);
      std::string section_key;
      if (flags_ & allow_keys) {
        // Split line at first colon
        auto pos = line.find_last_of(':');
        if (pos != std::string::npos) {
          section_key = std::string(line, pos + 1);

          // Check that the section key is correct
          if (section_key.size() == 0 ||
              !std::all_of(section_key.begin(), section_key.end(), isident)) {
            std::string message("Invalid section key '" + section_key + "'");
            throw syntax_error(message);
          }

          section_name.erase(pos);
        }
      }

      // Check that the section name consists of allowable characters only
      if (!std::all_of(section_name.begin(), section_name.end(), isident)) {
        std::string message("Invalid section name '" + section_name + "'");
        if (!(flags_ & allow_keys) &&
            line.find_last_of(':') != std::string::npos) {
          message += " (keys not configured)";
        }
        throw syntax_error(message);
      }

      // Section names are always stored in lowercase and we do not
      // distinguish between sections in lower and upper case.
      inplace_lower(&section_name);

      // If there is a key, check that it is not on the default section
      if (allow_keys && section_name == "default" && !section_key.empty())
        throw syntax_error("Key not allowed on DEFAULT section");

      if (section_name == "default")
        current = defaults_.get();
      else
        current = &add(section_name,
                       section_key);  // throws syntax_error, bad_section
    } else {                          // if (line[0] != '[')
      if (current == NULL)
        throw syntax_error("Option line before start of section");
      // Got option line
      std::string::size_type pos = line.find_first_of(":=");
      if (pos == std::string::npos)
        throw syntax_error("Malformed option line: '" + line + "'");
      std::string option(line, 0, pos);
      strip(&option);
      std::string value(line, pos + 1);
      strip(&value);

      // Check that the section name consists of allowable characters only
      if (!std::all_of(option.begin(), option.end(), isident))
        throw syntax_error("Invalid option name '" + option + "'");

      current->add(option, value);  // throws syntax_error, bad_section
    }
  }

  if (line.size() > 0) throw syntax_error("Unterminated last line");
}

bool Config::empty() const { return sections_.empty(); }

void Config::clear() {
  defaults_->clear();
  sections_.clear();
}

bool Config::remove(const SectionKey &section_key) noexcept {
  return sections_.erase(section_key);
}

bool Config::remove(const std::string &section,
                    const std::string &key /*= std::string()*/) noexcept {
  return remove(SectionKey(section, key));
}

void Config::update(const Config &other) {
  // Pre-condition is that the default section pointers before the
  // update all refer to the default section for this configuration
  // instance.
  assert(std::all_of(sections_.cbegin(), sections_.cend(),
                     [this](const SectionMap::value_type &val) -> bool {
                       return val.second.assert_default(defaults_.get());
                     }));

  for (const auto &section : other.sections_) {
    const SectionKey &key = section.first;
    SectionMap::iterator iter = sections_.find(key);
    if (iter == sections_.end())
      sections_.emplace(key, ConfigSection(section.second, defaults_));
    else
      iter->second.update(section.second);
  }

  defaults_->update(*other.defaults_.get());

  // Post-condition is that the default section pointers after the
  // update all refer to the default section for this configuration
  // instance.
#ifndef NDEBUG
  auto check = [this](const SectionMap::value_type &val) -> bool {
    return val.second.assert_default(defaults_.get());
  };
#endif
  assert(std::all_of(sections_.cbegin(), sections_.cend(), check));
}

Config::ConstSectionList Config::sections() const {
  decltype(sections()) result;
  for (auto &section : sections_) result.push_back(&section.second);
  return result;
}

}  // namespace mysql_harness
