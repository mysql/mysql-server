/* Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>

#include <NdbEnv.h>
#include <NdbConfig.h>
#include <NdbSleep.h>
#include <portlib/NdbDir.hpp>
#include <NdbAutoPtr.hpp>
#include <portlib/NdbNuma.h>

#include "vm/SimBlockList.hpp"
#include "vm/WatchDog.hpp"
#include "vm/ThreadConfig.hpp"
#include "vm/Configuration.hpp"

#include "ndbd.hpp"

#include <TransporterRegistry.hpp>

#include <ConfigRetriever.hpp>
#include <LogLevel.hpp>

#if defined NDB_SOLARIS
#include <sys/processor.h>
#endif

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

static void
systemInfo(const Configuration & config, const LogLevel & logLevel)
{
#ifdef NDB_WIN32
  int processors = 0;
  int speed;
  SYSTEM_INFO sinfo;
  GetSystemInfo(&sinfo);
  processors = sinfo.dwNumberOfProcessors;
  HKEY hKey;
  if(ERROR_SUCCESS==RegOpenKeyEx
     (HKEY_LOCAL_MACHINE,
      TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"),
      0, KEY_READ, &hKey)) {
    DWORD dwMHz;
    DWORD cbData = sizeof(dwMHz);
    if(ERROR_SUCCESS==RegQueryValueEx(hKey,
				      "~MHz", 0, 0, (LPBYTE)&dwMHz, &cbData)) {
      speed = int(dwMHz);
    }
    RegCloseKey(hKey);
  }
#elif defined NDB_SOLARIS
  // Search for at max 16 processors among the first 256 processor ids
  processor_info_t pinfo; memset(&pinfo, 0, sizeof(pinfo));
  int pid = 0;
  while(processors < 16 && pid < 256){
    if(!processor_info(pid++, &pinfo))
      processors++;
  }
  speed = pinfo.pi_clock;
#endif

  if(logLevel.getLogLevel(LogLevel::llStartUp) > 0){
    g_eventLogger->info("NDB Cluster -- DB node %d", globalData.ownId);
    g_eventLogger->info("%s --", NDB_VERSION_STRING);
#ifdef NDB_SOLARIS
    g_eventLogger->info("NDB is running on a machine with %d processor(s) at %d MHz",
                        processor, speed);
#endif
  }
  if(logLevel.getLogLevel(LogLevel::llStartUp) > 3){
    Uint32 t = config.timeBetweenWatchDogCheck();
    g_eventLogger->info("WatchDog timer is set to %d ms", t);
  }
}


Uint32
compute_acc_32kpages(const ndb_mgm_configuration_iterator * p)
{
  Uint64 accmem = 0;
  ndb_mgm_get_int64_parameter(p, CFG_DB_INDEX_MEM, &accmem);
  if (accmem)
  {
    accmem /= GLOBAL_PAGE_SIZE;
    
    Uint32 lqhInstances = 1;
    if (globalData.isNdbMtLqh)
    {
      lqhInstances = globalData.ndbMtLqhWorkers;
    }
    
    accmem += lqhInstances * (32 / 4); // Added as safty in Configuration.cpp
  }
  return Uint32(accmem);
}

static int
init_global_memory_manager(EmulatorData &ed, Uint32 *watchCounter)
{
  const ndb_mgm_configuration_iterator * p =
    ed.theConfiguration->getOwnConfigIterator();
  if (p == 0)
  {
    abort();
  }

  Uint32 numa = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_NUMA, &numa);
  if (numa == 1)
  {
    int res = NdbNuma_setInterleaved();
    g_eventLogger->info("numa_set_interleave_mask(numa_all_nodes) : %s",
                        res == 0 ? "OK" : "no numa support");
  }

  Uint64 shared_mem = 8*1024*1024;
  ndb_mgm_get_int64_parameter(p, CFG_DB_SGA, &shared_mem);
  Uint32 shared_pages = Uint32(shared_mem /= GLOBAL_PAGE_SIZE);

  Uint32 tupmem = 0;
  if (ndb_mgm_get_int_parameter(p, CFG_TUP_PAGE, &tupmem))
  {
    g_eventLogger->alert("Failed to get CFG_TUP_PAGE parameter from "
                        "config, exiting.");
    return -1;
  }

  {
    /**
     * IndexMemory
     */
    Uint32 accpages = compute_acc_32kpages(p);
    tupmem += accpages; // Add to RG_DATAMEM
  }

  Uint32 lqhInstances = 1;
  if (globalData.isNdbMtLqh)
  {
    lqhInstances = globalData.ndbMtLqhWorkers;
  }

  if (tupmem)
  {
    Resource_limit rl;
    rl.m_min = tupmem;
    rl.m_max = tupmem;
    rl.m_resource_id = RG_DATAMEM;
    ed.m_mem_manager->set_resource_limit(rl);
  }

  Uint32 logParts = NDB_DEFAULT_LOG_PARTS;
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_REDOLOG_PARTS, &logParts);

  Uint32 maxopen = logParts * 4; // 4 redo parts, max 4 files per part
  Uint32 filebuffer = NDB_FILE_BUFFER_SIZE;
  Uint32 filepages = (filebuffer / GLOBAL_PAGE_SIZE) * maxopen;
  globalData.ndbLogParts = logParts;

  {
    /**
     * RedoBuffer
     */
    Uint32 redomem = 0;
    ndb_mgm_get_int_parameter(p, CFG_DB_REDO_BUFFER,
                              &redomem);

    if (redomem)
    {
      redomem /= GLOBAL_PAGE_SIZE;
      Uint32 tmp = redomem & 15;
      if (tmp != 0)
      {
        redomem += (16 - tmp);
      }

      filepages += lqhInstances * redomem; // Add to RG_FILE_BUFFERS
    }
  }

  if (filepages)
  {
    Resource_limit rl;
    rl.m_min = filepages;
    rl.m_max = filepages;
    rl.m_resource_id = RG_FILE_BUFFERS;
    ed.m_mem_manager->set_resource_limit(rl);
  }

  Uint32 jbpages = compute_jb_pages(&ed);
  if (jbpages)
  {
    Resource_limit rl;
    rl.m_min = jbpages;
    rl.m_max = jbpages;
    rl.m_resource_id = RG_JOBBUFFER;
    ed.m_mem_manager->set_resource_limit(rl);
  }

  Uint32 sbpages = 0;
  if (globalTransporterRegistry.get_using_default_send_buffer() == false)
  {
    Uint64 mem;
    {
      Uint32 tot_mem = 0;
      ndb_mgm_get_int_parameter(p, CFG_TOTAL_SEND_BUFFER_MEMORY, &tot_mem);
      if (tot_mem)
      {
        mem = (Uint64)tot_mem;
      }
      else
      {
        mem = globalTransporterRegistry.get_total_max_send_buffer();
      }
    }

    sbpages = Uint32((mem + GLOBAL_PAGE_SIZE - 1) / GLOBAL_PAGE_SIZE);

    /**
     * Add extra send buffer pages for NDB multithreaded case
     */
    {
      Uint64 extra_mem = 0;
      ndb_mgm_get_int64_parameter(p, CFG_EXTRA_SEND_BUFFER_MEMORY, &extra_mem);
      Uint32 extra_mem_pages = Uint32((extra_mem + GLOBAL_PAGE_SIZE - 1) /
                                      GLOBAL_PAGE_SIZE);
      sbpages += mt_get_extra_send_buffer_pages(sbpages, extra_mem_pages);
    }

    Resource_limit rl;
    rl.m_min = sbpages;
    /**
     * allow over allocation (from SharedGlobalMemory) of up to 25% of
     *   totally allocated SendBuffer
     */
    rl.m_max = sbpages + (sbpages * 25) / 100;
    rl.m_resource_id = RG_TRANSPORTER_BUFFERS;
    ed.m_mem_manager->set_resource_limit(rl);
  }

  Uint32 pgman_pages = 0;
  {
    /**
     * Disk page buffer memory
     */
    Uint64 page_buffer = 64*1024*1024;
    ndb_mgm_get_int64_parameter(p, CFG_DB_DISK_PAGE_BUFFER_MEMORY,&page_buffer);

    Uint32 pages = 0;
    pages += Uint32(page_buffer / GLOBAL_PAGE_SIZE); // in pages
    pages += LCP_RESTORE_BUFFER * lqhInstances;

    pgman_pages += pages;
    pgman_pages += 64;

    Resource_limit rl;
    rl.m_min = pgman_pages;
    rl.m_max = pgman_pages;
    rl.m_resource_id = RG_DISK_PAGE_BUFFER;  // Add to RG_DISK_PAGE_BUFFER
    ed.m_mem_manager->set_resource_limit(rl);
  }

  Uint32 stpages = 64;
  {
    Resource_limit rl;
    rl.m_min = stpages;
    rl.m_max = 0;
    rl.m_resource_id = RG_SCHEMA_TRANS_MEMORY;
    ed.m_mem_manager->set_resource_limit(rl);
  }

  Uint32 sum = shared_pages + tupmem + filepages + jbpages + sbpages +
    pgman_pages + stpages;

  if (sum)
  {
    Resource_limit rl;
    rl.m_min = 0;
    rl.m_max = sum;
    rl.m_resource_id = 0;
    ed.m_mem_manager->set_resource_limit(rl);
  }

  if (!ed.m_mem_manager->init(watchCounter))
  {
    struct ndb_mgm_param_info dm;
    struct ndb_mgm_param_info sga;
    size_t size;

    size = sizeof(ndb_mgm_param_info);
    ndb_mgm_get_db_parameter_info(CFG_DB_DATA_MEM, &dm, &size);
    size = sizeof(ndb_mgm_param_info);
    ndb_mgm_get_db_parameter_info(CFG_DB_SGA, &sga, &size);

    g_eventLogger->alert("Malloc (%lld bytes) for %s and %s failed, exiting",
                         Uint64(shared_mem + tupmem) * GLOBAL_PAGE_SIZE,
                         dm.m_name, sga.m_name);
    return -1;
  }

  Uint32 late_alloc = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_LATE_ALLOC,
                            &late_alloc);

  Uint32 memlock = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_MEMLOCK, &memlock);

  if (late_alloc)
  {
    /**
     * Only map these groups that are required for ndb to even "start"
     */
    Uint32 rg[] = { RG_JOBBUFFER, RG_FILE_BUFFERS, RG_TRANSPORTER_BUFFERS, 0 };
    ed.m_mem_manager->map(watchCounter, memlock, rg);
  }
  else
  {
    ed.m_mem_manager->map(watchCounter, memlock); // Map all
  }

  return 0;                     // Success
}


