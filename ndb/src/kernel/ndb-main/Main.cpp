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
#include <NodeState.hpp>

#if defined NDB_SOLARIS
#include <sys/types.h>     // For system information
#include <sys/processor.h> // For system informatio
#endif

#if !defined NDB_SOFTOSE && !defined NDB_OSE
#include <signal.h>        // For process signals

extern "C" {
  void ndbSignal(int signo, void (*func) (int));
  void handler(int signo);                  // for process signal handling
};

void catchsigs();                         // for process signal handling

#endif

// Shows system information
void systemInfo(const Configuration & conf,
		const LogLevel & ll); 

const char programName[] = "NDB Kernel";

extern int global_ndb_check;
NDB_MAIN(ndb_kernel){

  global_ndb_check = 1;

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

#if defined (NDB_LINUX) || defined (NDB_SOLARIS)
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
#endif

  systemInfo(* theConfig,
	     theConfig->clusterConfigurationData().SizeAltData.logLevel);
  
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
  globalSignalLoggers.setOutputStream(signalLog);
#endif
  
#if !defined NDB_SOFTOSE && !defined NDB_OSE
  catchsigs();
#endif
   
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
  return 0;
}


void 
systemInfo(const Configuration & config, const LogLevel & logLevel){
  int processors = 0;
  int speed;
#ifdef NDB_WIN32
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
    ndbout << "-- NDB Cluster -- DB node " << globalData.ownId
	   << " -- " << NDB_VERSION_STRING << " -- " << endl;
#ifdef NDB_SOLARIS
    ndbout << "NDB is running "  
	   << " on a machine with " << processors
	   << " processor(s) at "   <<  speed <<" MHz" 
	   << endl;
#endif
  }
  if(logLevel.getLogLevel(LogLevel::llStartUp) > 3){
    Uint32 t = config.timeBetweenWatchDogCheck();
    ndbout << "WatchDog timer is set to " << t << " ms" << endl;
  }

}

#if !defined NDB_SOFTOSE && !defined NDB_OSE

#ifdef NDB_WIN32

void 
catchsigs()
{
  ndbSignal(SIGINT,  handler);    // 2
  ndbSignal(SIGILL,  handler);    // 4
  ndbSignal(SIGFPE, handler);     // 8
#ifndef VM_TRACE
  ndbSignal(SIGSEGV, handler);    // 11
#endif
  ndbSignal(SIGTERM, handler);    // 15
  ndbSignal(SIGBREAK, handler);   // 21
  ndbSignal(SIGABRT, handler);    // 22
}

#else

void 
catchsigs(){
  // Makes the main process catch process signals, eg installs a 
  // handler named "handler". "handler" will then be called is instead 
  // of the defualt process signal handler)
  ndbSignal(SIGHUP,  handler);    // 1
  ndbSignal(SIGINT,  handler);    // 2
  ndbSignal(SIGQUIT, handler);    // 3
  ndbSignal(SIGILL,  handler);    // 4
  ndbSignal(SIGTRAP, handler);    // 5
#ifdef NDB_LINUX
  ndbSignal(7, handler);
#elif NDB_SOLARIS
  ndbSignal(SIGEMT, handler);     // 7
#elif NDB_MACOSX
  ndbSignal(SIGEMT, handler);     // 7
#endif
  ndbSignal(SIGFPE, handler);     // 8
  // SIGKILL cannot be caught,    9
  ndbSignal(SIGBUS, handler);     // 10
  ndbSignal(SIGSEGV, handler);    // 11
  ndbSignal(SIGSYS, handler);     // 12 
  ndbSignal(SIGPIPE, handler);    // 13
  ndbSignal(SIGALRM, handler);    // 14
  ndbSignal(SIGTERM, handler);    // 15
  ndbSignal(SIGUSR1, handler);    // 16
  ndbSignal(SIGUSR2, handler);    // 17
#ifndef NDB_MACOSX
  ndbSignal(SIGPWR, handler);     // 19
  ndbSignal(SIGPOLL, handler);    // 22
#endif
  // SIGSTOP cannot be caught     23
  ndbSignal(SIGTSTP, handler);    // 24
  ndbSignal(SIGTTIN, handler);    // 26
  ndbSignal(SIGTTOU, handler);    // 27
  ndbSignal(SIGVTALRM, handler);  // 28
  ndbSignal(SIGPROF, handler);    // 29
  ndbSignal(SIGXCPU, handler);    // 30
  ndbSignal(SIGXFSZ, handler);    // 31
}
#endif

extern "C"
void ndbSignal(int signo, void (*func) (int)) {
#ifdef NDB_WIN32
  signal(signo, func);
#else
  struct sigaction act, oact;
  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if(signo == SIGALRM) {
#ifdef SA_INTERRUPT
    act.sa_flags |= SA_INTERRUPT;
#endif
  } else {
#ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;
#endif
  }
  sigaction(signo, &act, &oact);
#endif
}


#ifdef NDB_WIN32

extern "C"
void 
handler(int sig)
{
  switch(sig){
  case SIGINT:   /*  2 - Interrupt  */
  case SIGTERM:  /* 15 - Terminate  */
  case SIGBREAK:  /* 21 - Ctrl-Break sequence */
  case SIGABRT:   /* 22 - abnormal termination triggered by abort call */
    globalData.theRestartFlag = perform_stop;
    break;
  default:
    // restart the system
    char errorData[40];
    snprintf(errorData, 40, "Signal %d received", sig);
    ERROR_SET(fatal, 0, errorData, __FILE__);
    break;
  }
}

#else

extern "C"
void 
handler(int sig){
  switch(sig){
  case SIGHUP:   /*  1 - Hang up    */
  case SIGINT:   /*  2 - Interrupt  */
  case SIGQUIT:  /*  3 - Quit       */
  case SIGTERM:  /* 15 - Terminate  */
#ifndef NDB_MACOSX
  case SIGPWR:   /* 19 - Power fail */
  case SIGPOLL:  /* 22              */
#endif
  case SIGSTOP:  /* 23              */
  case SIGTSTP:  /* 24              */
  case SIGTTIN:  /* 26              */
  case SIGTTOU:  /* 27              */
    globalData.theRestartFlag = perform_stop;
    break;
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

#endif
#endif

	






