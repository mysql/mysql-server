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

#include <ndb_version.h>
#include "Configuration.hpp"
#include <TransporterRegistry.hpp>

#include "SimBlockList.hpp"
#include "ThreadConfig.hpp"
#include <SignalLoggerManager.hpp>
#include <NdbOut.hpp>
#include <NdbMain.h>
#include <NdbDaemon.h>
#include <NdbConfig.h>
#include <WatchDog.hpp>

#include <LogLevel.hpp>
#include <EventLogger.hpp>
#include <NodeState.hpp>

#if defined NDB_SOLARIS // ok
#include <sys/processor.h> // For system informatio
#endif

#if !defined NDB_SOFTOSE && !defined NDB_OSE
#include <signal.h>        // For process signals
#endif

extern EventLogger g_eventLogger;

void catchsigs(bool ignore); // for process signal handling
extern "C" void handler(int signo);  // for process signal handling

// Shows system information
void systemInfo(const Configuration & conf,
		const LogLevel & ll); 

const char programName[] = "NDB Kernel";

NDB_MAIN(ndb_kernel){

  // Print to stdout/console
  g_eventLogger.createConsoleHandler();
  g_eventLogger.setCategory("NDB");
  g_eventLogger.enable(Logger::LL_INFO, Logger::LL_ALERT); // Log INFO to ALERT

  globalEmulatorData.create();

  // Parse command line options
  Configuration* theConfig = globalEmulatorData.theConfiguration;
  if(!theConfig->init(argc, argv)){
    return 0;
  }
  
  { // Do configuration
    theConfig->setupConfiguration();
  }

  // Get NDB_HOME path
  char homePath[255];
  NdbConfig_HomePath(homePath, 255);

  if (theConfig->getDaemonMode()) {
    // Become a daemon
    char lockfile[255], logfile[255];
    snprintf(lockfile, 255, "%snode%d.pid", homePath, globalData.ownId);
    snprintf(logfile, 255, "%snode%d.out", homePath, globalData.ownId);
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
  }

  g_eventLogger.info("Angel pid: %d ndb pid: %d", getppid(), getpid());
  systemInfo(* theConfig, * theConfig->m_logLevel); 

    // Load blocks
  globalEmulatorData.theSimBlockList->load(* theConfig);
    
  // Set thread concurrency for Solaris' light weight processes
  int status;
  status = NdbThread_SetConcurrencyLevel(30);
  NDB_ASSERT(status == 0, "Can't set appropriate concurrency level.");
  
#ifdef VM_TRACE
  // Create a signal logger
  char buf[255];
  strcpy(buf, homePath);
  FILE * signalLog = fopen(strncat(buf,"Signal.log", 255), "a");
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
    NDB_ASSERT(0, "Illegal state globalData.theRestartFlag");
  }

  globalTransporterRegistry.startSending();
  globalTransporterRegistry.startReceiving();
  globalEmulatorData.theWatchDog->doStart();
  
  globalEmulatorData.theThreadConfig->ipControlLoop();
  
  NdbShutdown(NST_Normal);
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

void 
catchsigs(bool ignore){
#if ! defined NDB_SOFTOSE && !defined NDB_OSE 

#if defined SIGRTMIN
  #define MAX_SIG_CATCH SIGRTMIN
#elif defined NSIG
  #define MAX_SIG_CATCH NSIG
#else
  #error "neither SIGRTMIN or NSIG is defined on this platform, please report bug at bugs.mysql.com"
#endif

  // Makes the main process catch process signals, eg installs a 
  // handler named "handler". "handler" will then be called is instead 
  // of the defualt process signal handler)
  if(ignore){
    for(int i = 1; i < MAX_SIG_CATCH; i++){
      if(i != SIGCHLD)
	signal(i, SIG_IGN);
    }
  } else {
    for(int i = 1; i < MAX_SIG_CATCH; i++){
      signal(i, handler);
    }
  }
#endif
}

extern "C"
void 
handler(int sig){
  switch(sig){
  case SIGHUP:   /*  1 - Hang up    */
  case SIGINT:   /*  2 - Interrupt  */
  case SIGQUIT:  /*  3 - Quit       */
  case SIGTERM:  /* 15 - Terminate  */
#ifdef SIGPWR
  case SIGPWR:   /* 19 - Power fail */
#endif
#ifdef SIGPOLL
  case SIGPOLL:  /* 22              */
#endif
  case SIGSTOP:  /* 23              */
  case SIGTSTP:  /* 24              */
  case SIGTTIN:  /* 26              */
  case SIGTTOU:  /* 27              */
    globalData.theRestartFlag = perform_stop;
    break;
#ifdef SIGWINCH
  case SIGWINCH:
#endif
  case SIGPIPE:
    /**
     * Can happen in TCP Transporter
     *  
     *  Just ignore
     */
    break;
  default:
    // restart the system
    char errorData[40];
    snprintf(errorData, 40, "Signal %d received", sig);
    ERROR_SET(fatal, 0, errorData, __FILE__);
    break;
  }
}

	






