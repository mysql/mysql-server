/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PARSE_TREE_COL_ATTRS_INCLUDED
#define PARSE_TREE_COL_ATTRS_INCLUDED

#include <type_traits>

#include "my_dbug.h"
#include "mysql/mysql_lex_string.h"
#include "mysql_com.h"
#include "nullable.h"
#include "sql/gis/srid.h"
#include "sql/item_timefunc.h"
#include "sql/parse_tree_node_base.h"
#include "sql/sql_alter.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"

using Mysql::Nullable;

/**
  Parse context for column type attribyte specific parse tree nodes.

  For internal use in the contextualization code.

  @ingroup ptn_column_attrs ptn_gcol_attrs
*/
struct Column_parse_context : public Parse_context {
  const bool is_generated;  ///< Owner column is a generated one.

  Column_parse_context(THD *thd, SELECT_LEX *select, bool is_generated)
      : Parse_context(thd, select), is_generated(is_generated) {}
};

/**
  Base class for all column attributes in @SQL{CREATE/ALTER TABLE}

  @ingroup ptn_column_attrs ptn_gcol_attrs
*/
class PT_column_attr_base : public Parse_tree_node_tmpl<Column_parse_context> {
 protected:
  PT_column_attr_base() {}

 public:
  typedef decltype(Alter_info::flags) alter_info_flags_t;

  virtual void apply_type_flags(ulong *) const {}
  virtual void apply_alter_info_flags(uint *) const {}
  virtual void apply_comment(LEX_STRING *) const {}
  virtual void apply_default_value(Item **) const {}
  virtual void apply_on_update_value(Item **) const {}
  virtual void apply_srid_modifier(Nullable<gis::srid_t> *) const {}
  virtual bool apply_collation(const CHARSET_INFO **to MY_ATTRIBUTE((unused)),
                               bool *has_explicit_collation) const {
    *has_explicit_collation = false;
    return false;
  }
};

/**
  Node for the @SQL{NULL} column attribute

  @ingroup ptn_column_attrs
*/
class PT_null_column_attr : public PT_column_attr_base {
 public:
  virtual void apply_type_flags(ulong *type_flags) const {
    *type_flags &= ~NOT_NULL_FLAG;
    *type_flags |= EXPLICIT_NULL_FLAG;
  }
};

/**
  Node for the @SQL{NOT NULL} column attribute

  @ingroup ptn_column_attrs
*/
class PT_not_null_column_attr : public PT_column_attr_base {
  virtual void apply_type_flags(ulong *type_flags) const {
    *type_flags |= NOT_NULL_FLAG;
  }
};

/**
  Node for the @SQL{UNIQUE [KEY]} column attribute

  @ingroup ptn_column_attrs
*/
class PT_unique_key_column_attr : public PT_column_attr_base {
 public:
  virtual void apply_type_flags(ulong *type_flags) const {
    *type_flags |= UNIQUE_FLAG;
  }

  virtual void apply_alter_info_flags(uint *flags) const {
    *flags |= Alter_info::ALTER_ADD_INDEX;
  }
};

/**
  Node for the @SQL{PRIMARY [KEY]} column attribute

  @ingroup ptn_column_attrs
*/
class PT_primary_key_column_attr : public PT_column_attr_base {
 public:
  virtual void apply_type_flags(ulong *type_flags) const {
    *type_flags |= PRI_KEY_FLAG | NOT_NULL_FLAG;
  }

  virtual void apply_alter_info_flags(uint *flags) const {
    *flags |= Alter_info::ALTER_ADD_INDEX;
  }
};

/**
  Node for the @SQL{COMMENT @<comment@>} column attribute

  @ingroup ptn_column_attrs
*/
class PT_comment_column_attr : public PT_column_attr_base {
  const LEX_STRING comment;

 public:
  explicit PT_comment_column_attr(const LEX_STRING &comment)
      : comment(comment) {}

  virtual void apply_comment(LEX_STRING *to) const { *to = comment; }
};