static int
get_multithreaded_config(EmulatorData& ed)
{
  // multithreaded is compiled in ndbd/ndbmtd for now
  if (!globalData.isNdbMt)
  {
    ndbout << "NDBMT: non-mt" << endl;
    return 0;
  }

  THRConfig & conf = ed.theConfiguration->m_thr_config;
  Uint32 threadcount = conf.getThreadCount();
  ndbout << "NDBMT: MaxNoOfExecutionThreads=" << threadcount << endl;

  if (!globalData.isNdbMtLqh)
    return 0;

  ndbout << "NDBMT: workers=" << globalData.ndbMtLqhWorkers
         << " threads=" << globalData.ndbMtLqhThreads
         << " tc=" << globalData.ndbMtTcThreads
         << " send=" << globalData.ndbMtSendThreads
         << " receive=" << globalData.ndbMtReceiveThreads
         << endl;

  return 0;
}


static void
ndbd_exit(int code)
{
  // Don't allow negative return code
  if (code < 0)
    code = 255;

// gcov will not produce results when using _exit
#ifdef HAVE_gcov
  exit(code);
#else
  _exit(code);
#endif
}


static FILE *angel_info_w = NULL;

static void
writeChildInfo(const char *token, int val)
{
  fprintf(angel_info_w, "%s=%d\n", token, val);
  fflush(angel_info_w);
}

