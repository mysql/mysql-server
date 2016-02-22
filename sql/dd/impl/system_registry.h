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

#ifndef DD__SYSTEM_REGISTRY_INCLUDED
#define DD__SYSTEM_REGISTRY_INCLUDED

#include "my_global.h"

#include <vector>
#include <map>
#include <utility>
#include <string>

namespace dd {

class Object_table;


/**
  Class to wrap an entity object.

  The Entity_element instances are intended used in registries to support
  classification of meta data. The Entity_element template points to an
  instance of some type, and also has a property member that is associated
  with the entity, as well as a key with which the object is associated.

  @note The entity object ownership is decided by the relevant template
        parameter, see below.

  @tparam K    A key type, e.g. a pair of strings.
  @tparam T    An entity type, e.g. an Object_table instance.
  @tparam P    A property type, e.g. an enumeration.
  @tparam F    Function to map from property to name.
  @tparam D    Boolean to decide ownership of wrapped object.
*/

template <typename K, typename T, typename P, const char *F(P), bool D>
class Entity_element
{
private:
  const K m_key;      //< The key associated with the entity object.
  const T *m_entity;  //< Entity object pointer, e.g. an Object_table instance.
  const P m_property; //< Property of some kind, e.g. an enumeration.

public:
  Entity_element(const K &key, const T *entity, const P property):
    m_key(key), m_entity(entity), m_property(property)
  { }

  ~Entity_element()
  {
    // Delete the wrapped object depending on the template parameter.
    if (D)
      delete m_entity;
  }

  const K &key() const
  { return m_key; }

  const T *entity() const
  { return m_entity; }

  const P property() const
  { return m_property; }

#ifndef DBUG_OFF
  void dump()
  {
    fprintf(stderr, "Key= '%s.%s', property= '%s'\n",
            m_key.first.c_str(), m_key.second.c_str(),
            F(m_property));
  }
#endif
};


/**
  Class to represent collections of meta data for entities.

  This class is used to represent relevant meta data and properties of
  system related entities, e.g. all the dictionary tables. The meta data
  is associated with a key instance, usually a std::pair containing the
  schema- and entity names.

  The class supports adding and retrieving objects, as well as iteration
  based on the order of inserts.

  @note There is no support for concurrency. We assume that the registry
        is created and that the entities are inserted in a single threaded
        situation, e.g. while the server is being started. Read only access
        can happen from several threads simultaneously, though.

  @tparam   K   Key type.
  @tparam   T   Entity type.
  @tparam   P   Property type.
  @tparam   F   Function to map from property to name.
  @tparam   D   Boolean flag to decide whether the wrapped objects
                are owned externally.
*/

template <typename K, typename T, typename P, const char *F(P), bool D>
class Entity_registry
{
private:
  typedef Entity_element<K, T, P, F, D>       Entity_element_type;
  typedef std::vector<Entity_element_type*>   Entity_list_type;
  typedef std::map<K, Entity_element_type*>   Entity_map_type;

  Entity_list_type m_entity_list; //< List for ordered access.
  Entity_map_type  m_entity_map;  //< Map for direct key based lookup.

public:
  // Externally available iteration is based on the order of inserts.
  typedef typename Entity_list_type::iterator Iterator;


  /**
    Delete the heap memory owned by the entity registry.

    Only the wrapper elements are owned by the registry.
    The std::vector and std::map are not allocated
    dynamically, and their destructors are called implicitly
    when this destructor is called.
  */

  ~Entity_registry()
  {
    // Delete elements from the map. The objects that are wrapped
    // will be handled by the wrapper destructor.
    for (typename Entity_map_type::iterator it= m_entity_map.begin();
         it != m_entity_map.end(); ++it)
      delete it->second;
  }


  /**
    Add a new entity to the registry.

    This method will create a new key and a new entity wrapper element. The
    wrapper will be added to the map, along with the key, and to the vector
    to keep track of the order of inserts.

    @param schema_name  Schema name used to construct the key.
    @param entity_name  Entity name used to construct the key.
    @param property     Property used to classify the entity.
    @param entity       Entity which is classified and registered.
  */

  void add(const std::string &schema_name, const std::string &entity_name,
           P property, T *entity)
  {
    // Create a new key and make sure it does not already exist.
    K key(schema_name, entity_name);
    DBUG_ASSERT(m_entity_map.find(key) == m_entity_map.end());

    // Create a new entity wrapper element. The wrapper will be owned by the
    // entity map and deleted along with it.
    Entity_element_type *element= new (std::nothrow)
            Entity_element_type(key, entity, property);

    // Add the key, value pair to the map.
    m_entity_map.insert(typename Entity_map_type::value_type(key, element));

    // Add the entity to the ordered list too.
    m_entity_list.push_back(element);
  }