/**
  Node for the @SQL{COLLATE @<collation@>} column attribute

  @ingroup ptn_column_attrs
*/
class PT_collate_column_attr : public PT_column_attr_base {
  const CHARSET_INFO *const collation;

 public:
  explicit PT_collate_column_attr(const CHARSET_INFO *collation)
      : collation(collation) {}

  bool apply_collation(const CHARSET_INFO **to,
                       bool *has_explicit_collation) const override {
    *has_explicit_collation = true;
    if (*to == nullptr) {
      *to = collation;
      return false;
    }
    *to = merge_charset_and_collation(*to, collation);
    return *to == nullptr;
  }
};

// Specific to non-generated columns only:

/**
  Node for the @SQL{DEFAULT @<expression@>} column attribute

  @ingroup ptn_not_gcol_attr
*/
class PT_default_column_attr : public PT_column_attr_base {
  typedef PT_column_attr_base super;

  Item *item;

 public:
  explicit PT_default_column_attr(Item *item) : item(item) {}
  virtual void apply_default_value(Item **value) const { *value = item; }

  virtual bool contextualize(Column_parse_context *pc) {
    if (pc->is_generated) {
      my_error(ER_WRONG_USAGE, MYF(0), "DEFAULT", "generated column");
      return true;
    }
    return super::contextualize(pc) || item->itemize(pc, &item);
  }
  virtual void apply_type_flags(ulong *type_flags) const {
    if (item->type() == Item::NULL_ITEM) *type_flags |= EXPLICIT_NULL_FLAG;
  }
};

/**
  Node for the @SQL{UPDATE NOW[([@<precision@>])]} column attribute

  @ingroup ptn_not_gcol_attr
*/
class PT_on_update_column_attr : public PT_column_attr_base {
  typedef PT_column_attr_base super;

  const uint8 precision;
  Item *item;

 public:
  explicit PT_on_update_column_attr(uint8 precision) : precision(precision) {}
  virtual void apply_on_update_value(Item **value) const { *value = item; }

  virtual bool contextualize(Column_parse_context *pc) {
    if (pc->is_generated) {
      my_error(ER_WRONG_USAGE, MYF(0), "ON UPDATE", "generated column");
      return true;
    }
    if (super::contextualize(pc)) return true;

    item = new (pc->thd->mem_root) Item_func_now_local(precision);
    return item == NULL;
  }
};

/**
  Node for the @SQL{AUTO_INCREMENT} column attribute

  @ingroup ptn_not_gcol_attr
*/
class PT_auto_increment_column_attr : public PT_column_attr_base {
  typedef PT_column_attr_base super;

 public:
  virtual void apply_type_flags(ulong *type_flags) const {
    *type_flags |= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG;
  }
  virtual bool contextualize(Column_parse_context *pc) {
    if (pc->is_generated) {
      my_error(ER_WRONG_USAGE, MYF(0), "AUTO_INCREMENT", "generated column");
      return true;
    }
    return super::contextualize(pc);
  }
};

/**
  Node for the @SQL{SERIAL DEFAULT VALUE} column attribute

  @ingroup ptn_not_gcol_attr
*/
class PT_serial_default_value_column_attr : public PT_column_attr_base {
  typedef PT_column_attr_base super;

 public:
  virtual void apply_type_flags(ulong *type_flags) const {
    *type_flags |= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNIQUE_FLAG;
  }
  virtual void apply_alter_info_flags(uint *flags) const {
    *flags |= Alter_info::ALTER_ADD_INDEX;
  }
  virtual bool contextualize(Column_parse_context *pc) {
    if (pc->is_generated) {
      my_error(ER_WRONG_USAGE, MYF(0), "SERIAL DEFAULT VALUE",
               "generated column");
      return true;
    }
    return super::contextualize(pc);
  }
};

/**
  Node for the @SQL{COLUMN_FORMAT @<DEFAULT|FIXED|DYNAMIC@>} column attribute

  @ingroup ptn_not_gcol_attr
*/
class PT_column_format_column_attr : public PT_column_attr_base {
  typedef PT_column_attr_base super;

  column_format_type format;