static void
childReportSignal(int signum)
{
  writeChildInfo("signal", signum);
}

static void
childExit(int error_code, int exit_code, Uint32 currentStartPhase)
{
  writeChildInfo("error", error_code);
  writeChildInfo("sphase", currentStartPhase);
  fprintf(angel_info_w, "\n");
  fclose(angel_info_w);
  ndbd_exit(exit_code);
}

static void
childAbort(int error_code, int exit_code, Uint32 currentStartPhase)
{
  writeChildInfo("error", error_code);
  writeChildInfo("sphase", currentStartPhase);
  fprintf(angel_info_w, "\n");
  fclose(angel_info_w);

#ifdef _WIN32
  // Don't use 'abort' on Windows since it returns
  // exit code 3 which conflict with NRT_NoStart_InitialStart
  ndbd_exit(exit_code);
#else
  signal(SIGABRT, SIG_DFL);
  abort();
#endif
}

extern "C"
void
handler_shutdown(int signum){
  g_eventLogger->info("Received signal %d. Performing stop.", signum);
  childReportSignal(signum);
  globalData.theRestartFlag = perform_stop;
}

extern NdbMutex * theShutdownMutex;

extern "C"
void
handler_error(int signum){
  // only let one thread run shutdown
  static bool handling_error = false;
  static pthread_t thread_id; // Valid when handling_error is true

  if (handling_error &&
      pthread_equal(thread_id, pthread_self()))
  {
    // Shutdown thread received signal
#ifndef NDB_WIN32
	signal(signum, SIG_DFL);
    kill(getpid(), signum);
#endif
    while(true)
      NdbSleep_MilliSleep(10);
  }
  if(theShutdownMutex && NdbMutex_Trylock(theShutdownMutex) != 0)
    while(true)
      NdbSleep_MilliSleep(10);

  thread_id = pthread_self();
  handling_error = true;

  g_eventLogger->info("Received signal %d. Running error handler.", signum);
  childReportSignal(signum);
  // restart the system
  char errorData[64], *info= 0;
#ifdef HAVE_STRSIGNAL
  info= strsignal(signum);
#endif
  BaseString::snprintf(errorData, sizeof(errorData), "Signal %d received; %s", signum,
		       info ? info : "No text for signal available");
  ERROR_SET_SIGNAL(fatal, NDBD_EXIT_OS_SIGNAL_RECEIVED, errorData, __FILE__);
}


