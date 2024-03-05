/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#pragma once

/**
  @file
  This service provides interface for loading data in bulk from CSV files.

*/

#include "my_rapidjson_size_t.h"

#include <mysql/components/service.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include "m_string.h"
#include "my_thread_local.h"

/* Forward declaration for opaque types. */
class THD;
struct TABLE;
struct CHARSET_INFO;

using Bulk_loader = void;

/** Bulk loader source. */
enum class Bulk_source {
  /** Local file system. */
  LOCAL,
  /** OCI object store. */
  OCI,
  /** Amazon S3. */
  S3
};

inline std::string trim_left(const std::string &s) {
  auto pos = s.find_first_not_of(" \n\r\t");
  return s.substr(pos);
}

struct Bulk_load_file_info {
  Bulk_load_file_info(const Bulk_source &source,
                      const std::string &input_string, const size_t &n_files)
      : m_source(source),
        m_input_string(trim_left(input_string)),
        m_n_files(n_files) {}

  std::string m_file_prefix;
  std::optional<std::string> m_file_suffix;
  std::string m_appendtolastprefix;
  size_t m_start_index{1};
  bool m_is_dryrun{false};

  bool parse(std::string &error);

  std::ostream &print(std::ostream &out) const;

  /** Check if the COUNT clause has been explicitly specified.
  @return true if COUNT is specified explicitly, false otherwise. */
  bool is_count_specified() const { return m_n_files > 0; }

 private:
  rapidjson::Document m_doc;
  const Bulk_source m_source;
  const std::string m_input_string;

  /* This value can be 0, only if COUNT clause is not specified.  If COUNT
  clause is specified, this value will be greater than 0. */
  const size_t m_n_files{0};
};

inline std::ostream &Bulk_load_file_info::print(std::ostream &out) const {
  std::string suffix = m_file_suffix.has_value() ? m_file_suffix.value() : "";
  out << "[Bulk_load_file_info: m_file_prefix=" << m_file_prefix << ", "
      << "m_file_suffix=" << suffix << ", "
      << "m_appendtolastprefix=" << m_appendtolastprefix << ", "
      << "m_start_index=" << m_start_index << ", "
      << "m_is_dryrun=" << m_is_dryrun << "]";
  return out;
}

inline std::ostream &operator<<(std::ostream &out,
                                const Bulk_load_file_info &obj) {
  return obj.print(out);
}

/** Check whether the specified argument is a valid JSON object. Used to check
whether the user specified JSON or a regular filename as the LOAD location
argument.
@param[in] file_name_arg  filename argument provided by the user.
@return true if the arg is a JSON object. */
inline static bool is_json_object(const std::string &file_name_arg) {
  rapidjson::Document doc;
  doc.Parse(file_name_arg.c_str());
  return !doc.HasParseError() && doc.IsObject();
}