  /**
    Find an element in the registry.

    This method creates a key based on the submitted parameters, and
    looks up in the member map. If the key is found, the object pointed
    to by the wrapper is returned, otherwise, NULL is returned.

    @param schema_name   Schema containing the entity searched for.
    @param entity_name   Name  of the entity searched for.
    @retval              NULL if not found, otherwise, the entity
                         object pointed to by the wrapper element.
  */

  const T *find(const std::string &schema_name, const std::string &entity_name)
  {
    // Create a new key. This is only used for lookup, so it is allocated
    // on the stack.
    K key(schema_name, entity_name);

    // Lookup in the map based on the key.
    typename Entity_map_type::iterator element_it= m_entity_map.find(key);

    // Return NULL if not found, otherwise, return a pointer to the entity.
    if (element_it == m_entity_map.end())
      return NULL;

    return element_it->second->entity();
  }


  /**
    Get the beginning of the vector of ordered inserts.

    @retval Iterator referring to the first element in the vector,
            or end() if the vector is empty.
  */

  Iterator begin()
  { return m_entity_list.begin(); }


  /**
    Get the first element with a certain property.

    This method retrieves the first element with a certain property, and
    is used for iterating only over elements having this property.

    @param property  Property that the element should have.
    @retval          Iterator referring to the first element in the vector
                     with the submitted property, or end().
  */

  Iterator begin(P property)
  {
    Iterator it= begin();
    while (it != end())
    {
      if ((*it)->property() == property)
        break;
      ++it;
    }

    return it;
  }


  /**
    Get the end of the vector of ordered inserts.

    @retval Iterator referring to the special element after the last "real"
            element in the vector.
  */

  Iterator end()
  { return m_entity_list.end(); }


  /**
    Get the next element in the list of ordered inserts.

    @param current  The current iterator.
    @retval         Iterator referring to the next element in the vector.
  */

  Iterator next(Iterator current)
  {
    if (current == end())
      return current;

    return ++current;
  }


  /**
    Get the next element in the list of ordered inserts.

    This method retrieves the next element with a certain property, and
    is used for iterating only over elements having this property.

    @param current   The current iterator.
    @param property  Property that the next element should have.
    @retval          Iterator referring to the next element in the vector
                     with the submitted property.
  */

  Iterator next(Iterator current, P property)
  {
    if (current == end())
      return current;

    while (++current != end())
      if ((*current)->property() == property)
        break;

    return current;
  }

#ifndef DBUG_OFF
  void dump()
  {
    // List the entities in the order they were inserted.
    for (Iterator it= begin(); it != end(); ++it)
      (*it)->dump();
  }
#endif
};


/**
  Class used to represent the dictionary tables.

  This class is a singleton used to represent meta data of the dictionary
  tables, i.e., the tables that store meta data about dictionary entities.
  The meta data collected here are the Object_table instances, which are
  used to e.g. get hold of the definition of the table.

  The singleton contains an instance of the Entity_registry class, and
  has methods that mostly delegate to this instance.
 */

class System_tables
{
public:
  // Classification of system tables.
  enum class Types
  {
    CORE,
    DDSE
  };

  // Map from system table type to string description, e.g. for debugging.
  static const char *type_name(Types type)
  {
    switch (type)
    {
      case Types::CORE: return "CORE";
      case Types::DDSE: return "DDSE";
      default:          return "";
    }
  }

private:
  // The actual registry is referred and delegated to rather than
  // being inherited from.
  typedef Entity_registry<std::pair<const std::string, const std::string>,
          const Object_table, Types,
          type_name, true> System_table_registry_type;
  System_table_registry_type m_registry;

public:
  // The ordered iterator type must be public.
  typedef System_table_registry_type::Iterator Iterator;

  static System_tables *instance()
  {
    static System_tables s_instance;
    return &s_instance;
  }

  // Add predefined system tables.
  void init();

  // Add a new system table by delegation to the wrapped registry.
  void add(const std::string &schema_name, const std::string &table_name,
           Types type, const Object_table *table)
  { m_registry.add(schema_name, table_name, type, table); }

  // Find a system table by delegation to the wrapped registry.
  const Object_table *find(const std::string &schema_name,
                           const std::string &table_name)
  { return m_registry.find(schema_name, table_name); }

  Iterator begin()
  { return m_registry.begin(); }

  Iterator begin(Types type)
  { return m_registry.begin(type); }

