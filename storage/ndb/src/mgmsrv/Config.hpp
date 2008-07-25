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

#ifndef Config_H
#define Config_H

#include "ConfigInfo.hpp"
#include <mgmapi_configuration.hpp>


/**
 * @class Config
 * @brief Cluster Configuration Wrapper
 *
 * Adds a C++ wrapper around 'ndb_mgm_configuration' which is
 * exposed from mgmapi_configuration
 *
 */

class Config {
public:
  Config(struct ndb_mgm_configuration *config_values = NULL);
  virtual ~Config();

  void print() const;

  struct ndb_mgm_configuration * m_configValues;
};

#endif // Config_H
