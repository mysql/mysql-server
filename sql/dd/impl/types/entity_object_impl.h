/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__ENTITY_OBJECT_IMPL_INCLUDED
#define DD__ENTITY_OBJECT_IMPL_INCLUDED

#include "my_global.h"

#include "dd/sdi_fwd.h"
#include "dd/impl/types/weak_object_impl.h" // Weak_object_impl
#include "dd/types/entity_object.h"         // Entity_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Entity_object_impl : virtual public Entity_object,
                           public Weak_object_impl
{
public:
  Entity_object_impl()
   :m_id(INVALID_OBJECT_ID), m_has_new_primary_key(true)
  { }

  virtual ~Entity_object_impl()
  { }

public:
  virtual Object_id id() const
  { return m_id; }

  /* non-virtual */ void set_id(Object_id id)
  {
    m_id= id;
    fix_has_new_primary_key();
  }

  /* purecov: begin deadcode */
  virtual bool is_persistent() const
  { return (m_id != INVALID_OBJECT_ID); }
  /* purecov: end */

  virtual const std::string &name() const
  { return m_name; }

  virtual void set_name(const std::string &name)
  { m_name= name; }

  virtual Object_key *create_primary_key() const;

  virtual bool has_new_primary_key() const
  { return m_has_new_primary_key; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }

protected:
  virtual void set_primary_key_value(const Raw_new_record &r);

  virtual void fix_has_new_primary_key()
  { m_has_new_primary_key= (m_id == INVALID_OBJECT_ID); }

  void restore_id(const Raw_record &r, int field_idx);
  void restore_name(const Raw_record &r, int field_idx);

  bool store_id(Raw_record *r, int field_idx);
  bool store_name(Raw_record *r, int field_idx);
  bool store_name(Raw_record *r, int field_idx, bool is_null);

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;
  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

private:
  // NOTE: ID and Name attributes *must* remain private so that we can track
  // changes in them and prevent abuse.

  Object_id m_id;

  std::string m_name;

  /**
    Indicates that object is guaranteed to have ID which doesn't exist in
    database because it will be or just was generated using auto-increment.
    Main difference of this member from result of m_id == INVALID_OBJECT_ID
    check is that we delay resetting of this flag until end of store() method
    while m_id is updated right after object was inserted into the table.
    This is necessary to let entity's children figure out that their parent
    has new ID which was not used before (and hence their primary keys based
    on this ID will be new too) while still giving access to the exact value
    of new ID.
  */
  bool m_has_new_primary_key;

protected:
  // The generated copy constructor could have been used,
  // but by adding this we force derived classes which define
  // their own copy constructor to also invoke the Entity_object_impl
  // copy constructor in the initializer list.
  // Note that we must copy the m_has_new_primary_key property to make sure
  // the clone is handled correctly if storing it persistently as part of
  // updating a DD object.
  Entity_object_impl(const Entity_object_impl &src)
    : Weak_object(src), m_id(src.m_id), m_name(src.m_name),
            m_has_new_primary_key(src.m_has_new_primary_key)
  {}
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__ENTITY_OBJECT_IMPL_INCLUDED
