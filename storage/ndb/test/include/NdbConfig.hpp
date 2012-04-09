/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
  NdbConfig(int own_id, const char* addr = 0) 
    : NdbRestarter(addr), 
      ownNodeId(own_id) {};

  bool getProperty(unsigned nodeid, unsigned type, unsigned key, Uint32 * val);

  bool getHostName(unsigned int node_id, const char ** hostname);
  //protected:  
  int ownNodeId;
};

#endif
