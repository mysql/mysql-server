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

#include <ndb_version.h>
#include "Configuration.hpp"
#include <LocalConfig.hpp>
#include <TransporterRegistry.hpp>

#include "vm/SimBlockList.hpp"
#include "ThreadConfig.hpp"
#include <SignalLoggerManager.hpp>
#include <NdbOut.hpp>
#include <NdbMain.h>
#include <NdbDaemon.h>
#include <NdbSleep.h>
#include <NdbConfig.h>
#include <WatchDog.hpp>

#include <LogLevel.hpp>
#include <EventLogger.hpp>

#include <NdbAutoPtr.hpp>

#if defined NDB_SOLARIS // ok
#include <sys/processor.h> // For system informatio
#endif

extern EventLogger g_eventLogger;
extern NdbMutex * theShutdownMutex;

void catchsigs(bool ignore); // for process signal handling

extern "C" void handler_shutdown(int signum);  // for process signal handling
extern "C" void handler_error(int signum);  // for process signal handling

// Shows system information
void systemInfo(const Configuration & conf,
		const LogLevel & ll); 

const char programName[] = "NDB Kernel";

NDB_MAIN(ndb_kernel){

  ndb_init();
  // Print to stdout/console
  g_eventLogger.createConsoleHandler();
  g_eventLogger.setCategory("NDB");
  g_eventLogger.enable(Logger::LL_INFO, Logger::LL_ALERT); // Log INFO to ALERT

  globalEmulatorData.create();

  // Parse command line options
  Configuration* theConfig = globalEmulatorData.theConfiguration;
  if(!theConfig->init(argc, argv)){
    return NRT_Default;
  }
  
  LocalConfig local_config;
  if (!local_config.init(theConfig->getConnectString(),0)){
    local_config.printError();
    local_config.printUsage();
    return NRT_Default;
  }

  { // Do configuration
    signal(SIGPIPE, SIG_IGN);
    theConfig->fetch_configuration(local_config);
  }
  
  chdir(NdbConfig_get_path(0));

  if (theConfig->getDaemonMode()) {
    // Become a daemon
    char *lockfile= NdbConfig_PidFileName(globalData.ownId);
    char *logfile=  NdbConfig_StdoutFileName(globalData.ownId);
    NdbAutoPtr<char> tmp_aptr1(lockfile), tmp_aptr2(logfile);

    if (NdbDaemon_Make(lockfile, logfile, 0) == -1) {
      ndbout << "Cannot become daemon: " << NdbDaemon_ErrorText << endl;
      return 1;
    }
  }
  
  for(pid_t child = fork(); child != 0; child = fork()){
    /**
     * Parent
     */
    catchsigs(true);

    int status = 0;
    while(waitpid(child, &status, 0) != child);
    if(WIFEXITED(status)){
      switch(WEXITSTATUS(status)){
      case NRT_Default:
	g_eventLogger.info("Angel shutting down");
	exit(0);
	break;
      case NRT_NoStart_Restart:
	theConfig->setInitialStart(false);
	globalData.theRestartFlag = initial_state;
	break;
      case NRT_NoStart_InitialStart:
	theConfig->setInitialStart(true);
	globalData.theRestartFlag = initial_state;
	break;
      case NRT_DoStart_InitialStart:
	theConfig->setInitialStart(true);
	globalData.theRestartFlag = perform_start;
	break;
      default:
	if(theConfig->stopOnError()){
	  /**
	   * Error shutdown && stopOnError()
	   */
	  exit(0);
	}
	// Fall-through
      case NRT_DoStart_Restart:
	theConfig->setInitialStart(false);
	globalData.theRestartFlag = perform_start;
	break;
      }
    } else if(theConfig->stopOnError()){
      /**
       * Error shutdown && stopOnError()
       */
      exit(0);
    }
    g_eventLogger.info("Ndb has terminated (pid %d) restarting", child);
    theConfig->fetch_configuration(local_config);
  }

  g_eventLogger.info("Angel pid: %d ndb pid: %d", getppid(), getpid());
  theConfig->setupConfiguration();
  systemInfo(* theConfig, * theConfig->m_logLevel); 
  
    // Load blocks
  globalEmulatorData.theSimBlockList->load(* theConfig);
    
  // Set thread concurrency for Solaris' light weight processes
  int status;
  status = NdbThread_SetConcurrencyLevel(30);
  assert(status == 0);
  
#ifdef VM_TRACE
  // Create a signal logger
  char *buf= NdbConfig_SignalLogFileName(globalData.ownId);
  NdbAutoPtr<char> tmp_aptr(buf);
  FILE * signalLog = fopen(buf, "a");
  globalSignalLoggers.setOwnNodeId(globalData.ownId);
  globalSignalLoggers.setOutputStream(signalLog);
#endif
  
  catchsigs(false);
   
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

  SocketServer socket_server;

  globalTransporterRegistry.startSending();
  globalTransporterRegistry.startReceiving();
  if (!globalTransporterRegistry.start_service(socket_server)){
    ndbout_c("globalTransporterRegistry.start_service() failed");
    exit(-1);
  }

  if (!globalTransporterRegistry.start_clients()){
    ndbout_c("globalTransporterRegistry.start_clients() failed");
    exit(-1);
  }

  globalEmulatorData.theWatchDog->doStart();
  
  socket_server.startServer();

  //  theConfig->closeConfiguration();

  globalEmulatorData.theThreadConfig->ipControlLoop();
  
  NdbShutdown(NST_Normal);

  socket_server.stopServer();
  socket_server.stopSessions();

  globalTransporterRegistry.stop_clients();

  return NRT_Default;
}