static void
catchsigs(bool foreground){
  static const int signals_shutdown[] = {
#ifdef SIGBREAK
    SIGBREAK,
#endif
#ifdef SIGHUP
    SIGHUP,
#endif
    SIGINT,
#if defined SIGPWR
    SIGPWR,
#elif defined SIGINFO
    SIGINFO,
#endif
    SIGQUIT,
    SIGTERM,
#ifdef SIGTSTP
    SIGTSTP,
#endif
#ifdef SIGTTIN
    SIGTTIN,
#endif
#ifdef SIGTTOU
    SIGTTOU
#endif
  };

  static const int signals_error[] = {
    SIGABRT,
#ifdef SIGALRM
    SIGALRM,
#endif
#ifdef SIGBUS
    SIGBUS,
#endif
#ifdef SIGCHLD
    SIGCHLD,
#endif
    SIGFPE,
    SIGILL,
#ifdef SIGIO
    SIGIO,
#endif
#ifdef SIGPOLL
    SIGPOLL,
#endif
    SIGSEGV
  };

  static const int signals_ignore[] = {
    SIGPIPE
  };

  size_t i;
  for(i = 0; i < sizeof(signals_shutdown)/sizeof(signals_shutdown[0]); i++)
    signal(signals_shutdown[i], handler_shutdown);
  for(i = 0; i < sizeof(signals_error)/sizeof(signals_error[0]); i++)
    signal(signals_error[i], handler_error);
  for(i = 0; i < sizeof(signals_ignore)/sizeof(signals_ignore[0]); i++)
    signal(signals_ignore[i], SIG_IGN);

#ifdef SIGTRAP
  if (!foreground)
    signal(SIGTRAP, handler_error);
#endif

}

