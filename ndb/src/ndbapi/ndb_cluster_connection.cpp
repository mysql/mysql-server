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
#include <my_pthread.h>

#include <ndb_cluster_connection.hpp>
#include <TransporterFacade.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbThread.h>
#include <ndb_limits.h>
#include <ConfigRetriever.hpp>
#include <ndb_version.h>

static int g_run_connect_thread= 0;

Ndb_cluster_connection::Ndb_cluster_connection(const char *connect_string)
{
  m_facade= TransporterFacade::theFacadeInstance= new TransporterFacade();
  if (connect_string)
    m_connect_string= strdup(connect_string);
  else
    m_connect_string= 0;
  m_config_retriever= 0;
  m_connect_thread= 0;
  m_connect_callback= 0;
}

extern "C" pthread_handler_decl(run_ndb_cluster_connection_connect_thread, me)
{
  my_thread_init();
  g_run_connect_thread= 1;
  ((Ndb_cluster_connection*) me)->connect_thread();
  my_thread_end();
  NdbThread_Exit(0);
  return me;
}

void Ndb_cluster_connection::connect_thread()
{
  DBUG_ENTER("Ndb_cluster_connection::connect_thread");
  int r;
  do {
    if ((r = connect(1)) == 0)
      break;
    if (r == -1) {
      printf("Ndb_cluster_connection::connect_thread error\n");
      DBUG_ASSERT(false);
      g_run_connect_thread= 0;
    }
  } while (g_run_connect_thread);
  if (m_connect_callback)
    (*m_connect_callback)();
  DBUG_VOID_RETURN;
}

int Ndb_cluster_connection::start_connect_thread(int (*connect_callback)(void))
{
  int r;
  DBUG_ENTER("Ndb_cluster_connection::start_connect_thread");
  m_connect_callback= connect_callback;
  if ((r = connect(1)) == 1)
  {
    m_connect_thread= NdbThread_Create(run_ndb_cluster_connection_connect_thread,
				       (void**)this,
				       32768,
				       "ndb_cluster_connection",
				       NDB_THREAD_PRIO_LOW);
  }
  else if (r < 0)
  {
    DBUG_RETURN(-1);
  }
  else if (m_connect_callback)
  { 
    (*m_connect_callback)();
  }
  DBUG_RETURN(0);
}

int Ndb_cluster_connection::connect(int reconnect)
{
  DBUG_ENTER("Ndb_cluster_connection::connect");
  const char* error = 0;
  do {
    if (m_config_retriever == 0)
    {
      m_config_retriever= new ConfigRetriever(NDB_VERSION, NODE_TYPE_API);
      m_config_retriever->setConnectString(m_connect_string);
      if(m_config_retriever->init() == -1)
	break;
    }
    else
      if (reconnect == 0)
	DBUG_RETURN(0);
    if (reconnect)
    {
      int r= m_config_retriever->do_connect(1);
      if (r == 1)
	DBUG_RETURN(1); // mgmt server not up yet
      if (r == -1)
	break;
    }
    else
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
    ndb_mgm_destroy_configuration(props);
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
  TransporterFacade::stop_instance();
  if (m_connect_thread)
  {
    void *status;
    g_run_connect_thread= 0;
    NdbThread_WaitFor(m_connect_thread, &status);
    NdbThread_Destroy(&m_connect_thread);
    m_connect_thread= 0;
  }
  if (m_facade != 0)
  {
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