/** Validates whether the json argument matches the expected schema for bulk
load, if it matches it fills out the Bulk_load_input structure, sets error
and returns false otherwise.
@param[out] error contains the appropriate error message.
@param[out] info  parsed structure of file information (containing prefix and
optional suffix)
@param[in]  doc   rapidjson document
@return false if configuration contains unknown or unsupported values. */
inline static bool parse_input_arg(std::string &error,
                                   Bulk_load_file_info &info,
                                   const rapidjson::Document &doc) {
  constexpr char PREFIX_KEY[] = "url-prefix";
  constexpr char SUFFIX_KEY[] = "url-suffix";
  constexpr char APPENDTOLASTPREFIX_KEY[] = "url-prefix-last-append";
  constexpr char SEQUENCE_START_KEY[] = "url-sequence-start";
  constexpr char DRYRUN_KEY[] = "is-dryrun";
  static const std::unordered_set<std::string> all_keys = {
      PREFIX_KEY, SUFFIX_KEY, APPENDTOLASTPREFIX_KEY, SEQUENCE_START_KEY,
      DRYRUN_KEY};

  if (!doc.IsObject()) {
    error = "Invalid JSON object used for filename argument!";
    return false;
  }

  for (const auto &child : doc.GetObject()) {
    std::string key = child.name.GetString();
    if (all_keys.find(key) == all_keys.end()) {
      std::stringstream ss;
      ss << "Unsupported JSON key: " << key;
      error = ss.str();
      return false;
    }
  }

  if (!doc.HasMember(PREFIX_KEY)) {
    error = "Missing url-prefix in JSON filename argument!";
    return false;
  }

  if (!doc[PREFIX_KEY].IsString()) {
    std::stringstream ss;
    ss << "The value of key " << PREFIX_KEY << " must be a string";
    error = ss.str();
    return false;
  }

  info.m_file_prefix = doc[PREFIX_KEY].GetString();

  if (doc.HasMember(SUFFIX_KEY)) {
    if (!info.is_count_specified()) {
      std::ostringstream sout;
      sout << "Cannot specify " << SUFFIX_KEY << " without COUNT clause";
      error = sout.str();
      return false;
    }

    if (!doc[SUFFIX_KEY].IsString()) {
      std::stringstream ss;
      ss << "The value of key " << SUFFIX_KEY << " must be a string";
      error = ss.str();
      return false;
    }
    info.m_file_suffix = doc[SUFFIX_KEY].GetString();
  }

  if (doc.HasMember(APPENDTOLASTPREFIX_KEY)) {
    if (!info.is_count_specified()) {
      std::ostringstream sout;
      sout << "Cannot specify " << APPENDTOLASTPREFIX_KEY
           << " without COUNT clause";
      error = sout.str();
      return false;
    }
    if (!doc[APPENDTOLASTPREFIX_KEY].IsString()) {
      std::stringstream ss;
      ss << "The value of key " << APPENDTOLASTPREFIX_KEY
         << " must be a string";
      error = ss.str();
      return false;
    }
    info.m_appendtolastprefix = doc[APPENDTOLASTPREFIX_KEY].GetString();
  }

  if (doc.HasMember(SEQUENCE_START_KEY)) {
    if (!info.is_count_specified()) {
      std::ostringstream sout;
      sout << "Cannot specify " << SEQUENCE_START_KEY
           << " without COUNT clause";
      error = sout.str();
      return false;
    }
    if (doc[SEQUENCE_START_KEY].IsInt64()) {
      /* Check for -ve numbers and report error */
      const int64_t val = doc[SEQUENCE_START_KEY].GetInt64();
      if (val < 0) {
        std::ostringstream sout;
        sout << "The value of key " << SEQUENCE_START_KEY
             << " cannot be negative: (" << val << ")";
        error = sout.str();
        return false;
      }
    }
    if (doc[SEQUENCE_START_KEY].IsUint64()) {
      info.m_start_index = doc[SEQUENCE_START_KEY].GetUint64();
    } else if (doc[SEQUENCE_START_KEY].IsString()) {
      const std::string val = doc[SEQUENCE_START_KEY].GetString();
      if (val.empty()) {
        std::ostringstream sout;
        sout << "The value of key " << SEQUENCE_START_KEY << " cannot be empty";
        error = sout.str();
        return false;
      } else if ((val.length() == 7) &&
                 (native_strncasecmp(val.c_str(), "default", 7) == 0)) {
        info.m_start_index = 1;
      } else if (std::all_of(val.begin(), val.end(),
                             [](unsigned char c) { return std::isdigit(c); })) {
        info.m_start_index = std::strtoull(val.c_str(), nullptr, 10);
      } else {
        std::ostringstream sout;
        sout << "The value of key " << SEQUENCE_START_KEY << " is invalid ("
             << val << ")";
        error = sout.str();
        return false;
      }
    } else {
      std::ostringstream sout;
      sout << "Invalid value for key " << SEQUENCE_START_KEY;
      error = sout.str();
      return false;
    }
  }

  if (doc.HasMember(DRYRUN_KEY)) {
    if (doc[DRYRUN_KEY].IsBool()) {
      info.m_is_dryrun = doc[DRYRUN_KEY].GetBool();
    } else if (doc[DRYRUN_KEY].IsString()) {
      const std::string val = doc[DRYRUN_KEY].GetString();
      if (val == "1" || (native_strncasecmp(val.c_str(), "on", 2) == 0) ||
          (native_strncasecmp(val.c_str(), "true", 4) == 0)) {
        info.m_is_dryrun = true;
      } else if (val == "0" ||
                 (native_strncasecmp(val.c_str(), "off", 3) == 0) ||
                 (native_strncasecmp(val.c_str(), "false", 5)) == 0) {
        info.m_is_dryrun = false;
      } else {
        std::ostringstream sout;
        sout << "Unsupported " << DRYRUN_KEY << " value: " << val;
        error = sout.str();
        return false;
      }
    } else if (doc[DRYRUN_KEY].IsUint64()) {
      const uint64_t val = doc[DRYRUN_KEY].GetUint64();
      if (val == 0) {
        info.m_is_dryrun = false;
      } else if (val == 1) {
        info.m_is_dryrun = true;
      } else {
        std::ostringstream sout;
        sout << "Unsupported " << DRYRUN_KEY << " value: " << val;
        error = sout.str();
        return false;
      }
    } else {
      std::stringstream ss;
      ss << "Invalid value for key " << DRYRUN_KEY;
      error = ss.str();
      return false;
    }
  } else {
    info.m_is_dryrun = false;
  }

  error = "";
  return true;
}

