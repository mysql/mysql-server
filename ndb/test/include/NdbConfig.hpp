/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDBT_CONFIG_HPP
#define NDBT_CONFIG_HPP

#include <mgmapi.h>
#include <Vector.hpp>
#include <NdbRestarter.hpp>
#include <Properties.hpp>

class NdbConfig : public NdbRestarter{
public:
  NdbConfig(int own_id, const char* addr = 0) 
    : NdbRestarter(addr), 
      ownNodeId(own_id) {};

  bool getProperty(unsigned int node_id, const char* type,
		   const char * name, Uint32 * value) const;
  bool getProperty(unsigned int node_id, const char* type,
		   const char * name, const char ** value) const;  

  bool getHostName(unsigned int node_id,
		   const char ** hostname) const;
protected:  
  bool getPropsForNode(unsigned int node_id,
		const char* type, 
		const Properties ** props) const;

  int ownNodeId;
};

#endif
