/*
   Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements the function defined in ndb_dd_sdi.h
#include "storage/ndb/plugin/ndb_dd_sdi.h"

// Using
#include "my_rapidjson_size_t.h"  // IWYU pragma: keep

#include <rapidjson/document.h>      // rapidjson::Document
#include <rapidjson/prettywriter.h>  // rapidjson::PrettyWriter
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>  // rapidjson::Writer

#include "my_sys.h"         // my_error
#include "mysql_version.h"  // MYSQL_VERSION_ID
#include "mysqld_error.h"   // ER_IMP_INCOMPATIBLE_MYSQLD_VERSION
#include "sql/dd/impl/sdi.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"

typedef rapidjson::Writer<dd::RJ_StringBuffer, dd::RJ_Encoding, dd::RJ_Encoding,
                          dd::RJ_Allocator, 0>
    MinifyWriter;
typedef rapidjson::PrettyWriter<dd::RJ_StringBuffer, dd::RJ_Encoding,
                                dd::RJ_Encoding, dd::RJ_Allocator, 0>
    PrettyWriter;

#ifndef NDEBUG
/*
  @brief minify a JSON formatted SDI. Remove whitespace and other
  useless data.

  @note the JSON format is normally in 'pretty' format which takes
  up much more storage space and network bandwidth than 'minified' format.

  @sdi the JSON string to minify

  @return minified JSON string or empty string on failure.
*/

static dd::sdi_t minify(dd::sdi_t sdi) {
  dd::RJ_Document doc;
  doc.Parse<0>(sdi.c_str());

  if (doc.HasParseError()) {
    return "";
  }

  dd::RJ_StringBuffer buf;
  MinifyWriter w(buf);
  if (!doc.Accept(w)) {
    return "";
  }

  return buf.GetString();
}
#endif

dd::sdi_t ndb_dd_sdi_prettify(dd::sdi_t sdi) {
  dd::RJ_Document doc;
  doc.Parse<0>(sdi.c_str());

  if (doc.HasParseError()) {
    return "";
  }

  dd::RJ_StringBuffer buf;
  PrettyWriter w(buf);
  if (!doc.Accept(w)) {
    return "";
  }

  return buf.GetString();
}

static bool check_sdi_compatibility(const dd::RJ_Document &doc) {
  // Check mysql_version_id
  assert(doc.HasMember("mysqld_version_id"));
  const dd::RJ_Value &mysqld_version_id = doc["mysqld_version_id"];
  assert(mysqld_version_id.IsUint64());
  if (mysqld_version_id.GetUint64() > std::uint64_t(MYSQL_VERSION_ID)) {
    // Cannot deserialize SDIs from newer versions
    my_error(ER_IMP_INCOMPATIBLE_MYSQLD_VERSION, MYF(0),
             mysqld_version_id.GetUint64(), std::uint64_t(MYSQL_VERSION_ID));
    return true;
  }
  // Skip dd_version and sdi_version checks to ensure compatibility during
  // upgrades
  return false;
}

bool ndb_dd_sdi_deserialize(THD *thd, const dd::sdi_t &sdi, dd::Table *table) {
  const dd::SdiCompatibilityChecker comp_checker = check_sdi_compatibility;
  return dd::deserialize(thd, sdi, table, comp_checker);
}

dd::sdi_t ndb_dd_sdi_serialize(THD *thd, const dd::Table &table,
                               const dd::String_type &schema_name) {
#ifndef NDEBUG
  // Verify that dd::serialize generates SDI in minimized format
  dd::sdi_t sdi = dd::serialize(thd, table, schema_name);
  assert(minify(sdi) == sdi);
#endif
  return dd::serialize(thd, table, schema_name);
}
