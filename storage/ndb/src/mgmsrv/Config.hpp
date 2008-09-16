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
  Config(ConfigValues* config_values);
  virtual ~Config();

  void print() const;

  /*
    Returns generation of the config
    0 => not set(yet), ie. config has never been committed
   */
  Uint32 getGeneration() const;
  bool setGeneration(Uint32);

  /*
   Pack the config into a UtilBuffer and return it's size in bytes
  */
  Uint32 pack(UtilBuffer&) const;

  /*
    Compare against another config and return a list of
    differences in a Properties object
  */
  void diff(const Config* other, Properties& diff_list,
            const unsigned* exclude=NULL) const;

  /*
    Print the difference against another config
   */
  void print_diff(const Config* other) const;

  /*
    Print the difference to string buffer
  */
  const char* diff2str(const Config* other, BaseString& str) const;

  /*
    Determine if changing to the other config is illegal
  */
  bool illegal_change(const Config* other) const;

  /*
    Check if the config is equal to another config
  */
  bool equal(const Config*, const unsigned* exclude = NULL) const;

  struct ndb_mgm_configuration * m_configValues;

private:
  bool setValue(Uint32 section, Uint32 section_no,
                Uint32 id, Uint32 new_gen);

  bool illegal_change(const Properties&) const;
  bool equal(const Properties&) const;
  const char* diff2str(const Properties&, BaseString& str) const;
};

class ConfigIter : public ndb_mgm_configuration_iterator {
public:
  ConfigIter(const Config* conf, unsigned type) :
    ndb_mgm_configuration_iterator(*conf->m_configValues, type) {};
};

#endif // Config_H
