/*
   Copyright (c) 2004, 2024, Oracle and/or its affiliates.

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

#ifndef MGMAPI_CONFIGURATION_HPP
#define MGMAPI_CONFIGURATION_HPP

#include <ConfigValues.hpp>

struct ndb_mgm_configuration {
  ConfigValues m_config_values;
};

/**
  This is a struct for navigating in the set of configuration values. Each
  configuration value belongs to a section instance, and each section instance
  is an instance of a section type.
  This struct lets you iterate over the instances of a given section type, and
  then lookup configuration values within the current section instance.
 */
struct ndb_mgm_configuration_iterator {
  Uint32 m_sectionNo;
  Uint32 m_typeOfSection;
  ConfigValues::ConstIterator m_config;

  ndb_mgm_configuration_iterator(const ndb_mgm_configuration *, unsigned type);

  /**
    Go to the first section instance. Return 0 if successful, i.e. if there is
    at least one instance.
   */
  int first();

  /** Go to the next instance. Return 0 if there was a next instance. */
  int next();

  /**
     Return 0 if there is a valid current instance (i.e. if the last first() or
     next() call succeeded).
   */
  int valid() const;

  /**
    Search for a configuration value with type='Int' id='param' and
    value='value'. If no match is found in the current section instance,
    try the next until we have tried the last section. Return 0 if a match
    was found.
    Note: This method may change the current section (i.e. call next()).
   */
  int find(int param, unsigned value);

  /**
    Lookup config value within current section. Return 0 if and only if
    value was found.
   */
  int get(int param, unsigned *value) const;
  int get(int param, Uint64 *value) const;
  int get(int param, const char **value) const;

 private:
  void reset();
  int enter();
};
#endif
