/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef KEY_SPEC_INCLUDED
#define KEY_SPEC_INCLUDED

#include "my_global.h"
#include "m_string.h"
#include "sql_alloc.h"
#include "sql_lex.h"


class Key_part_spec :public Sql_alloc {
public:
  LEX_STRING field_name;
  uint length;
  Key_part_spec(const LEX_STRING &name, uint len)
    : field_name(name), length(len)
  {}
  Key_part_spec(const char *name, const size_t name_len, uint len)
    : length(len)
  { field_name.str= (char *)name; field_name.length= name_len; }
  bool operator==(const Key_part_spec& other) const;
  /**
    Construct a copy of this Key_part_spec. field_name is copied
    by-pointer as it is known to never change. At the same time
    'length' may be reset in mysql_prepare_create_table, and this
    is why we supply it with a copy.

    @return If out of memory, 0 is returned and an error is set in
    THD.
  */
  Key_part_spec *clone(MEM_ROOT *mem_root) const
  { return new (mem_root) Key_part_spec(*this); }
};


class Key_spec :public Sql_alloc {
public:
  keytype type;
  KEY_CREATE_INFO key_create_info;
  List<Key_part_spec> columns;
  LEX_STRING name;
  bool generated;

  Key_spec(keytype type_par, const LEX_STRING &name_arg,
           KEY_CREATE_INFO *key_info_arg,
           bool generated_arg, List<Key_part_spec> &cols)
    :type(type_par), key_create_info(*key_info_arg), columns(cols),
    name(name_arg), generated(generated_arg)
  {}
  Key_spec(keytype type_par, const char *name_arg, size_t name_len_arg,
           KEY_CREATE_INFO *key_info_arg, bool generated_arg,
           List<Key_part_spec> &cols)
    :type(type_par), key_create_info(*key_info_arg), columns(cols),
    generated(generated_arg)
  {
    name.str= (char *)name_arg;
    name.length= name_len_arg;
  }
  Key_spec(const Key_spec &rhs, MEM_ROOT *mem_root);
  virtual ~Key_spec() {}

  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  virtual Key_spec *clone(MEM_ROOT *mem_root) const
  { return new (mem_root) Key_spec(*this, mem_root); }
};


class Foreign_key_spec: public Key_spec {
public:

  LEX_CSTRING ref_db;
  LEX_CSTRING ref_table;
  List<Key_part_spec> ref_columns;
  uint delete_opt, update_opt, match_opt;
  Foreign_key_spec(const LEX_STRING &name_arg, List<Key_part_spec> &cols,
                   const LEX_CSTRING &ref_db_arg,
                   const LEX_CSTRING &ref_table_arg,
                   List<Key_part_spec> &ref_cols,
                   uint delete_opt_arg, uint update_opt_arg, uint match_opt_arg)
    :Key_spec(KEYTYPE_FOREIGN, name_arg, &default_key_create_info, 0, cols),
    ref_db(ref_db_arg), ref_table(ref_table_arg), ref_columns(ref_cols),
    delete_opt(delete_opt_arg), update_opt(update_opt_arg),
    match_opt(match_opt_arg)
  {
    // We don't check for duplicate FKs.
    key_create_info.check_for_duplicate_indexes= false;
  }
  Foreign_key_spec(const Foreign_key_spec &rhs, MEM_ROOT *mem_root);
  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  virtual Key_spec *clone(MEM_ROOT *mem_root) const
  { return new (mem_root) Foreign_key_spec(*this, mem_root); }
  /* Used to validate foreign key options */
  bool validate(List<Create_field> &table_fields);
};

/* Equality comparison of keys (ignoring name) */
bool foreign_key_prefix(Key_spec *a, Key_spec *b);



#endif  // KEY_SPEC_INCLUDED
