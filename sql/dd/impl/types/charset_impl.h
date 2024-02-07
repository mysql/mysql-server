/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#ifndef DD__CHARSET_IMPL_INCLUDED
#define DD__CHARSET_IMPL_INCLUDED

#include <stdio.h>
#include <sys/types.h>
#include <new>

#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_imp
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/charset.h"  // dd::Charset

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table;
class Open_dictionary_tables_ctx;
class Raw_record;
class Weak_object;
class Object_table;

///////////////////////////////////////////////////////////////////////////

class Charset_impl : public Entity_object_impl, public Charset {
 public:
  Charset_impl()
      : m_mb_max_length(0), m_default_collation_id(INVALID_OBJECT_ID) {}

 public:
  const Object_table &object_table() const override;

  static void register_tables(Open_dictionary_tables_ctx *otx);

  bool validate() const override;

  bool restore_attributes(const Raw_record &r) override;

  bool store_attributes(Raw_record *r) override;

 public:
  /////////////////////////////////////////////////////////////////////////
  // Default collation.
  /////////////////////////////////////////////////////////////////////////

  Object_id default_collation_id() const override {
    return m_default_collation_id;
  }

  void set_default_collation_id(Object_id collation_id) override {
    m_default_collation_id = collation_id;
  }

  /////////////////////////////////////////////////////////////////////////
  // mb_max_length
  /////////////////////////////////////////////////////////////////////////

  uint mb_max_length() const override { return m_mb_max_length; }

  virtual void set_mb_max_length(uint mb_max_length) {
    m_mb_max_length = mb_max_length;
  }

  /////////////////////////////////////////////////////////////////////////
  // comment
  /////////////////////////////////////////////////////////////////////////

  const String_type &comment() const override { return m_comment; }

  virtual void set_comment(const String_type &comment) { m_comment = comment; }

  // Fix "inherits ... via dominance" warnings
  Entity_object_impl *impl() override { return Entity_object_impl::impl(); }
  const Entity_object_impl *impl() const override {
    return Entity_object_impl::impl();
  }
  Object_id id() const override { return Entity_object_impl::id(); }
  bool is_persistent() const override {
    return Entity_object_impl::is_persistent();
  }
  const String_type &name() const override {
    return Entity_object_impl::name();
  }
  void set_name(const String_type &name) override {
    Entity_object_impl::set_name(name);
  }

 public:
  void debug_print(String_type &outb) const override {
    char outbuf[1024];
    sprintf(outbuf,
            "CHARSET OBJECT: {OID: %lld}, name= %s, "
            "collation_id= {OID: %lld}, mb_max_length= %u, "
            "comment= %s",
            id(), name().c_str(), m_default_collation_id, m_mb_max_length,
            m_comment.c_str());
    outb = String_type(outbuf);
  }

 private:
  uint m_mb_max_length;
  String_type m_comment;

  Object_id m_default_collation_id;

  Charset *clone() const override { return new Charset_impl(*this); }

  Charset *clone_dropped_object_placeholder() const override {
    /*
      Even though we don't drop charsets en masse we still create slimmed
      down version for consistency sake.
    */
    Charset_impl *placeholder = new Charset_impl();
    placeholder->set_id(id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__CHARSET_IMPL_INCLUDED
