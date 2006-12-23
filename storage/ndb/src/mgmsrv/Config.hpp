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

#include <LogLevel.hpp>

#include <kernel_types.h>

#include <NdbOut.hpp>
#include <ndb_limits.h>
#include <Properties.hpp>
#include <ConfigInfo.hpp>

class ConfigInfo;

/**
 * @class Config
 * @brief Cluster Configuration  (corresponds to initial configuration file)
 *
 * Contains all cluster configuration parameters.
 *
 * The information includes all configurable parameters for a NDB cluster:
 * - DB, API and MGM nodes with all their properties, 
 * - Connections between nodes and computers the nodes will execute on.
 *
 * The following categories (sections) of configuration parameters exists:
 * - COMPUTER, DB, MGM, API, TCP, SCI, SHM
 *
 */

class Config {
public:
  /**
   *   Constructor which loads the object with an Properties object
   */
  Config();
  virtual ~Config();

  /**
   *   Prints the configuration in configfile format
   */
  void printConfigFile(NdbOut &out = ndbout) const;
  void printConfigFile(OutputStream &out) const {
    NdbOut ndb(out);
    printConfigFile(ndb);
  }

  /**
   * Info
   */
  const ConfigInfo * getConfigInfo() const { return &m_info;}
private:
  ConfigInfo m_info;

  void printAllNameValuePairs(NdbOut &out,
			      const Properties *prop,
			      const char* section) const;

  /**
   *   Information about parameters (min, max values etc)
   */
public:
  Properties * m_oldConfig;
  struct ndb_mgm_configuration * m_configValues;
};

#endif // Config_H
