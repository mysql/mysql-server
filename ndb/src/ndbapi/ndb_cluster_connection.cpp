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

#include <ndb_global.h>
#include <pthread.h>

#include <ndb_cluster_connection.hpp>
#include <TransporterFacade.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <ndb_limits.h>
#include <ConfigRetriever.hpp>
#include <ndb_version.h>

Ndb_cluster_connection::Ndb_cluster_connection(const char *connect_string)
{
  m_facade= TransporterFacade::theFacadeInstance= new TransporterFacade();
  if (connect_string)
    m_connect_string= strdup(connect_string);
  else
    m_connect_string= 0;
  m_config_retriever= 0;
}

int Ndb_cluster_connection::connect()
{
  DBUG_ENTER("Ndb_cluster_connection::connect");
  if (m_config_retriever != 0) {
    DBUG_RETURN(0);
  }

  m_config_retriever= new ConfigRetriever(NDB_VERSION, NODE_TYPE_API);
  m_config_retriever->setConnectString(m_connect_string);

  const char* error = 0;
  do {
    if(m_config_retriever->init() == -1)
      break;
    
    if(m_config_retriever->do_connect() == -1)
      break;

    Uint32 nodeId = m_config_retriever->allocNodeId();
    for(Uint32 i = 0; nodeId == 0 && i<5; i++){
      NdbSleep_SecSleep(3);
      nodeId = m_config_retriever->allocNodeId();
    }
    if(nodeId == 0)
      break;

    ndb_mgm_configuration * props = m_config_retriever->getConfig();
    if(props == 0)
      break;
    m_facade->start_instance(nodeId, props);
    free(props);

    m_facade->connected();

    DBUG_RETURN(0);
  } while(0);
  
  ndbout << "Configuration error: ";
  const char* erString = m_config_retriever->getErrorString();
  if (erString == 0) {
    erString = "No error specified!";
  }
  ndbout << erString << endl;
  DBUG_RETURN(-1);
}

Ndb_cluster_connection::~Ndb_cluster_connection()
{
  if (m_facade != 0) {
    delete m_facade;
    if (m_facade != TransporterFacade::theFacadeInstance)
      abort();
    TransporterFacade::theFacadeInstance= 0;
  }
  if (m_connect_string)
    free(m_connect_string);
  if (m_config_retriever)
    delete m_config_retriever;
}