 public:
  explicit PT_column_format_column_attr(column_format_type format)
      : format(format) {}

  virtual void apply_type_flags(ulong *type_flags) const {
    *type_flags &= ~(FIELD_FLAGS_COLUMN_FORMAT_MASK);
    *type_flags |= format << FIELD_FLAGS_COLUMN_FORMAT;
  }
  virtual bool contextualize(Column_parse_context *pc) {
    if (pc->is_generated) {
      my_error(ER_WRONG_USAGE, MYF(0), "COLUMN_FORMAT", "generated column");
      return true;
    }
    return super::contextualize(pc);
  }
};

/**
  Node for the @SQL{STORAGE @<DEFAULT|DISK|MEMORY@>} column attribute

  @ingroup ptn_not_gcol_attr
*/
class PT_storage_media_column_attr : public PT_column_attr_base {
  typedef PT_column_attr_base super;

  ha_storage_media media;

 public:
  explicit PT_storage_media_column_attr(ha_storage_media media)
      : media(media) {}

  virtual void apply_type_flags(ulong *type_flags) const {
    *type_flags &= ~(FIELD_FLAGS_STORAGE_MEDIA_MASK);
    *type_flags |= media << FIELD_FLAGS_STORAGE_MEDIA;
  }
  virtual bool contextualize(Column_parse_context *pc) {
    if (pc->is_generated) {
      my_error(ER_WRONG_USAGE, MYF(0), "STORAGE", "generated column");
      return true;
    }
    return super::contextualize(pc);
  }
};

/// Node for the SRID column attribute
class PT_srid_column_attr : public PT_column_attr_base {
  typedef PT_column_attr_base super;

  gis::srid_t m_srid;

 public:
  explicit PT_srid_column_attr(gis::srid_t srid) : m_srid(srid) {}

  void apply_srid_modifier(Nullable<gis::srid_t> *srid) const override {
    *srid = m_srid;
  }
};

// Type nodes:

/**
  Base class for all column type nodes

  @ingroup ptn_column_types
*/
class PT_type : public Parse_tree_node {
 public:
  const enum_field_types type;

 protected:
  explicit PT_type(enum_field_types type) : type(type) {}

 public:
  virtual ulong get_type_flags() const { return 0; }
  virtual const char *get_length() const { return NULL; }
  virtual const char *get_dec() const { return NULL; }
  virtual const CHARSET_INFO *get_charset() const { return NULL; }
  virtual uint get_uint_geom_type() const { return 0; }
  virtual List<String> *get_interval_list() const { return NULL; }
};

/**
  Node for numeric types

  Type list:
  * NUMERIC, REAL, DOUBLE, DECIMAL and FIXED,
  * INTEGER, INT, INT1, INT2, INT3, INT4, TINYINT, SMALLINT, MEDIUMINT and
    BIGINT.

  @ingroup ptn_column_types
*/
class PT_numeric_type : public PT_type {
  const char *length;
  const char *dec;
  Field_option options;

  using Parent_type = std::remove_const<decltype(PT_type::type)>::type;

 public:
  PT_numeric_type(Numeric_type type_arg, const char *length, const char *dec,
                  Field_option options)
      : PT_type(static_cast<Parent_type>(type_arg)),
        length(length),
        dec(dec),
        options(options) {}
  PT_numeric_type(Int_type type_arg, const char *length, Field_option options)
      : PT_type(static_cast<enum_field_types>(type_arg)),
        length(length),
        dec(0),
        options(options) {}

  virtual ulong get_type_flags() const { return static_cast<ulong>(options); }
  virtual const char *get_length() const { return length; }
  virtual const char *get_dec() const { return dec; }
};

/**
  Node for the BIT type

  @ingroup ptn_column_types
*/
class PT_bit_type : public PT_type {
  const char *length;

 public:
  PT_bit_type() : PT_type(MYSQL_TYPE_BIT), length("1") {}
  explicit PT_bit_type(const char *length)
      : PT_type(MYSQL_TYPE_BIT), length(length) {}

