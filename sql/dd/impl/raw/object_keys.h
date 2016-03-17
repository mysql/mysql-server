/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__OBJECT_KEYS_INCLUDED
#define DD__OBJECT_KEYS_INCLUDED

#include "my_global.h"

#include "m_ctype.h"

#include "dd/object_id.h"        // dd::Object_id
#include "dd/impl/object_key.h"  // dd::Object_key

extern "C" MYSQL_PLUGIN_IMPORT CHARSET_INFO *system_charset_info;

namespace dd {

///////////////////////////////////////////////////////////////////////////

struct Raw_key;
class Raw_table;

///////////////////////////////////////////////////////////////////////////

// NOTE: the current naming convention is as follows:
// - use '_key' suffix to name keys identifying 0 or 1 row;
// - use '_range_key' suffix to name keys identifying 0 or N rows.

///////////////////////////////////////////////////////////////////////////

// Key type to be used for keys that are not supported by an object type.
class Void_key : public Object_key
{
public:
  Void_key()
  { }

public:
  /* purecov: begin inspected */
  virtual Raw_key *create_access_key(Raw_table *) const
  { return NULL; }
  /* purecov: end */

  /* purecov: begin inspected */
  virtual std::string str() const
  { return ""; }
  /* purecov: end */

  // We need a comparison operator since the type will be used
  // as a template type argument.
  /* purecov: begin inspected */
  bool operator <(const Void_key &rhs) const
  { return this < &rhs; }
  /* purecov: end */
};

///////////////////////////////////////////////////////////////////////////

// Entity_object-id primary key for global objects.
class Primary_id_key : public Object_key
{
public:
  Primary_id_key()
  { }

  Primary_id_key(Object_id object_id)
   :m_object_id(object_id)
  { }

  // Update a preallocated instance.
  void update(Object_id object_id)
  { m_object_id= object_id; }

public:
  virtual Raw_key *create_access_key(Raw_table *db_table) const;

  virtual std::string str() const;

  bool operator <(const Primary_id_key &rhs) const
  { return m_object_id < rhs.m_object_id; }

private:
  Object_id m_object_id;
};

///////////////////////////////////////////////////////////////////////////

// Entity_object-id partial key for looking for containing objects.
class Parent_id_range_key : public Object_key
{
public:
  Parent_id_range_key(int id_index_no,
                      int id_column_no,
                      Object_id object_id)
   :m_id_index_no(id_index_no),
    m_id_column_no(id_column_no),
    m_object_id(object_id)
  { }

public:
  virtual Raw_key *create_access_key(Raw_table *db_table) const;

  virtual std::string str() const;

private:
  int m_id_index_no;
  int m_id_column_no;
  Object_id m_object_id;
};

///////////////////////////////////////////////////////////////////////////

// Entity_object-name key for global objects.
class Global_name_key : public Object_key
{
public:
  Global_name_key()
  { }

  Global_name_key(int name_column_no,
                  const std::string &object_name)
   :m_name_column_no(name_column_no),
    m_object_name(object_name)
  { }

  // Update a preallocated instance.
  void update(int name_column_no,
              const std::string &object_name)
  {
    m_name_column_no= name_column_no;
    m_object_name= object_name;
  }

public:
  virtual Raw_key *create_access_key(Raw_table *db_table) const;

  /* purecov: begin inspected */
  virtual std::string str() const
  { return m_object_name; }
  /* purecov: end */

  bool operator <(const Global_name_key &rhs) const
  { return m_object_name < rhs.m_object_name; }

private:
  int m_name_column_no;
  std::string m_object_name;
};

///////////////////////////////////////////////////////////////////////////

// Entity_object-name key for objects which are identified within a container.
class Item_name_key : public Object_key
{
public:
  Item_name_key()
  { }

  Item_name_key(int container_id_column_no,
                Object_id container_id,
                int name_column_no,
                const std::string &object_name)
   :m_container_id_column_no(container_id_column_no),
    m_name_column_no(name_column_no),
    m_container_id(container_id),
    m_object_name(object_name)
  { }