void 
systemInfo(const Configuration & config, const LogLevel & logLevel){
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
#elif defined NDB_SOLARIS // ok
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
    g_eventLogger.info("NDB Cluster -- DB node %d", globalData.ownId);
    g_eventLogger.info("%s --", NDB_VERSION_STRING);
#ifdef NDB_SOLARIS // ok
    g_eventLogger.info("NDB is running on a machine with %d processor(s) at %d MHz",
		       processor, speed);
#endif
  }
  if(logLevel.getLogLevel(LogLevel::llStartUp) > 3){
    Uint32 t = config.timeBetweenWatchDogCheck();
    g_eventLogger.info("WatchDog timer is set to %d ms", t);
  }

}

#define handler_register(signum, handler, ignore)\
{\
  if (ignore) {\
    if(signum != SIGCHLD)\
      signal(signum, SIG_IGN);\
  } else\
    signal(signum, handler);\
}

void 
catchsigs(bool ignore){
#if ! defined NDB_SOFTOSE && !defined NDB_OSE 

  static const int signals_shutdown[] = {
#ifdef SIGBREAK
    SIGBREAK,
#endif
    SIGHUP,
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
    SIGTTIN,
    SIGTTOU
  };

  static const int signals_error[] = {
    SIGABRT,
    SIGALRM,
#ifdef SIGBUS
    SIGBUS,
#endif
    SIGCHLD,
    SIGFPE,
    SIGILL,
#ifdef SIGIO
    SIGIO,
#endif
#ifdef SIGPOLL
    SIGPOLL,
#endif
    SIGSEGV,
#ifdef SIGTRAP
    SIGTRAP
#endif
  };
#endif

  static const int signals_ignore[] = {
    SIGPIPE
  };

  size_t i;
  for(i = 0; i < sizeof(signals_shutdown)/sizeof(signals_shutdown[0]); i++)
    handler_register(signals_shutdown[i], handler_shutdown, ignore);
  for(i = 0; i < sizeof(signals_error)/sizeof(signals_error[0]); i++)
    handler_register(signals_error[i], handler_error, ignore);
  for(i = 0; i < sizeof(signals_ignore)/sizeof(signals_ignore[0]); i++)
    handler_register(signals_ignore[i], SIG_IGN, ignore);
}

extern "C"
void 
handler_shutdown(int signum){
  g_eventLogger.info("Received signal %d. Performing stop.", signum);
  globalData.theRestartFlag = perform_stop;
}

extern "C"
void 
handler_error(int signum){
  // only let one thread run shutdown
  static long thread_id= 0;

  if (thread_id != 0 && thread_id == my_thread_id())
  {
    // Shutdown thread received signal
    signal(signum, SIG_DFL);
    kill(getpid(), signum);
    while(true)
      NdbSleep_MilliSleep(10);
  }
  if(theShutdownMutex && NdbMutex_Trylock(theShutdownMutex) != 0)
    while(true)
      NdbSleep_MilliSleep(10);
  thread_id= my_thread_id();
  g_eventLogger.info("Received signal %d. Running error handler.", signum);
  // restart the system
  char errorData[40];
  snprintf(errorData, 40, "Signal %d received", signum);
  ERROR_SET_SIGNAL(fatal, 0, errorData, __FILE__);
}