  virtual const char *get_length() const { return length; }
};

/**
  Node for the BOOL/BOOLEAN type

  @ingroup ptn_column_types
*/
class PT_boolean_type : public PT_type {
 public:
  PT_boolean_type() : PT_type(MYSQL_TYPE_TINY) {}
  virtual const char *get_length() const { return "1"; }
};

enum class Char_type : ulong {
  CHAR = MYSQL_TYPE_STRING,
  VARCHAR = MYSQL_TYPE_VARCHAR,
  TEXT = MYSQL_TYPE_BLOB,
};

class PT_char_type : public PT_type {
  const char *length;
  const CHARSET_INFO *charset;
  const bool force_binary;

  using Parent_type = std::remove_const<decltype(PT_type::type)>::type;

 public:
  PT_char_type(Char_type char_type, const char *length,
               const CHARSET_INFO *charset, bool force_binary = false)
      : PT_type(static_cast<Parent_type>(char_type)),
        length(length),
        charset(charset),
        force_binary(force_binary) {
    DBUG_ASSERT(charset == NULL || !force_binary);
  }
  PT_char_type(Char_type char_type, const CHARSET_INFO *charset,
               bool force_binary = false)
      : PT_char_type(char_type, "1", charset, force_binary) {}
  virtual ulong get_type_flags() const {
    return force_binary ? BINCMP_FLAG : 0;
  }
  virtual const char *get_length() const { return length; }
  virtual const CHARSET_INFO *get_charset() const { return charset; }
};

enum class Blob_type {
  TINY = MYSQL_TYPE_TINY_BLOB,
  MEDIUM = MYSQL_TYPE_MEDIUM_BLOB,
  LONG = MYSQL_TYPE_LONG_BLOB,
};

/**
  Node for BLOB types

  Types: BLOB, TINYBLOB, MEDIUMBLOB, LONGBLOB, LONG, LONG VARBINARY,
         LONG VARCHAR, TEXT, TINYTEXT, MEDIUMTEXT, LONGTEXT.

  @ingroup ptn_column_types
*/
class PT_blob_type : public PT_type {
  const char *length;
  const CHARSET_INFO *charset;
  const bool force_binary;

  using Parent_type = std::remove_const<decltype(PT_type::type)>::type;

 public:
  PT_blob_type(Blob_type blob_type, const CHARSET_INFO *charset,
               bool force_binary = false)
      : PT_type(static_cast<Parent_type>(blob_type)),
        length(NULL),
        charset(charset),
        force_binary(force_binary) {
    DBUG_ASSERT(charset == NULL || !force_binary);
  }
  explicit PT_blob_type(const char *length)
      : PT_type(MYSQL_TYPE_BLOB),
        length(length),
        charset(&my_charset_bin),
        force_binary(false) {}

  virtual ulong get_type_flags() const {
    return force_binary ? BINCMP_FLAG : 0;
  }
  virtual const CHARSET_INFO *get_charset() const { return charset; }
  virtual const char *get_length() const { return length; }
};

/**
  Node for the YEAR type

  @ingroup ptn_column_types
*/
class PT_year_type : public PT_type {
 public:
  PT_year_type() : PT_type(MYSQL_TYPE_YEAR) {}
};

/**
  Node for the DATE type

  @ingroup ptn_column_types
*/
class PT_date_type : public PT_type {
 public:
  PT_date_type() : PT_type(MYSQL_TYPE_DATE) {}
};

enum class Time_type : ulong {
  TIME = MYSQL_TYPE_TIME2,
  DATETIME = MYSQL_TYPE_DATETIME2,
};

/**
  Node for the TIME, TIMESTAMP and DATETIME types

  @ingroup ptn_column_types
*/
class PT_time_type : public PT_type {
  const char *dec;

  using Parent_type = std::remove_const<decltype(PT_type::type)>::type;

 public:
  PT_time_type(Time_type time_type, const char *dec)
      : PT_type(static_cast<Parent_type>(time_type)), dec(dec) {}

  virtual const char *get_dec() const { return dec; }
};