  // Update a preallocated instance.
  void update(int container_id_column_no,
              Object_id container_id,
              int name_column_no,
              const std::string &object_name)
  {
    m_container_id_column_no= container_id_column_no;
    m_name_column_no= name_column_no;
    m_container_id= container_id;
    m_object_name= object_name;
  }

public:
  virtual Raw_key *create_access_key(Raw_table *db_table) const;

  virtual std::string str() const;

  bool operator <(const Item_name_key &rhs) const
  {
    return m_container_id < rhs.m_container_id ? true
         : rhs.m_container_id < m_container_id ? false
         : m_object_name < rhs.m_object_name;
  }

private:
  int m_container_id_column_no;
  int m_name_column_no;

  Object_id m_container_id;
  std::string m_object_name;
};

///////////////////////////////////////////////////////////////////////////

// TODO: find a better name.
class Se_private_id_key : public Object_key
{
public:
  Se_private_id_key()
  { }

/* purecov: begin deadcode */
  Se_private_id_key(int index_no,
                    int engine_column_no,
                    const std::string &engine,
                    int private_id_column_no,
                    Object_id private_id)
   :m_index_no(index_no),
    m_engine_column_no(engine_column_no),
    m_engine(&engine),
    m_private_id_column_no(private_id_column_no),
    m_private_id(private_id)
  { }
/* purecov: end */

  // Update a preallocated instance.
  void update(int index_no,
              int engine_column_no,
              const std::string &engine,
              int private_id_column_no,
              Object_id private_id)
  {
    m_index_no= index_no;
    m_engine_column_no= engine_column_no;
    m_engine= &engine;
    m_private_id_column_no= private_id_column_no;
    m_private_id= private_id;
  }

public:
  virtual Raw_key *create_access_key(Raw_table *db_table) const;

  virtual std::string str() const;

  bool operator <(const Se_private_id_key &rhs) const
  { return m_private_id < rhs.m_private_id; }

private:
  int m_index_no;

  int m_engine_column_no;
  const std::string *m_engine;

  int m_private_id_column_no;
  Object_id m_private_id;
};

///////////////////////////////////////////////////////////////////////////

class Composite_pk : public Object_key
{
public:
  Composite_pk(int index_no,
               uint first_column_no,
               ulonglong first_id,
               uint second_column_no,
               ulonglong second_id
               )
   :m_index_no(index_no),
    m_first_column_no(first_column_no),
    m_first_id(first_id),
    m_second_column_no(second_column_no),
    m_second_id(second_id)
  { }

public:
  virtual Raw_key *create_access_key(Raw_table *db_table) const;

  virtual std::string str() const;

private:
  int m_index_no;

  int m_first_column_no;
  ulonglong m_first_id;

  int m_second_column_no;
  ulonglong m_second_id;
};

///////////////////////////////////////////////////////////////////////////

class Routine_name_key : public Object_key
{
public:
  Routine_name_key()
  { }

  Routine_name_key(int container_id_column_no,
                   Object_id container_id,
                   int type_column_no,
                   uint type,
                   int name_column_no,
                   const std::string &object_name)
   :m_container_id_column_no(container_id_column_no),
    m_type_column_no(type_column_no),
    m_name_column_no(name_column_no),
    m_container_id(container_id),
    m_type(type),
    m_object_name(object_name)
  { }

  // Update a preallocated instance.
  void update(int container_id_column_no,
              Object_id container_id,
              int type_column_no,
              uint type,
              int name_column_no,
              const std::string &object_name)
  {
    m_container_id_column_no= container_id_column_no;
    m_type_column_no= type_column_no;
    m_name_column_no= name_column_no;
    m_container_id= container_id;
    m_type= type;
    m_object_name= object_name;
  }

public:
  virtual Raw_key *create_access_key(Raw_table *db_table) const;

  virtual std::string str() const;

  bool operator <(const Routine_name_key &rhs) const;

private:
  int m_container_id_column_no;
  int m_type_column_no;
  int m_name_column_no;

  Object_id m_container_id;
  uint m_type;
  std::string m_object_name;
};

///////////////////////////////////////////////////////////////////////////

}
#endif // DD__OBJECT_KEYS_INCLUDED
