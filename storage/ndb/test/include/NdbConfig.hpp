/* Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDBT_CONFIG_HPP
#define NDBT_CONFIG_HPP

#include <ndb_types.h>
#include <mgmapi.h>
#include <Vector.hpp>
#include <NdbRestarter.hpp>
#include <mgmapi_config_parameters.h>

class NdbConfig : public NdbRestarter {
public:
  NdbConfig(const char* address = 0)
    : NdbRestarter(address)
  {}

  bool getProperty(unsigned nodeid, unsigned type, unsigned key, Uint32 * val);
  bool getHostName(unsigned int node_id, const char ** hostname);
};

#endif