/**
  Node for the TIMESTAMP type

  @ingroup ptn_column_types
*/
class PT_timestamp_type : public PT_type {
  typedef PT_type super;

  const char *dec;
  ulong type_flags;

 public:
  explicit PT_timestamp_type(const char *dec)
      : super(MYSQL_TYPE_TIMESTAMP2), dec(dec), type_flags(0) {}

  virtual const char *get_dec() const { return dec; }
  virtual ulong get_type_flags() const { return type_flags; }

  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc)) return true;
    /*
      TIMESTAMP fields are NOT NULL by default, unless the variable
      explicit_defaults_for_timestamp is true.
    */
    if (!pc->thd->variables.explicit_defaults_for_timestamp)
      type_flags = NOT_NULL_FLAG;
    /*
      To flag the current statement as dependent for binary
      logging on the session var. Extra copying to Lex is
      done in case prepared stmt.
    */
    pc->thd->lex->binlog_need_explicit_defaults_ts =
        pc->thd->binlog_need_explicit_defaults_ts = true;

    return false;
  }
};

/**
  Node for spatial types

  Types: GEOMETRY, GEOMCOLLECTION/GEOMETRYCOLLECTION, POINT, MULTIPOINT,
         LINESTRING, MULTILINESTRING, POLYGON, MULTIPOLYGON

  @ingroup ptn_column_types
*/
class PT_spacial_type : public PT_type {
  Field::geometry_type geo_type;

 public:
  explicit PT_spacial_type(Field::geometry_type geo_type)
      : PT_type(MYSQL_TYPE_GEOMETRY), geo_type(geo_type) {}

  virtual const CHARSET_INFO *get_charset() const { return &my_charset_bin; }
  virtual uint get_uint_geom_type() const { return geo_type; }
  virtual const char *get_length() const {
    if (geo_type == Field::GEOM_POINT)
      return STRINGIFY_ARG(MAX_LEN_GEOM_POINT_FIELD);
    else
      return NULL;
  }
};

enum class Enum_type { ENUM = MYSQL_TYPE_ENUM, SET = MYSQL_TYPE_SET };

template <Enum_type enum_type>
class PT_enum_type_tmpl : public PT_type {
  List<String> *const interval_list;
  const CHARSET_INFO *charset;
  const bool force_binary;

  using Parent_type = std::remove_const<decltype(PT_type::type)>::type;

 public:
  PT_enum_type_tmpl(List<String> *interval_list, const CHARSET_INFO *charset,
                    bool force_binary)
      : PT_type(static_cast<Parent_type>(enum_type)),
        interval_list(interval_list),
        charset(charset),
        force_binary(force_binary) {
    DBUG_ASSERT(charset == NULL || !force_binary);
  }

  virtual const CHARSET_INFO *get_charset() const { return charset; }
  virtual ulong get_type_flags() const {
    return force_binary ? BINCMP_FLAG : 0;
  }
  virtual List<String> *get_interval_list() const { return interval_list; }
};

/**
  Node for the ENUM type

  @ingroup ptn_column_types
*/
typedef PT_enum_type_tmpl<Enum_type::ENUM> PT_enum_type;

/**
  Node for the SET type

  @ingroup ptn_column_types
*/
typedef PT_enum_type_tmpl<Enum_type::SET> PT_set_type;

class PT_serial_type : public PT_type {
 public:
  PT_serial_type() : PT_type(MYSQL_TYPE_LONGLONG) {}

  virtual ulong get_type_flags() const {
    return AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNSIGNED_FLAG | UNIQUE_FLAG;
  }
};

/**
  Node for the JSON type

  @ingroup ptn_column_types
*/
class PT_json_type : public PT_type {
 public:
  PT_json_type() : PT_type(MYSQL_TYPE_JSON) {}
  virtual const CHARSET_INFO *get_charset() const { return &my_charset_bin; }
};

/**
  Base class for both generated and regular column definitions

  @ingroup ptn_create_table
*/
class PT_field_def_base : public Parse_tree_node {
  typedef Parse_tree_node super;
  typedef decltype(Alter_info::flags) alter_info_flags_t;

