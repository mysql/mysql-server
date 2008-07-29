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

#include "MgmtSrvr.hpp"
#include <InitConfigFileParser.hpp>
#include <ConfigRetriever.hpp>
#include <NdbSleep.h>


Config*
MgmtSrvr::load_init_config(void)
{
   InitConfigFileParser parser;
   g_eventLogger->info("Reading cluster configuration from '%s'",
                      m_opts.config_filename);
  return parser.parseConfig(m_opts.config_filename);
}


Config*
MgmtSrvr::load_init_mycnf(void)
{
  InitConfigFileParser parser;
  g_eventLogger->info("Reading cluster configuration using my.cnf");
  return parser.parse_mycnf();
}


bool
MgmtSrvr::fetch_config(void)
{
  char buf[128];
  DBUG_ENTER("MgmtSrvr::fetch_config");
  assert(_config == NULL);

  /* Loop until config loaded from other mgmd(s) */
  g_eventLogger->info("Trying to get configuration from other mgmd(s)"\
                     "using '%.128s'...",
                     m_config_retriever.get_connectstring(buf, sizeof(buf)));


  int retry= 0, delay= 0, verbose= 1;
  while (m_config_retriever.do_connect(retry, delay, verbose) != 0) {
    g_eventLogger->info("Waiting for connection to other mgmd(s)...");
    NdbSleep_SecSleep(1);
  }
  g_eventLogger->info("Connected...");

  // "login" and alloc node id from the other mgmd
  _ownNodeId= m_config_retriever.allocNodeId(retry, delay);
  if (_ownNodeId == 0) {
    g_eventLogger->error(m_config_retriever.getErrorString());
    DBUG_RETURN(NULL);
  }

  // read config from other managent server
  struct ndb_mgm_configuration * tmp = m_config_retriever.getConfig();
  if (tmp == NULL) {
    g_eventLogger->error(m_config_retriever.getErrorString());
    DBUG_RETURN(NULL);
  }

  setConfig(new Config(tmp));

  DBUG_RETURN(true);
}