#ifdef _WIN32
static HANDLE g_shutdown_event;

DWORD WINAPI shutdown_thread(LPVOID)
{
  // Wait forever until the shutdown event is signaled
  WaitForSingleObject(g_shutdown_event, INFINITE);

  g_eventLogger->info("Performing stop");
  globalData.theRestartFlag = perform_stop;
  return 0;
}
#endif


void
ndbd_run(bool foreground, int report_fd,
         const char* connect_str, int force_nodeid, const char* bind_address,
         bool no_start, bool initial, bool initialstart,
         unsigned allocated_nodeid, int connect_retries, int connect_delay)
{
#ifdef _WIN32
  {
    char shutdown_event_name[32];
    _snprintf(shutdown_event_name, sizeof(shutdown_event_name),
              "ndbd_shutdown_%d", GetCurrentProcessId());

    g_shutdown_event = CreateEvent(NULL, TRUE, FALSE, shutdown_event_name);
    if (g_shutdown_event == NULL)
    {
      g_eventLogger->error("Failed to create shutdown event, error: %d",
                           GetLastError());
     ndbd_exit(1);
    }

    HANDLE thread = CreateThread(NULL, 0, &shutdown_thread, NULL, 0, NULL);
    if (thread == NULL)
    {
      g_eventLogger->error("couldn't start shutdown thread, error: %d",
                           GetLastError());
      ndbd_exit(1);
    }
  }
#endif

  if (foreground)
    g_eventLogger->info("Ndb started in foreground");

  if (report_fd)
  {
    g_eventLogger->debug("Opening report stream on fd: %d", report_fd);
    // Open a stream for sending extra status to angel
    if (!(angel_info_w = fdopen(report_fd, "w")))
    {
      g_eventLogger->error("Failed to open stream for reporting "
                           "to angel, error: %d (%s)", errno, strerror(errno));
      ndbd_exit(-1);
    }
  }
  else
  {
    // No reporting requested, open /dev/null
    const char* dev_null = IF_WIN("nul", "/dev/null");
    if (!(angel_info_w = fopen(dev_null, "w")))
    {
      g_eventLogger->error("Failed to open stream for reporting to "
                           "'%s', error: %d (%s)", dev_null, errno,
                           strerror(errno));
      ndbd_exit(-1);
    }
  }

  globalEmulatorData.create();

  Configuration* theConfig = globalEmulatorData.theConfiguration;
  if(!theConfig->init(no_start, initial, initialstart))
  {
    g_eventLogger->error("Failed to init Configuration");
    ndbd_exit(-1);
  }

  theConfig->fetch_configuration(connect_str, force_nodeid, bind_address,
                                 allocated_nodeid, connect_retries,
                                 connect_delay);

  if (NdbDir::chdir(NdbConfig_get_path(NULL)) != 0)
  {
    g_eventLogger->warning("Cannot change directory to '%s', error: %d",
                           NdbConfig_get_path(NULL), errno);
    // Ignore error
  }

  theConfig->setupConfiguration();

  if (get_multithreaded_config(globalEmulatorData))
    ndbd_exit(-1);

  systemInfo(* theConfig, * theConfig->m_logLevel);

  NdbThread* pWatchdog = globalEmulatorData.theWatchDog->doStart();

  {
    /*
     * Memory allocation can take a long time for large memory.
     *
     * So we want the watchdog to monitor the process of initial allocation.
     */
    Uint32 watchCounter;
    watchCounter = 9;           //  Means "doing allocation"
    globalEmulatorData.theWatchDog->registerWatchedThread(&watchCounter, 0);
    if (init_global_memory_manager(globalEmulatorData, &watchCounter))
      ndbd_exit(1);
    globalEmulatorData.theWatchDog->unregisterWatchedThread(0);
  }

  globalEmulatorData.theThreadConfig->init();

#ifdef VM_TRACE
  // Create a signal logger before block constructors
  char *buf= NdbConfig_SignalLogFileName(globalData.ownId);
  NdbAutoPtr<char> tmp_aptr(buf);
  FILE * signalLog = fopen(buf, "a");
  globalSignalLoggers.setOwnNodeId(globalData.ownId);
  globalSignalLoggers.setOutputStream(signalLog);
#if 1 // to log startup
  { const char* p = NdbEnv_GetEnv("NDB_SIGNAL_LOG", (char*)0, 0);
    if (p != 0) {
      char buf[200];
      BaseString::snprintf(buf, sizeof(buf), "BLOCK=%s", p);
      for (char* q = buf; *q != 0; q++) *q = toupper(toascii(*q));
      globalSignalLoggers.log(SignalLoggerManager::LogInOut, buf);
      globalData.testOn = 1;
      assert(signalLog != 0);
      fprintf(signalLog, "START\n");
      fflush(signalLog);
    }
  }
#endif
#endif

  // Load blocks (both main and workers)
  globalEmulatorData.theSimBlockList->load(globalEmulatorData);

  // Set thread concurrency for Solaris' light weight processes
  int status;
  status = NdbThread_SetConcurrencyLevel(30);
  assert(status == 0);

  catchsigs(foreground);

  /**
   * Do startup
   */
  switch(globalData.theRestartFlag){
  case initial_state:
    globalEmulatorData.theThreadConfig->doStart(NodeState::SL_CMVMI);
    break;
  case perform_start:
    globalEmulatorData.theThreadConfig->doStart(NodeState::SL_CMVMI);
    globalEmulatorData.theThreadConfig->doStart(NodeState::SL_STARTING);
    break;
  default:
    assert("Illegal state globalData.theRestartFlag" == 0);
  }

  globalTransporterRegistry.startSending();
  globalTransporterRegistry.startReceiving();
  if (!globalTransporterRegistry.start_service(*globalEmulatorData.m_socket_server)){
    ndbout_c("globalTransporterRegistry.start_service() failed");
    ndbd_exit(-1);
  }

  // Re-use the mgm handle as a transporter
  if(!globalTransporterRegistry.connect_client(
		 theConfig->get_config_retriever()->get_mgmHandlePtr()))
      ERROR_SET(fatal, NDBD_EXIT_CONNECTION_SETUP_FAILED,
                "Failed to convert mgm connection to a transporter",
                __FILE__);

  NdbThread* pTrp = globalTransporterRegistry.start_clients();
  if (pTrp == 0)
  {
    ndbout_c("globalTransporterRegistry.start_clients() failed");
    ndbd_exit(-1);
  }

  NdbThread* pSockServ = globalEmulatorData.m_socket_server->startServer();

  globalEmulatorData.theConfiguration->addThread(pTrp, SocketClientThread);
  globalEmulatorData.theConfiguration->addThread(pWatchdog, WatchDogThread);
  globalEmulatorData.theConfiguration->addThread(pSockServ, SocketServerThread);

  //  theConfig->closeConfiguration();
  {
    NdbThread *pThis = NdbThread_CreateObject(0);
    Uint32 inx = globalEmulatorData.theConfiguration->addThread(pThis,
                                                                MainThread);
    globalEmulatorData.theThreadConfig->ipControlLoop(pThis, inx);
    globalEmulatorData.theConfiguration->removeThreadId(inx);
  }
  NdbShutdown(0, NST_Normal);

  ndbd_exit(0);
}