 public:
  enum_field_types type;
  ulong type_flags;
  const char *length;
  const char *dec;
  const CHARSET_INFO *charset;
  bool has_explicit_collation;
  uint uint_geom_type;
  List<String> *interval_list;
  alter_info_flags_t alter_info_flags;
  LEX_STRING comment;
  Item *default_value;
  Item *on_update_value;
  Generated_column *gcol_info;
  Nullable<gis::srid_t> m_srid;

 protected:
  PT_type *type_node;

  explicit PT_field_def_base(PT_type *type_node)
      : has_explicit_collation(false),
        alter_info_flags(0),
        comment(EMPTY_STR),
        default_value(NULL),
        on_update_value(NULL),
        gcol_info(NULL),
        type_node(type_node) {}

 public:
  virtual bool contextualize(Parse_context *pc) {
    if (super::contextualize(pc) || type_node->contextualize(pc)) return true;

    type = type_node->type;
    type_flags = type_node->get_type_flags();
    length = type_node->get_length();
    dec = type_node->get_dec();
    charset = type_node->get_charset();
    uint_geom_type = type_node->get_uint_geom_type();
    interval_list = type_node->get_interval_list();
    return false;
  }

 protected:
  template <class T>
  bool contextualize_attrs(Column_parse_context *pc,
                           Mem_root_array<T *> *attrs) {
    if (attrs != NULL) {
      for (auto attr : *attrs) {
        if (attr->contextualize(pc)) return true;
        attr->apply_type_flags(&type_flags);
        attr->apply_alter_info_flags(&alter_info_flags);
        attr->apply_comment(&comment);
        attr->apply_default_value(&default_value);
        attr->apply_on_update_value(&on_update_value);
        attr->apply_srid_modifier(&m_srid);
        if (attr->apply_collation(&charset, &has_explicit_collation))
          return true;
      }
    }
    return false;
  }
};

/**
  Base class for regular (non-generated) column definition nodes

  @ingroup ptn_create_table
*/
class PT_field_def : public PT_field_def_base {
  typedef PT_field_def_base super;

  Mem_root_array<PT_column_attr_base *> *opt_attrs;

 public:
  PT_field_def(PT_type *type_node_arg,
               Mem_root_array<PT_column_attr_base *> *opt_attrs)
      : super(type_node_arg), opt_attrs(opt_attrs) {}

  virtual bool contextualize(Parse_context *pc_arg) {
    Column_parse_context pc(pc_arg->thd, pc_arg->select, false);
    return super::contextualize(&pc) || contextualize_attrs(&pc, opt_attrs);
  }
};

/**
  Base class for generated column definition nodes

  @ingroup ptn_create_table
*/
class PT_generated_field_def : public PT_field_def_base {
  typedef PT_field_def_base super;

  const Virtual_or_stored virtual_or_stored;
  Item *expr;
  Mem_root_array<PT_column_attr_base *> *opt_attrs;

 public:
  PT_generated_field_def(PT_type *type_node_arg, Item *expr,
                         Virtual_or_stored virtual_or_stored,
                         Mem_root_array<PT_column_attr_base *> *opt_attrs)
      : super(type_node_arg),
        virtual_or_stored(virtual_or_stored),
        expr(expr),
        opt_attrs(opt_attrs) {}

  virtual bool contextualize(Parse_context *pc_arg) {
    Column_parse_context pc(pc_arg->thd, pc_arg->select, true);
    if (super::contextualize(&pc) || contextualize_attrs(&pc, opt_attrs) ||
        expr->itemize(&pc, &expr))
      return true;

    gcol_info = new (pc.thd->mem_root) Generated_column;
    if (gcol_info == NULL) return true;  // OOM
    gcol_info->expr_item = expr;
    if (virtual_or_stored == Virtual_or_stored::STORED)
      gcol_info->set_field_stored(true);
    gcol_info->set_field_type(type);

    return false;
  }
};

#endif /* PARSE_TREE_COL_ATTRS_INCLUDED */
