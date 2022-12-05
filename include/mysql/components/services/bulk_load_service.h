/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#pragma once

/**
  @file
  This service provides interface for loading data in bulk from CSV files.

*/

#include <mysql/components/service.h>
#include <string>

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

/** Bulk loader string attributes. */
enum class Bulk_string {
  /** Schema name */
  SCHEMA_NAME,
  /* Table name */
  TABLE_NAME,
  /* File prefix URL */
  FILE_PREFIX,
  /** Colum trminator */
  COLUMN_TERM,
  /** Row trminator */
  ROW_TERM,
};

/** Bulk loader boolean attributes. */
enum class Bulk_condition {
  /** The algorithm used is different based on whether the data is in sorted
  primary key order. This option tells whether to expect sorted input. */
  ORDERED_DATA,
  /** If enclosing is optional. */
  OPTIONAL_ENCLOSE
};

/** Bulk loader size attributes. */
enum class Bulk_size {
  /** Number of input files. */
  COUNT_FILES,
  /** Number of rows to skip. */
  COUNT_ROW_SKIP,
  /** Number of columns in the table. */
  COUNT_COLUMNS
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
  @param[in]  src     BUlk loader source
  @param[in]  charset source data character set
  @return bulk loader object, opaque type.
*/
DECLARE_METHOD(Bulk_loader *, create_bulk_loader,
               (THD * thd, const TABLE *table, Bulk_source src,
                const CHARSET_INFO *charset));
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
  Load data from CSV files.
  @param[in,out]  loader  bulk loader
  @return true if successful.
*/
DECLARE_METHOD(bool, load, (Bulk_loader * loader, long long &affected_rows));

/**
  Drop bulk loader.
  @param[in,out]  thd     mysql THD
  @param[in,out]  laoder  loader object to drop
*/
DECLARE_METHOD(void, drop_bulk_loader, (THD * thd, Bulk_loader *loader));

END_SERVICE_DEFINITION(bulk_load_driver)