inline bool Bulk_load_file_info::parse(std::string &error) {
  rapidjson::ParseResult ok = m_doc.Parse(m_input_string.c_str());
  std::string parse_error;

  if (!ok) {
    parse_error = rapidjson::GetParseError_En(ok.Code());
  }

  if (!m_doc.HasParseError()) {
    if (!parse_input_arg(error, *this, m_doc)) {
      return false;
    }
  } else {
    if (m_source == Bulk_source::OCI) {
      auto pos = m_input_string.find(":");
      std::string protocol = m_input_string.substr(0, pos);
      std::for_each(protocol.begin(), protocol.end(),
                    [](unsigned char c) { return std::tolower(c); });
      if (protocol == "http" || protocol == "https") {
        /* Protocol is supported. */
      } else {
        std::ostringstream sout;
        if (protocol.starts_with('{')) {
          sout << "Could be malformed JSON (" << parse_error << ") or ";
        }
        sout << "Unsupported protocol in URL";
        error = sout.str();
        return false;
      }
    } else if (m_source == Bulk_source::LOCAL) {
      /* Nothing yet. */
    }

    /* In case json parsing failed, the file_name_arg only contains the file
     * prefix. */
    m_file_prefix = m_input_string;
    m_file_suffix = std::nullopt;
  }
  return true;
}

/** Bulk data compression algorithm. */
enum class Bulk_compression_algorithm { NONE, ZSTD };

/** Bulk loader string attributes. */
enum class Bulk_string {
  /** Schema name */
  SCHEMA_NAME,
  /* Table name */
  TABLE_NAME,
  /* File prefix URL */
  FILE_PREFIX,
  /* File suffix */
  FILE_SUFFIX,
  /** Column terminator */
  COLUMN_TERM,
  /** Row terminator */
  ROW_TERM,
  /** String to append to last file prefix. */
  APPENDTOLASTPREFIX,
};

/** Bulk loader boolean attributes. */
enum class Bulk_condition {
  /** The algorithm used is different based on whether the data is in sorted
  primary key order. This option tells whether to expect sorted input. */
  ORDERED_DATA,
  /** If enclosing is optional. */
  OPTIONAL_ENCLOSE,
  /** If true, the current execution is only a dry run.  No need to load data
  into the table. */
  DRYRUN
};

/** Bulk loader size attributes. */
enum class Bulk_size {
  /** Number of input files. */
  COUNT_FILES,
  /** Number of rows to skip. */
  COUNT_ROW_SKIP,
  /** Number of columns in the table. */
  COUNT_COLUMNS,
  /** Number of concurrent loaders to use, */
  CONCURRENCY,
  /** Total memory size to use for LOAD in bytes. */
  MEMORY,
  /** Index of the first file. */
  START_INDEX
};

/** Bulk loader single byte attributes. */
enum class Bulk_char {
  /** Escape character. */
  ESCAPE_CHAR,
  /** Column enclosing character. */
  ENCLOSE_CHAR
};

/** Bulk load driver service. */
BEGIN_SERVICE_DEFINITION(bulk_load_driver)

/**
  Create bulk loader.
  @param[in]  thd     mysql THD
  @param[in]  table   mysql TABLE object
  @param[in]  src     bulk loader source
  @param[in]  charset source data character set
  @return bulk loader object, opaque type.
*/
DECLARE_METHOD(Bulk_loader *, create_bulk_loader,
               (THD * thd, my_thread_id connection_id, const TABLE *table,
                Bulk_source src, const CHARSET_INFO *charset));
/**
  Set string attribute for loading data.
  @param[in,out]  loader  bulk loader
  @param[in]      type    attribute type
  @param[in]      value   attribute value
*/
DECLARE_METHOD(void, set_string,
               (Bulk_loader * loader, Bulk_string type, std::string value));
/**
  Set single byte character attribute for loading data.
  @param[in,out]  loader  bulk loader
  @param[in]      type    attribute type
  @param[in]      value   attribute value
*/
DECLARE_METHOD(void, set_char,
               (Bulk_loader * loader, Bulk_char type, unsigned char value));
/**
  Set size attribute for loading data.
  @param[in,out]  loader  bulk loader
  @param[in]      type    attribute type
  @param[in]      value   attribute value
*/
DECLARE_METHOD(void, set_size,
               (Bulk_loader * loader, Bulk_size type, size_t value));
/**
  Set boolean condition attribute for loading data.
  @param[in,out]  loader  bulk loader
  @param[in]      type    attribute type
  @param[in]      value   attribute value
*/
DECLARE_METHOD(void, set_condition,
               (Bulk_loader * loader, Bulk_condition type, bool value));

/**
  Set boolean condition attribute for loading data.
  @param[in,out]  loader    bulk loader
  @param[in]      algorithm the compression algorithm used
*/
DECLARE_METHOD(void, set_compression_algorithm,
               (Bulk_loader * loader, Bulk_compression_algorithm algorithm));

/**
  Load data from CSV files.
  @param[in,out]  loader  bulk loader
  @return true if successful.
*/
DECLARE_METHOD(bool, load, (Bulk_loader * loader, size_t &affected_rows));

/**
  Drop bulk loader.
  @param[in,out]  thd     mysql THD
  @param[in,out]  loader  loader object to drop
*/
DECLARE_METHOD(void, drop_bulk_loader, (THD * thd, Bulk_loader *loader));

END_SERVICE_DEFINITION(bulk_load_driver)
