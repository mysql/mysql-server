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

#include <signaldata/TestOrd.hpp>
#include <OutputStream.hpp>

#include "MgmtSrvr.hpp"
#include <InitConfigFileParser.hpp>
#include <ConfigRetriever.hpp>
#include <ndb_version.h>

Config *
MgmtSrvr::readConfig() {
  Config *conf;
  InitConfigFileParser parser;
  if (m_configFilename.length())
  {
    conf = parser.parseConfig(m_configFilename.c_str());
  }
  else 
  {
    ndbout_c("Reading cluster configuration using my.cnf");
    conf = parser.parse_mycnf();
  }
  return conf;
}

Config *
MgmtSrvr::fetchConfig() {
  struct ndb_mgm_configuration * tmp = m_config_retriever->getConfig();
  if(tmp != 0){
    Config * conf = new Config();
    conf->m_configValues = tmp;
    return conf;
  }
  return 0;
}
