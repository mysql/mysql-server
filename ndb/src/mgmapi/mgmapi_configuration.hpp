/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef MGMAPI_CONFIGURATION_HPP
#define MGMAPI_CONFIGURATION_HPP

#include <ConfigValues.hpp>

struct ndb_mgm_configuration {
  ConfigValues m_config;
};

struct ndb_mgm_configuration_iterator {
  Uint32 m_sectionNo;
  Uint32 m_typeOfSection;
  ConfigValues::ConstIterator m_config;

  ndb_mgm_configuration_iterator(const ndb_mgm_configuration &, unsigned type);
  ~ndb_mgm_configuration_iterator();

  int first();
  int next();
  int valid() const;
  int find(int param, unsigned value);

  int get(int param, unsigned * value) const ;
  int get(int param, Uint64 * value) const ;
  int get(int param, const char ** value) const ;

  //
  void reset();
  int enter();
};

#endif