  Iterator end()
  { return m_registry.end(); }

  Iterator next(Iterator current, Types type)
  { return m_registry.next(current, type); }

#ifndef DBUG_OFF
  void dump()
  { m_registry.dump(); }
#endif
};


/**
  Class used to represent the system views.

  This class is a singleton used to represent meta data of the system
  views, i.e., the views that are available through the information schema.

  @note The registry currently only stores the view names and their
        (dummy) classification.

  The singleton contains an instance of the Entity_registry class, and
  has methods that mostly delegate to this instance.
 */

class System_views
{
public:
  // Dummy class acting as meta data placeholder in liu of WL#6599.
  class System_view
  { };

  // Classification of system views.
  enum class Types
  {
    INFORMATION_SCHEMA,
  };

  // Map from system view type to string description, e.g. for debugging.
  static const char *type_name(Types type)
  {
    switch (type)
    {
      case Types::INFORMATION_SCHEMA: return "INFORMATION_SCHEMA";
      default:                        return "";
    }
  }

private:
  // The actual registry is referred and delegated to rather than
  // being inherited from.
  typedef Entity_registry<std::pair<const std::string, const std::string>,
          System_view, Types, type_name, true> System_view_registry_type;
  System_view_registry_type m_registry;

public:
  // The ordered iterator type must be public.
  typedef System_view_registry_type::Iterator Iterator;

  static System_views *instance()
  {
    static System_views s_instance;
    return &s_instance;
  }

  // Add predefined system views.
  void init();

  // Add a new system view by delegation to the wrapped registry.
  void add(const std::string &schema_name, const std::string &view_name,
           Types type)
  {
    m_registry.add(schema_name, view_name, type,
                   new (std::nothrow) System_view());
  }

  // Find a system view by delegation to the wrapped registry.
  const System_view *find(const std::string &schema_name,
                          const std::string &view_name)
  { return m_registry.find(schema_name, view_name); }

  Iterator begin()
  { return m_registry.begin(); }

  Iterator begin(Types type)
  { return m_registry.begin(type); }

  Iterator end()
  { return m_registry.end(); }

  Iterator next(Iterator current, Types type)
  { return m_registry.next(current, type); }

#ifndef DBUG_OFF
  void dump()
  { m_registry.dump(); }
#endif
};


/**
  Class used to represent the system tablespaces.

  This class is a singleton used to represent meta data of the system
  tablespaces, i.e., the tablespaces that are predefined in the DDSE, or
  needed by the SQL layer.

  @note The registry currently only stores the tablespace names and their
        (dummy) classification.

  The singleton contains an instance of the Entity_registry class, and
  has methods that mostly delegate to this instance.
 */

class System_tablespaces
{
public:
  // Dummy class acting as meta data placeholder.
  class System_tablespace
  { };

  // Classification of system tablespaces.
  enum Types
  {
    DD,                 // For storing the DD tables.
    PREDEFINED_DDSE     // Needed by the DDSE.
  };

  // Map from system tablespace type to string description, e.g. for debugging.
  static const char *type_name(Types type)
  {
    switch (type)
    {
      case Types::DD:              return "DD";
      case Types::PREDEFINED_DDSE: return "PREDEFINED_DDSE";
      default:                     return "";
    }
  }

private:
  // The actual registry is referred and delegated to rather than
  // being inherited from.
  typedef Entity_registry<std::pair<const std::string, const std::string>,
          System_tablespace, Types,
          type_name, true> System_tablespace_registry_type;
  System_tablespace_registry_type m_registry;

public:
  // The ordered iterator type must be public.
  typedef System_tablespace_registry_type::Iterator Iterator;

  static System_tablespaces *instance()
  {
    static System_tablespaces s_instance;
    return &s_instance;
  }

  // Add a new system tablespace by delegation to the wrapped registry.
  void add(const std::string &tablespace_name, Types type)
  {
    m_registry.add("", tablespace_name, type,
                   new (std::nothrow) System_tablespace());
  }

  // Find a system tablespace by delegation to the wrapped registry.
  const System_tablespace *find(const std::string &tablespace_name)
  { return m_registry.find("", tablespace_name); }

  Iterator begin()
  { return m_registry.begin(); }

  Iterator begin(Types type)
  { return m_registry.begin(type); }

  Iterator end()
  { return m_registry.end(); }

  Iterator next(Iterator current, Types type)
  { return m_registry.next(current, type); }

#ifndef DBUG_OFF
  void dump()
  { m_registry.dump(); }
#endif
};
}

#endif // DD__SYSTEM_REGISTRY_INCLUDED
