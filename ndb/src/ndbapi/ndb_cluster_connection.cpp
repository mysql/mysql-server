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
#include <my_sys.h>

#include <ndb_cluster_connection.hpp>
#include <TransporterFacade.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbThread.h>
#include <ndb_limits.h>
#include <ConfigRetriever.hpp>
#include <ndb_version.h>

static int g_run_connect_thread= 0;

#include <NdbMutex.h>
NdbMutex *ndb_global_event_buffer_mutex= NULL;
#ifdef VM_TRACE
NdbMutex *ndb_print_state_mutex= NULL;
#endif

Ndb_cluster_connection::Ndb_cluster_connection(const char *connect_string)
{
  DBUG_ENTER("Ndb_cluster_connection");
  DBUG_PRINT("enter",("Ndb_cluster_connection this=0x%x", this));
  m_facade= TransporterFacade::theFacadeInstance= new TransporterFacade();

  m_config_retriever= 0;
  m_connect_thread= 0;
  m_connect_callback= 0;

  if (ndb_global_event_buffer_mutex == NULL)
  {
    ndb_global_event_buffer_mutex= NdbMutex_Create();
  }
#ifdef VM_TRACE
  if (ndb_print_state_mutex == NULL)
  {
    ndb_print_state_mutex= NdbMutex_Create();
  }
#endif
  m_config_retriever=
    new ConfigRetriever(connect_string, NDB_VERSION, NODE_TYPE_API);
  if (m_config_retriever->hasError())
  {
    printf("Could not connect initialize handle to management server: %s",
	   m_config_retriever->getErrorString());
    delete m_config_retriever;
    m_config_retriever= 0;
  }
  DBUG_VOID_RETURN;
}

int Ndb_cluster_connection::get_connected_port() const
{
  if (m_config_retriever)
    return m_config_retriever->get_mgmd_port();
  return -1;
}

const char *Ndb_cluster_connection::get_connected_host() const
{
  if (m_config_retriever)
    return m_config_retriever->get_mgmd_host();
  return 0;
}

const char *Ndb_cluster_connection::get_connectstring(char *buf, int buf_sz) const
{
  if (m_config_retriever)
    return m_config_retriever->get_connectstring(buf,buf_sz);
  return 0;
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
    NdbSleep_SecSleep(1);
    if ((r = connect(0,0,0)) == 0)
      break;
    if (r == -1) {
      printf("Ndb_cluster_connection::connect_thread error\n");
      DBUG_ASSERT(false);
      g_run_connect_thread= 0;
    } else {
      // Wait before making a new connect attempt
      NdbSleep_SecSleep(1);
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
  if ((r = connect(0,0,0)) == 1)
  {
    DBUG_PRINT("info",("starting thread"));
    m_connect_thread= 
      NdbThread_Create(run_ndb_cluster_connection_connect_thread,
		       (void**)this, 32768, "ndb_cluster_connection",
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

int Ndb_cluster_connection::connect(int no_retries, int retry_delay_in_seconds, int verbose)
{
  DBUG_ENTER("Ndb_cluster_connection::connect");
  const char* error = 0;
  do {
    if (m_config_retriever == 0)
      DBUG_RETURN(-1);
    if (m_config_retriever->do_connect(no_retries,retry_delay_in_seconds,verbose))
      DBUG_RETURN(1); // mgmt server not up yet

    Uint32 nodeId = m_config_retriever->allocNodeId(4/*retries*/,3/*delay*/);
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
  DBUG_ENTER("~Ndb_cluster_connection");
  DBUG_PRINT("enter",("~Ndb_cluster_connection this=0x%x", this));
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
  if (m_config_retriever)
    delete m_config_retriever;
  DBUG_VOID_RETURN;
}