extern "C" my_bool opt_core;

// instantiated and updated in NdbcntrMain.cpp
extern Uint32 g_currentStartPhase;

int simulate_error_during_shutdown= 0;

void
NdbShutdown(int error_code,
            NdbShutdownType type,
	    NdbRestartType restartType)
{
  if(type == NST_ErrorInsert)
  {
    type = NST_Restart;
    restartType = (NdbRestartType)
      globalEmulatorData.theConfiguration->getRestartOnErrorInsert();
    if(restartType == NRT_Default)
    {
      type = NST_ErrorHandler;
      globalEmulatorData.theConfiguration->stopOnError(true);
    }
  }

  if((type == NST_ErrorHandlerSignal) || // Signal handler has already locked mutex
     (NdbMutex_Trylock(theShutdownMutex) == 0)){
    globalData.theRestartFlag = perform_stop;

    bool restart = false;

    if((type != NST_Normal &&
	globalEmulatorData.theConfiguration->stopOnError() == false) ||
       type == NST_Restart)
    {
      restart  = true;
    }

    const char * shutting = "shutting down";
    if(restart)
    {
      shutting = "restarting";
    }

    switch(type){
    case NST_Normal:
      g_eventLogger->info("Shutdown initiated");
      break;
    case NST_Watchdog:
      g_eventLogger->info("Watchdog %s system", shutting);
      break;
    case NST_ErrorHandler:
      g_eventLogger->info("Error handler %s system", shutting);
      break;
    case NST_ErrorHandlerSignal:
      g_eventLogger->info("Error handler signal %s system", shutting);
      break;
    case NST_Restart:
      g_eventLogger->info("Restarting system");
      break;
    default:
      g_eventLogger->info("Error handler %s system (unknown type: %u)",
                          shutting, (unsigned)type);
      type = NST_ErrorHandler;
      break;
    }

    const char * exitAbort = 0;
    if (opt_core)
      exitAbort = "aborting";
    else
      exitAbort = "exiting";

    if(type == NST_Watchdog)
    {
      /**
       * Very serious, don't attempt to free, just die!!
       */
      g_eventLogger->info("Watchdog shutdown completed - %s", exitAbort);
      if (opt_core)
      {
	childAbort(error_code, -1,g_currentStartPhase);
      }
      else
      {
	childExit(error_code, -1,g_currentStartPhase);
      }
    }

#ifndef NDB_WIN32
    if (simulate_error_during_shutdown)
    {
      kill(getpid(), simulate_error_during_shutdown);
      while(true)
	NdbSleep_MilliSleep(10);
    }
#endif

    globalEmulatorData.theWatchDog->doStop();

#ifdef VM_TRACE
    FILE * outputStream = globalSignalLoggers.setOutputStream(0);
    if(outputStream != 0)
      fclose(outputStream);
#endif

    /**
     * Don't touch transporter here (yet)
     *   cause with ndbmtd, there are locks and nasty stuff
     *   and we don't know which we are holding...
     */
#if NOT_YET

    /**
     * Stop all transporter connection attempts and accepts
     */
    globalEmulatorData.m_socket_server->stopServer();
    globalEmulatorData.m_socket_server->stopSessions();
    globalTransporterRegistry.stop_clients();

    /**
     * Stop transporter communication with other nodes
     */
    globalTransporterRegistry.stopSending();
    globalTransporterRegistry.stopReceiving();

    /**
     * Remove all transporters
     */
    globalTransporterRegistry.removeAll();
#endif

    if(type == NST_ErrorInsert && opt_core)
    {
      // Unload some structures to reduce size of core
      globalEmulatorData.theSimBlockList->unload();
      NdbMutex_Unlock(theShutdownMutex);
      globalEmulatorData.destroy();
    }

    if(type != NST_Normal && type != NST_Restart)
    {
      g_eventLogger->info("Error handler shutdown completed - %s", exitAbort);
      if (opt_core)
      {
	childAbort(error_code, -1,g_currentStartPhase);
      }
      else
      {
	childExit(error_code, -1,g_currentStartPhase);
      }
    }

    /**
     * This is a normal restart, depend on angel
     */
    if(type == NST_Restart){
      childExit(error_code, restartType,g_currentStartPhase);
    }

    g_eventLogger->info("Shutdown completed - exiting");
  }
  else
  {
    /**
     * Shutdown is already in progress
     */

    /**
     * If this is the watchdog, kill system the hard way
     */
    if (type== NST_Watchdog)
    {
      g_eventLogger->info("Watchdog is killing system the hard way");
#if defined VM_TRACE
      childAbort(error_code, -1,g_currentStartPhase);
#else
      childExit(error_code, -1, g_currentStartPhase);
#endif
    }

    while(true)
      NdbSleep_MilliSleep(10);
  }
}
