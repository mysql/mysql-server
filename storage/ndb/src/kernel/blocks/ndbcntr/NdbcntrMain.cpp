/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define NDBCNTR_C
#include "Ndbcntr.hpp"

#include <ndb_limits.h>
#include <ndb_version.h>
#include <SimpleProperties.hpp>
#include <cstring>
#include <signaldata/AbortAll.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/CntrStart.hpp>
#include <signaldata/CreateHashMap.hpp>
#include <signaldata/CreateTable.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/NdbSttor.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/NodeRecoveryStatusRep.hpp>
#include <signaldata/NodeStateSignalData.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/SchemaTrans.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/StartPerm.hpp>
#include <signaldata/StopMe.hpp>
#include <signaldata/StopPerm.hpp>
#include <signaldata/SystemError.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/WaitGCP.hpp>
#include "config.h"  // WORDS_BIGENDIAN

#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/ReadConfig.hpp>

#include <signaldata/FailRep.hpp>

#include <signaldata/FsAppendReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/LocalSysfile.hpp>
#include <signaldata/UndoLogLevel.hpp>

#include <AttributeHeader.hpp>
#include <Configuration.hpp>
#include <DebuggerNames.hpp>
#include <signaldata/DihRestart.hpp>

#include <NdbTick.h>
#include <NdbOut.hpp>

#include <signaldata/CreateFilegroup.hpp>
#include <signaldata/CreateNodegroupImpl.hpp>
#include <signaldata/DropNodegroupImpl.hpp>
#include <signaldata/TakeOver.hpp>

#include "../backup/BackupFormat.hpp"

#include <NdbGetRUsage.h>
#include <EventLogger.hpp>
#include "util/ndb_ndbxfrm1.h"  // ndb_ndbxfrm1::toggle_endian

#define JAM_FILE_ID 458

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
//#define DEBUG_NODE_STOP 1
//#define DEBUG_LOCAL_SYSFILE 1
//#define DEBUG_UNDO 1
//#define DEBUG_REDO_CONTROL 1
//#define DEBUG_NODE_GROUP_START 1
//#define DEBUG_LCP 1
#endif

#ifdef DEBUG_NODE_GROUP_START
#define DEB_NODE_GROUP_START(arglist) \
  do {                                \
    g_eventLogger->info arglist;      \
  } while (0)
#else
#define DEB_NODE_GROUP_START(arglist) \
  do {                                \
  } while (0)
#endif

#ifdef DEBUG_NODE_STOP
#define DEB_NODE_STOP(arglist)   \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_NODE_STOP(arglist) \
  do {                         \
  } while (0)
#endif

#ifdef DEBUG_REDO_CONTROL
#define DEB_REDO_CONTROL(arglist) \
  do {                            \
    g_eventLogger->info arglist;  \
  } while (0)
#else
#define DEB_REDO_CONTROL(arglist) \
  do {                            \
  } while (0)
#endif

#ifdef DEBUG_LOCAL_SYSFILE
#define DEB_LOCAL_SYSFILE(arglist) \
  do {                             \
    g_eventLogger->info arglist;   \
  } while (0)
#else
#define DEB_LOCAL_SYSFILE(arglist) \
  do {                             \
  } while (0)
#endif

#ifdef DEBUG_LCP
#define DEB_LCP(arglist)         \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_LCP(arglist) \
  do {                   \
  } while (0)
#endif

#ifdef DEBUG_UNDO
#define DEB_UNDO(arglist)        \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_UNDO(arglist) \
  do {                    \
  } while (0)
#endif

// used during shutdown for reporting current startphase
// accessed from Emulator.cpp, NdbShutdown()
Uint32 g_currentStartPhase = 0;

/**
 * ALL_BLOCKS Used during start phases and while changing node state
 *
 * NDBFS_REF Has to be before NDBCNTR_REF (due to "ndb -i" stuff)
 */
struct BlockInfo {
  BlockReference Ref;  // BlockReference
  Uint32 NextSP;       // Next start phase
  Uint32 ErrorInsertStart;
  Uint32 ErrorInsertStop;
  bool in_ndbd;
};

static BlockInfo ALL_BLOCKS[] = {
    {NDBFS_REF, 0, 2000, 2999, true},    {DBTC_REF, 1, 8000, 8035, true},
    {DBDIH_REF, 1, 7000, 7173, true},    {DBLQH_REF, 1, 5000, 5030, true},
    {DBACC_REF, 1, 3000, 3999, true},    {DBTUP_REF, 1, 4000, 4007, true},
    {DBDICT_REF, 1, 6000, 6003, true},   {NDBCNTR_REF, 0, 1000, 1999, true},
    {CMVMI_REF, 1, 9600, 9999, true},  // before QMGR
    {QMGR_REF, 1, 1, 999, true},         {TRIX_REF, 1, 0, 0, true},
    {BACKUP_REF, 1, 10000, 10999, true}, {DBUTIL_REF, 1, 11000, 11999, true},
    {SUMA_REF, 1, 13000, 13999, true},   {DBTUX_REF, 1, 12000, 12999, true},
    {TSMAN_REF, 1, 0, 0, true},          {LGMAN_REF, 1, 0, 0, true},
    {PGMAN_REF, 1, 0, 0, true},          {RESTORE_REF, 1, 0, 0, true},
    {DBINFO_REF, 1, 0, 0, true},         {DBSPJ_REF, 1, 0, 0, true},
    {THRMAN_REF, 1, 0, 0, true},         {DBQLQH_REF, 1, 0, 0, false},
    {DBQACC_REF, 1, 0, 0, false},        {DBQTUP_REF, 1, 0, 0, false},
    {QBACKUP_REF, 1, 0, 0, false},       {DBQTUX_REF, 1, 0, 0, false},
    {QRESTORE_REF, 1, 0, 0, false},      {TRPMAN_REF, 1, 0, 0, true}};

static const Uint32 ALL_BLOCKS_SZ = sizeof(ALL_BLOCKS) / sizeof(BlockInfo);

static BlockReference readConfigOrder[ALL_BLOCKS_SZ] = {
    CMVMI_REF,  TRPMAN_REF, NDBFS_REF,   DBINFO_REF, DBTUP_REF,   DBACC_REF,
    DBTC_REF,   DBLQH_REF,  DBTUX_REF,   DBDICT_REF, DBDIH_REF,   NDBCNTR_REF,
    QMGR_REF,   TRIX_REF,   BACKUP_REF,  DBUTIL_REF, SUMA_REF,    TSMAN_REF,
    LGMAN_REF,  PGMAN_REF,  RESTORE_REF, DBSPJ_REF,  THRMAN_REF,  DBQLQH_REF,
    DBQACC_REF, DBQTUP_REF, QBACKUP_REF, DBQTUX_REF, QRESTORE_REF};

/*******************************/
/*  CONTINUEB                  */
/*******************************/
void Ndbcntr::execCONTINUEB(Signal *signal) {
  jamEntry();
  UintR Ttemp1 = signal->theData[0];
  switch (Ttemp1) {
    case ZSTARTUP: {
      if (getNodeState().startLevel == NodeState::SL_STARTED) {
        jam();
        return;
      }

      if (cmasterNodeId == getOwnNodeId() && c_start.m_starting.isclear()) {
        jam();
        trySystemRestart(signal);
        // Fall-through
      }

      const Uint64 elapsed =
          NdbTick_Elapsed(c_start.m_startTime, NdbTick_getCurrentTicks())
              .milliSec();

      if (elapsed > c_start.m_startFailureTimeout) {
        jam();
        Uint32 to_3 = 0;
        const ndb_mgm_configuration_iterator *p =
            m_ctx.m_config.getOwnConfigIterator();
        ndb_mgm_get_int_parameter(p, CFG_DB_START_FAILURE_TIMEOUT, &to_3);
        BaseString tmp;
        tmp.append(
            "Shutting down node as total restart time exceeds "
            " StartFailureTimeout as set in config file ");
        if (to_3 == 0)
          tmp.append(" 0 (inifinite)");
        else
          tmp.appfmt(" %d", to_3);

        progError(__LINE__, NDBD_EXIT_RESTART_TIMEOUT, tmp.c_str());
      }

      signal->theData[0] = ZSTARTUP;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 1);
      break;
    }
    case ZSHUTDOWN:
      jam();
      c_stopRec.checkTimeout(signal);
      break;
    case ZBLOCK_STTOR:
      if (ERROR_INSERTED(1002)) {
        signal->theData[0] = ZBLOCK_STTOR;
        sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
        return;
      } else {
        c_missra.sendNextSTTOR(signal);
      }
      return;
    default:
      jam();
      systemErrorLab(signal, __LINE__);
      return;
      break;
  }  // switch
}  // Ndbcntr::execCONTINUEB()

void Ndbcntr::execAPI_START_REP(Signal *signal) {
  jamEntry();
  if (refToBlock(signal->getSendersBlockRef()) == QMGR) {
    for (Uint32 i = 0; i < ALL_BLOCKS_SZ; i++) {
      if (isNdbMtLqh() || ALL_BLOCKS[i].in_ndbd)
        sendSignal(ALL_BLOCKS[i].Ref, GSN_API_START_REP, signal, 1, JBB);
    }
  }
}
/*******************************/
/*  SYSTEM_ERROR               */
/*******************************/
void Ndbcntr::execSYSTEM_ERROR(Signal *signal) {
  const SystemError *const sysErr = (SystemError *)signal->getDataPtr();
  char buf[100];
  int killingNode = refToNode(sysErr->errorRef);
  Uint32 data1 = sysErr->data[0];

  jamEntry();
  switch (sysErr->errorCode) {
    case SystemError::GCPStopDetected: {
      BaseString::snprintf(buf, sizeof(buf),
                           "Node %d killed this node because "
                           "GCP stop was detected",
                           killingNode);
      signal->theData[0] = 7025;
      EXECUTE_DIRECT(DBDIH, GSN_DUMP_STATE_ORD, signal, 1);
      jamEntry();

      {
        signal->theData[0] = DumpStateOrd::LgmanDumpUndoStateLocalLog;
        EXECUTE_DIRECT_MT(LGMAN, GSN_DUMP_STATE_ORD, signal, 1, 0);
      }

      jamEntry();

      if (ERROR_INSERTED(1004)) {
        jam();
        g_eventLogger->info("NDBCNTR not shutting down due to GCP stop");
        return;
      }
      CRASH_INSERTION(1005);

      break;
    }
    case SystemError::CopyFragRefError:
      CRASH_INSERTION(1000);
      BaseString::snprintf(buf, sizeof(buf),
                           "Killed by node %d as "
                           "copyfrag failed, error: %u",
                           killingNode, data1);
      break;

    case SystemError::StartFragRefError:
      BaseString::snprintf(buf, sizeof(buf),
                           "Node %d killed this node because "
                           "it replied StartFragRef error code: %u.",
                           killingNode, data1);
      break;

    case SystemError::CopySubscriptionRef:
      CRASH_INSERTION(1003);
      BaseString::snprintf(
          buf, sizeof(buf),
          "Node %d killed this node because "
          "it could not copy a subscription during node restart. "
          "Copy subscription error code: %u.",
          killingNode, data1);
      break;
    case SystemError::CopySubscriberRef:
      BaseString::snprintf(
          buf, sizeof(buf),
          "Node %d killed this node because "
          "it could not start a subscriber during node restart. "
          "Copy subscription error code: %u.",
          killingNode, data1);
      break;
    default:
      BaseString::snprintf(buf, sizeof(buf),
                           "System error %d, "
                           " this node was killed by node %d",
                           sysErr->errorCode, killingNode);
      break;
  }

  progError(__LINE__, NDBD_EXIT_SYSTEM_ERROR, buf);
  return;
}  // Ndbcntr::execSYSTEM_ERROR()

struct ddentry {
  Uint32 type;
  const char *name;
  Uint64 size;
};

/**
 * f_dd[] = {
 * { DictTabInfo::LogfileGroup, "DEFAULT-LG", 32*1024*1024 },
 * { DictTabInfo::Undofile, "undofile.dat", 64*1024*1024 },
 * { DictTabInfo::Tablespace, "DEFAULT-TS", 1024*1024 },
 * { DictTabInfo::Datafile, "datafile.dat", 64*1024*1024 },
 * { ~0, 0, 0 }
 * };
 */
Vector<ddentry> f_dd;

static Uint64 parse_size(const char *src) {
  Uint64 num = 0;
  char *endptr = 0;
  num = my_strtoll(src, &endptr, 10);

  if (endptr) {
    switch (*endptr) {
      case 'k':
      case 'K':
        num *= 1024;
        break;
      case 'm':
      case 'M':
        num *= 1024;
        num *= 1024;
        break;
      case 'g':
      case 'G':
        num *= 1024;
        num *= 1024;
        num *= 1024;
        break;
    }
  }
  return num;
}

static int parse_spec(Vector<ddentry> &dst, const char *src, Uint32 type) {
  const char *key;
  Uint32 filetype;

  struct ddentry group;
  if (type == DictTabInfo::LogfileGroup) {
    key = "undo_buffer_size=";
    group.size = 64 * 1024 * 1024;
    group.name = "DEFAULT-LG";
    group.type = type;
    filetype = DictTabInfo::Undofile;
  } else {
    key = "extent_size=";
    group.size = 1024 * 1024;
    group.name = "DEFAULT-TS";
    group.type = type;
    filetype = DictTabInfo::Datafile;
  }
  size_t keylen = strlen(key);

  BaseString arg(src);
  Vector<BaseString> list;
  arg.split(list, ";");

  bool first = true;
  for (Uint32 i = 0; i < list.size(); i++) {
    list[i].trim();
    if (native_strncasecmp(list[i].c_str(), "name=", sizeof("name=") - 1) ==
        0) {
      group.name = strdup(list[i].c_str() + sizeof("name=") - 1);
    } else if (native_strncasecmp(list[i].c_str(), key, keylen) == 0) {
      group.size = parse_size(list[i].c_str() + keylen);
    } else if (strlen(list[i].c_str()) == 0 && (i + 1) == list.size()) {
      /**
       * ignore stray ";"
       */
    } else {
      /**
       * interpret as filespec
       */
      struct ddentry entry;
      const char *path = list[i].c_str();
      char *sizeptr = const_cast<char *>(strchr(path, ':'));
      if (sizeptr == 0) {
        return -1;
      }
      *sizeptr = 0;

      entry.name = strdup(path);
      entry.size = parse_size(sizeptr + 1);
      entry.type = filetype;

      if (first) {
        /**
         * push group as well
         */
        first = false;
        dst.push_back(group);
      }
      dst.push_back(entry);
    }
  }
  return 0;
}

/**
Restart Phases in MySQL Cluster
-------------------------------
In MySQL Cluster the restart is processed in phases, the restart of a node
is driven by a set of phases. In addition a node restart is also synchronised
with already started nodes and other nodes that are starting up in parallel
with our node. This comment will describe the various phases used.

The first step in starting a node is to create the data node run-time
environment. The data node process is normally running with an angel process,
this angel process ensures that the data node is automatically restarted in
cases of failures. So the only reason to run the data node again is after an
OS crash or after a shutdown by an operator or as part of a software upgrade.

When starting up the data node, the data node needs a node id, this is either
assigned through setting the parameter --ndb-nodeid when starting the data
node, or it is assigned by the management server when retrieving the
configuration. The angel process will ensure that the assigned node id will be
the same for all restarts of the data node.

After forking the data node process, the starting process stays as the angel
process and the new process becomes the actual data node process. The actual
data node process starts by retrieving the configuration from the management
server.

At this stage we have read the options, we have allocated a node id, we have
the configuration loaded from the management server. We will print some
important information to the data node log about our thread configuration and
some other things. To ensure that we find the correct files and create files
in the correct place we set the datadir of our data node process.

Next we have to start the watch-dog thread since we are now starting to do
activities where we want to ensure that we don't get stuck due to some
software error.

Next we will allocate the memory of the global memory pools, this is where
most memory is allocated, we still have a fair amount of memory allocated as
part of the initialisation of the various software modules in the NDB kernel,
but step by step we're moving towards usage of the global memory pools.

Allocating memory can be a fairly time-consuming process where the OS can
require up to one second for each GByte of memory allocated (naturally OS
dependent and will change over time). What actually consumes the time here is
actually that we also touch each page to ensure that the allocated memory is
also mapped to real physical memory to avoid page misses while we're running
the process. To speed up this process we have made the touching of memory
multi-threaded.

Actually where most memory is allocated is configurable, the configuration
variable LateAlloc can be used to delay the allocation of most memory to early
phases of the restart.

The only memory that is required to allocate in the early phase is the job
buffer, memory for sending messages over the network and finally memory for
messages to and from the file system threads. So allocation of e.g.
DataMemory, IndexMemory and DiskPageBufferMemory can be delayed until the
early start phases.

After allocating the global memory pool we initialise all data used by the
run-time environment. This ensures that we're ready to send and receive data
between the threads used in the data node process as soon as they are started.

At this point we've only started the watch-dog process and the thread started
as part of creating the process (this thread will later be converted to the
first receive thread if we're running ndbmtd and the only execution thread if
we are running ndbd). Next step is to load all software modules and initialise
those to ensure they're properly set-up when the messages start arriving for
execution.

Before we start the run-time environment we also need to activate the send
and receive services. This involves creating a socket client thread that
attempts to connect to socket server parts of other nodes in the cluster and
a thread to listen to the socket server used for those data nodes we
communicate as the socket server.

The default behaviour is that the node with the lowest nodeid is the socket
server in the communication setup. This can be changed in the data node
configuration.

Before we proceed and start the data node environment we will place the start
signals of the run-time environment in its proper job buffer. Actually to
start the system one needs to place two equal signals in the job buffer. The
first start signal starts the communication to other nodes and sets the state
to wait for the next signal to actually start the system. The second one will
start running the start phases.

Finally we start all the threads of the run-time environment. These can
currently include a main thread, a rep thread, a number of tc threads,
a number of send threads, a number of receive threads and a number of
ldm threads. Given that communication buffers for all threads have been
preallocated, we can start sending signals immediately as those threads
startup. The receiving thread will start to take care of its received signals
as soon as it has come to that point in its thread startup code.

There are two identical start signals, the first starts a recurring signal
that is sent on a regular basis to keep track of time in the data node.
Only the second one starts performing the various start phases.

A startup of a data node is handled in a set of phases. The first phase is
to send the signal READ_CONFIG_REQ to all software modules in the kernel,
then STTOR is similarly sent to all software modules in 256 phases numbered
from 0 to 255. These are numbered from 0 to 255, we don't use all of those
phases, but the code is flexible such that any of those phases could be
used now or sometime in the future.

In addition we have 6 modules that are involved in one more set of start
phases. The signal sent in these phases are called NDB_STTOR. The original
idea was to view this message as the local start of the NDB subsystem.
These signals are sent and handled by NDBCNTR and sent as part of the STTOR
handling in NDBCNTR. This means that it becomes a sequential part of the
startup phases.

Before starting the phases we ensure that any management node can connect
to our node and that all other node are disconnected and that they can only
send messages to the QMGR module. The management server receives reports
about various events in the data node and the QMGR module is taking care of
the inclusion of the data node into the cluster. Before we're included in
the cluster we cannot communicate with other nodes in any manner.

The start always starts in the main thread where each software module is
represented by at least a proxy module that all multithreaded modules contain.
The proxy module makes it possible to easy send and receive messages to a
set of modules of the same type using one message and one reply.

The READ_CONFIG_REQ signals are always sent in the same order. It starts by
sending to CMVMI, this is the block that receives the start order and it
performs a number of functions from where the software modules can affect the
run-time environment. It normally allocates most memory of the process and
touches all of this memory. It is part of the main thread.

The next module receiving READ_CONFIG_REQ is NDBFS, this is the module that
controls the file system threads, this module is found in the main thread.

Next module is DBINFO, this module supports the ndbinfo database used to get
information about the data node internals in table format, this module is
found in the main thread.

Next is DBTUP, this is the module where the actual data is stored. Next DBACC,
the module where primary key and unique key hash indexes are stored and where
we control row locks from. Both those blocks are contained in the ldm threads.

Next is DBTC, the module where transaction coordination is managed from,
this module is part of the tc thread. Next is DBLQH, the module that controls
the actions on data through key operations and scans and also handles the
REDO logs. This is the main module of the ldm thread.

Next is DBTUX that operates ordered index reusing pages used to store rows
in DBTUP, also part of the ldm thread. Next is DBDICT, the dictionary module
used to store and handle all metadata information about tables and columns,
tablespaces, log files and so forth. DICT is part of the main thread.

Next is DBDIH, the module to store and handle distribution information about
all tables, the table partitions and all replicas of each partition. It
controls the local checkpoint process, the global checkpoint process and
controls a major part of the restart processing. The DIH module is a part of
the main thread.

Next is NDBCNTR that controls the restart phases, it's part of the main
thread. Next is QMGR which takes care of the heartbeat protocol and inclusion
and exclusion of nodes in the cluster. It's part of the main thread.

Next is TRIX that performs a few services related to ordered indexes and other
trigger-based services. It's part of the tc thread. Next is BACKUP, this is
used for backups and local checkpoints and is part of the ldm thread.

Next is DBUTIL that provides a number of services such as performing key
operations on behalf of code in the modules. It's part of the main thread.
Next is the SUMA module that takes care of replication events, this is the
module handled by the rep thread.

Next is TSMAN, then LGMAN, and then PGMAN that are all part of the disk data
handling taking care of tablespace, UNDO logging and page management. They
are all part of the ldm thread.

RESTORE is a module used to restore local checkpoints as part of a startup.
This module is also part of the ldm thread.

Finally we have the DBSPJ module that takes care of join queries pushed down
to the data node, it executes as part of the tc thread.

The DBTUP, DBACC, DBLQH, DBTUX, BACKUP, TSMAN, LGMAN, PGMAN, RESTORE are all
tightly integrated modules that takes care of the data and indexes locally in
each node. This set of modules form an LDM instance, each node can have
multiple LDM instances and these can be spread over a set of threads.
Each LDM instance owns its own partition of the data.

We also have two modules that are not a part of restart handling, this is the
TRPMAN module that performs a number of transport-related functions
(communication with other nodes). It executes in the receive threads. Finally
we have the THRMAN that executes in every thread and does some thread
management functionality.

All modules receive READ_CONFIG_REQ, all modules also receive STTOR for
phase 0 and phase 1. In phase 1 they report back which startphases they want
to get informed about more.

During the READ_CONFIG_REQ the threads can execute for a very long time in
a module since we can be allocating and touching memory of large sizes. This
means that our watchdog thread have a special timeout for this phase to
ensure that we don't crash the process simply due to a long time of
initialising our memory. In normal operations each signal should execute only
for a small number of microseconds.

The start phases are synchronized by sending the message STTOR to all modules,
logically each module gets this signal for each start phase from 0 to 255.
However the response message STTORRY contains the list of start phases the
module really is interested in.

The NDBCNTR module that handles the start phase signals can optimise away
any signals not needed. The order in which modules receive the STTOR message
is the same for all phases:

1) NDBFS
2) DBTC
3) DBDIH
4) DBLQH
5) DBACC
6) DBTUP
7) DBDICT
8) NDBCNTR
9) CMVMI
10)QMGR
11)TRIX
12)BACKUP
13)DBUTIL
14)SUMA
15)DBTUX
16)TSMAN
17)LGMAN
18)PGMAN
19)RESTORE
20)DBINFO
21)DBSPJ

In addition there is a special start phase handling controlled by NDBCNTR,
so when NDBCNTR receives its own STTOR message it starts a local start phase
handling involving the modules, DBLQH, DBDICT, DBTUP, DBACC, DBTC and DBDIH.

This happens for phases 2 through 8. The messages sent in these start phases
are NDB_STTOR and NDB_STTORRY, they are handled in a similar manner to STTOR
and STTORRY. The modules receive also those start phases in the same order
for all phases and this order is:

1) DBLQH
2) DBDICT
3) DBTUP
4) DBACC
5) DBTC
6) DBDIH

For those modules that are multithreaded, the STTOR and NDB_STTOR messages
always are received by the Proxy module that executes in the main thread.
The Proxy module will then send the STTOR and NDB_STTOR messages to each
individual instance of the module (the number of instances is normally the
same as the number of threads, but could sometimes be different). It does
so in parallel, so all instances execute STTOR in parallel.

So effectively each instance of a module will logically first receive
READ_CONFIG_REQ, then a set of STTOR messages for each start phase and some
modules will also receive NDB_STTOR in a certain order. All these messages
are sent in a specific order and sequentially. So this means that we have the
ability to control when things are done by performing it in the correct start
phase.

Next we will describe step-by-step what happens in a node restart (or a node
start as part of a cluster start/restart). The startup is currently a
sequential process except where it is stated that it happens in parallel.
The below description thus describes the order things actually happens
currently.

READ_CONFIG_REQ
---------------
The READ_CONFIG_REQ does more or less the same for all software modules. It
allocates the memory required by the software module and initialises the
memory (creates various free lists and so forth). It also reads the various
configuration parameter which is of interest to the module (these often
affect the size of the memory we allocate).

It starts in CMVMI that allocates most of the global memory pool, next we
have NDBFS that creates the necessary file directories for disk data, it
also creates the bound IO threads that can be used by one file at a time
(initial number of threads configurable through InitalNoOpenFiles), then it
creates a number of free threads (number of them configurable through
IOThreadPool) used by disk data files (all files used to handle disk data),
each such thread can be used to open/read/write/close a disk data file.
Finally NDBFS also creates the communication channel from the file system
threads back to the other threads.

All other modules follow the same standard, they calculate a number of sizes
based on hard coded defines or through configuration variables, they allocate
memory for those variables, finally they initialise those allocated memory
structures.

STTOR Phase 0
-------------
First STTOR phase executed is STTOR phase 0. The only modules doing anything
in this phase is NDBCNTR that clears the file system if the start is an initial
start and CMVMI that creates the file system directory.

STTOR Phase 1
-------------
Next phase executed is STTOR phase 1, in this phase most modules initialise
some more data, references to neighbour modules are setup if necessary. In
addition DBDIH create some special mutexes that ensures that only one process
is involved in certain parts of the code at a time.

NDBCNTR initialises some data related to running NDB_STTOR starting in
phase 2. CMVMI locks memory if configured to do so, after this it installs the
normal watchdog timeout since now all large memory allocations are performed.
CMVMI also starts regular memory reporting.

QMGR is the most active module in this phase. It initialises some data, it
gets the restart type (initial start or normal start) from DBDIH, it opens
communication to all nodes in the cluster, it starts checking for node
failures of the include node handling. Finally it runs the protocol to
include the new node into the heartbeat protocol. This could take a while
since the node inclusion process can only bring in one node at a time and
the protocol contains some delays.

The BACKUP module then starts the disk speed check loop which will run as
long as the node is up and running.

STTOR Phase 2
-------------
Next step is to execute STTOR phase 2. The only module that does anything in
STTOR phase 2 is NDBCNTR, it asks DIH for the restart type, it reads the node
from the configuration, it initialises the partial timeout variables that
controls for how long to wait before we perform a partial start.

NDBCNTR sends the signal CNTR_START_REQ to the NDBCNTR in the current master
node, this signal enables the master node to delay the start of this node if
necessary due to other starting nodes or some other condition. For cluster
starts/restarts it also gives the master node the chance to ensure we wait
for enough nodes to start up before we start the nodes.

The master only accepts one node at a time that has received CNTR_START_CONF,
the next node can only receive CNTR_START_CONF after the previous starting
node have completed copying the metadata and releasing the metadata locks and
locks on DIH info, that happens below in STTOR phase 5.

So in a rolling restart it is quite common that the first node will get
CNTR_START_CONF and then instead get blocked on the DICT lock waiting for
an LCP to complete. The other nodes starting up in parallel will instead
wait on CNTR_START_CONF since only one node at a time can pass this.

After receiving CNTR_START_CONF, NDBCNTR continues by running NDB_STTOR
phase 1. Here DBLQH initialises the node records, it starts a reporting
service. It does also initialise the data about the REDO log, this also
includes initialising the REDO log on disk for all types of initial start
(can be quite time consuming).

DBDICT initialises the schema file (contains the tables that have been created
in the cluster and other metadata objects). DBTUP initialises a default value
fragment and DBTC and DBDIH initialises some data variables. After completing
the NDB_STTOR phase in NDBCNTR there is no more work done in STTOR phase 2.

STTOR Phase 3
-------------
Next step is to run the STTOR phase 3. Most modules that need the list of
nodes in the cluster reads this in this phase. DBDIH reads the nodes in this
phase, DBDICT sets the restart type. Next NDBCNTR receives this phase and
starts NDB_STTOR phase 2. This phase starts by setting up any node group
transporters specified in the configuration.  In this phase DBLQH sets up
connections from its operation records to the operation records in DBACC and
DBTUP. This is done in parallel for all DBLQH module instances.

DBDIH now prepares the node restart process by locking the meta data. This
means that we will wait until any ongoing meta data operation is completed
and when it is completed we will lock the meta data such that no meta data
changes can be done until we're done with the phase where we are copying the
metadata information.

The reason for locking is that all meta data and distribution info is fully
replicated. So we need to lock this information while we are copying the data
from the master node to the starting node. While we retain this lock we cannot
change meta data through meta data transactions. Before copying the meta data
later we also need to ensure no local checkpoint is running since this also
updates the distribution information.

After locking this we need to request permission to start the node from the
master node. The request for permission to start the node is handled by the
starting node sending START_PERMREQ to the master node. This could receive a
negative reply if another node is already processing a node restart, it could
fail if an initial start is required. If another node is already starting we
will wait 3 second and try again. This is executed in DBDIH as part of
NDB_STTOR phase 2.

After completing the NDB_STTOR phase 2 the STTOR phase 3 continues by the
CMVMI module activating the checks of send packed data which is used by scan
and key operations.

Next the BACKUP module reads the configured nodes. Next the SUMA module sets
the reference to the Page Pool such that it can reuse pages from this global
memory pool, next DBTUX sets the restart type. Finally PGMAN starts a stats
loop and a cleanup loop that will run as long as the node is up and running.

We could crash the node if our node is still involved in some processes
ongoing in the master node. This is fairly normal and will simply trigger a
crash followed by a normal new start up by the angel process. The request
for permission is handled by the master sending the information to all nodes.

For initial starts the request for permission can be quite time consuming
since we have to invalidate all local checkpoints from all tables in the
meta data on all nodes. There is no parallelisation of this invalidation
process currently, so it will invalidate one table at a time.

STTOR Phase 4
-------------
After completing STTOR phase 3 we move onto STTOR phase 4. This phase starts
by DBLQH acquiring a backup record in the BACKUP module that will be used
for local checkpoint processing.

Next NDBCNTR starts NDB_STTOR phase 3. This starts also in DBLQH where we
read the configured nodes. Then we start reading the REDO log to get it
set-up (we will set this up in the background, it will be synchronised by
another part of cluster restart/node restart later described), for all types
of initial starts we will wait until the initialisation of the REDO log have
been completed until reporting this phase as completed.

Next DBDICT will read the configured nodes whereafter also DBTC reads the
configured nodes and starts transaction counters reporting. Next in
NDB_STTOR phase 3 is that DBDIH initialises restart data for initial starts.

Before completing its work in STTOR phase 4, NDBCNTR will set-up a waiting
point such that all starting nodes have reached this point before
proceeding. This is only done for cluster starts/restarts, so not for node
restarts.

The master node controls this waitpoint and will send the signal
NDB_STARTREQ to DBDIH when all nodes of the cluster restart have reached
this point. More on this signal later.

The final thing happening in STTOR phase 4 is that DBSPJ reads the configured
nodes.

STTOR Phase 5
-------------
We now move onto STTOR phase 5. The first thing done here is to run NDB_STTOR
phase 4. Only DBDIH does some work here and it only does something in node
restarts. In this case it asks the current master node to start it up by
sending the START_MEREQ signal to it.

START_MEREQ works by copying distribution information from master DBDIH node
and then also meta data information from master DBDICT. It copies one table
of distribution information at a time which makes the process a bit slow
since it includes writing the table to disk in the starting node.

The only manner to trace this event is when writing the table distribution
information per table in DBDIH in the starting node. We can trace the
reception of DICTSTARTREQ that is received in the starting nodes DBDICT.

When DBDIH and DBDICT information is copied then we need to block the global
checkpoint in order to include the new node in all changes of meta data and
distribution information from now on. This is performed by sending
INCL_NODEREQ to all nodes. After this we can release the meta data lock that
was set by DBDIH already in STTOR phase 2.

After completing NDB_STTOR phase 4, NDBCNTR synchronises the start again in
the following manner:

If initial cluster start and master then create system tables
If cluster start/restart then wait for all nodes to reach this point.
After waiting for nodes in a cluster start/restart then run NDB_STTOR
phase 5 in master node (only sent to DBDIH).
If node restart then run NDB_STTOR phase 5 (only sent to DBDIH).

NDB_STTOR phase 5 in DBDIH is waiting for completion of a local checkpoint
if it is a master and we are running a cluster start/restart. For node
restarts we send the signal START_COPYREQ to the starting node to ask for
copying of data to our node.

  START OF DATABASE RECOVERY

We start with explaining a number of terms used.
------------------------------------------------
LCP: Local checkpoint, in NDB this means that all data in main memory is
written to disk and we also write changed disk pages to disk to ensure
that all changes before a certain point is available on disk.
Execute REDO log: This means that we're reading the REDO log one REDO log
record at a time and executing the action if needed that is found in the
REDO log record.
Apply the REDO log: Synonym of execute the REDO log.
Prepare REDO log record: This is a REDO log record that contains the
information about a change in the database (insert/delete/update/write).
COMMIT REDO log record: This is a REDO log record that specifies that a
Prepare REDO log record is to be actually executed. The COMMIT REDO log
record contains a back reference to the Prepare REDO log record.
ABORT REDO log record: Similarly to the COMMIT REDO log record but here
the transaction was aborted so there is no need to apply the REDO log
record.
Database: Means in this context all the data residing in the cluster or
in the node when there is a node restart.
Off-line Database: Means that our database in our node is not on-line
and thus cannot be used for reading. This is the state of the database
after restoring a LCP, but before applying the REDO log.
Off-line Consistent database: This is a database state which is not
up-to-date with the most recent changes, but it represents an old state
in the database that existed previously. This state is achieved after
restoring an LCP and executing the REDO log.
On-line Database: This is a database state which is up-to-date, any node
that can be used to read data is has its database on-line (actually
fragments are brought on-line one by one).
On-line Recoverable Database: This is an on-line database that is also
recoverable. In a node restart we reach the state on-line database first,
but we need to run an LCP before the database can also be recovered to
its current state. A recoverable database is also durable so this means
that we're adding the D in ACID to the database when we reach this state.
Node: There are API nodes, data nodes and management server nodes. A data
node is a ndbd/ndbmtd process that runs all the database logic and
contains the database data. The management server node is a process that
runs ndb_mgmd that contains the cluster configuration and also performs
a number of management services. API nodes are part of application processes
and within mysqld's. There can be more than one API node per application
process. Each API node is connected through a socket (or other
communication media) to each of the data nodes and management server nodes.
When one refers to nodes in this text it's mostly implied that we're
talking about a data node.
Node Group: A set of data nodes that all contain the same data. The number
of nodes in a node group is equal to the number of replicas we use in the
cluster.
Fragment: A part of a table that is fully stored on one node group.
Partition: Synonym of fragment.
Fragment replica: This is one fragment in one node. There can be up
to 4 replicas of a fragment (so thus a node group can have up to
4 nodes in it).
Distribution information: This is information about the partitions
(synonym of fragments) of the tables and on which nodes they reside
and information about LCPs that have been executed on each fragment
replica.
Metadata: This is the information about tables, indexes, triggers,
foreign keys, hash maps, files, log file groups, table spaces.
Dictionary information: Synonym to metadata.
LDM: Stands for Local Data Manager, these are the blocks that execute
the code that handles the data handled within one data node. It contains
blocks that handles the tuple storage, the hash index, the T-tree index,
the page buffer manager, the tablespace manager, a block that writes
LCPs and a block that restores LCPs, a log manager for disk data.

------------------------------------------------------------------------------
| What happens as part START_COPYREQ is what is the real database restore    |
| process. Here most of the important database recovery algorithms are       |
| executed to bring the database online again. The earlier phases were still |
| needed to restore the metadata and setup communication, setup memory and   |
| bringing in the starting node as a full citizen in the cluster of data     |
| nodes.                                                                     |
------------------------------------------------------------------------------

START_COPYREQ goes through all distribution information and sends
START_FRAGREQ to the owning DBLQH module instance for each fragment replica
to be restored on the node. DBLQH will start immediately to restore those
fragment replicas, it will queue the fragment replicas and restore one at a
time. This happens in two phases, first all fragment replicas that requires
restore of a local checkpoint starts to do that.

After all fragment replicas to restore have been sent and we have restored all
fragments from a local checkpoint stored on our disk (or sometime by getting
the entire fragment from an alive node) then it is time to run the disk data
UNDO log. Finally after running this UNDO log we're ready to get the fragment
replicas restored to latest disk-durable state by applying the REDO log.

DBDIH will send all required information for all fragment replicas to DBLQH
whereafter it sends START_RECREQ to DBLQH to indicate all fragment replica
information have been sent now.

START_RECREQ is sent through the DBLQH proxy module and this part is
parallelised such that all LDM instances are performing the below parts in
parallel.

If we're doing a initial node restart we don't need to restore any local
checkpoints since initial node restart means that we start without a file
system. So this means that we have to restore all data from other nodes in
the node group. In this case we start applying the copying of fragment
replicas immediately in DBLQH when we receive START_FRAGREQ. In this case
we don't need to run any Undo or Redo log since there is no local checkpoint
to restore the fragment.

When this is completed and DBDIH has reported that all fragment replicas to
start have been sent by sending START_RECREQ to DBLQH we will send
START_RECREQ to TSMAN whereafter we are done with the restore of the data.

We will specify all fragment replicas to restore as part of REDO log
execution. This is done through the signal EXEC_FRAGREQ. When all such signals
have been sent we send EXEC_SRREQ to indicate we have prepared for the next
phase of REDO log execution in DBLQH.

When all such signals are sent we have completed what is termed as phase 2
of DBLQH, the phase 1 in DBLQH is what started in NDB_STTOR phase 3 to prepare
the REDO log for reading it. So when both those phases are complete we're ready
to start what is termed phase 3 in DBLQH.

These DBLQH phases are not related to the start phases, these are internal
stages of startup in the DBLQH module.

Phase 3 in DBLQH is the reading of the REDO log and applying it on fragment
replicas restored from the local checkpoint. This is required to create a
database state which is synchronised on a specific global checkpoint. So we
first install a local checkpoint for all fragments, next we apply the REDO
log to synchronise the fragment replica with a certain global checkpoint.

Before executing the REDO log we need to calculate the start GCI and the last
GCI to apply in the REDO log by checking the limits on all fragment replicas
we will restore to the desired global checkpoint.

DBDIH has stored information about each local checkpoint of a fragment
replica which global checkpoint ranges that are required to run from the REDO
log in order to bring it to the state of a certain global checkpoint. This
information was sent in the START_FRAGREQ signal. DBLQH will merge all of
those limits per fragment replica to a global range of global checkpoints to
run for this LDM instance. So each fragment replica has its own GCP id range
to execute and this means that the minimum of all those start ranges and
maximum of all the end ranges is the global range of GCP ids that we need
to execute in the REDO log to bring the cluster on-line again.

The next step is to calculate the start and stop megabyte in the REDO log for
each log part by using the start and stop global checkpoint id. All the
information required to calculate this is already in memory, so it's a pure
calculation.

When we execute the REDO log we actually only apply the COMMIT records in the
correct global checkpoint range. The COMMIT record and the actual change
records are in different places in the REDO log, so for each Megabyte of
REDO log we record how far back in the REDO log we have to go to find the
change records.

While running the REDO log we maintain a fairly large cache of the REDO log
to avoid that we have to do disk reads in those cases where the transaction
ran for a long time.

This means that long-running and large transactions can have a negative effect
on restart times.

After all log parts have completed this calculation we're now ready to start
executing the REDO log. After executing the REDO log to completion we also
write some stuff into the REDO log to indicate that any information beyond
what we used here won't be used at any later time.

We now need to wait for all other log parts to also complete execution of
their parts of the REDO log. The REDO log execution is designed such that we
can execute the REDO log in more than one phase, this is intended for cases
where we can rebuild a node from more than one live node. Currently this code
should never be used.

So the next step is to check for the new head and tail of the REDO log parts.
This is done through the same code that uses start and stop global
checkpoints to calculate this number. This phase of the code also prepares
the REDO log parts for writing new REDO log records by ensuring that the
proper REDO log files are open. It also involves some rather tricky code to
ensure that pages that have been made dirty are properly handled.

  COMPLETED RESTORING OFF-LINE CONSISTENT DATABASE
------------------------------------------------------------------------------
| After completing restoring fragment replicas to a consistent global        |
| checkpoint, we will now start rebuilding the ordered indexes based on the  |
| data restored. After rebuilding the ordered indexes we are ready to send   |
| START_RECCONF to the starting DBDIH. START_RECCONF is sent through the     |
| DBLQH proxy, so it won't be passed onto DBDIH until all DBLQH instances    |
| have completed this phase and responded with START_RECCONF.                |
------------------------------------------------------------------------------

At this point in the DBLQH instances we have restored a consistent but old
variant of all data in the node. There are still no ordered indexes and there
is still much work remaining to get the node synchronised with the other nodes
again. For cluster restarts it might be that the node is fully ready to go,
it's however likely that some nodes still requires being synchronised with
nodes that have restored a more recent global checkpoint.

The DBDIH of the starting node will then start the take over process now
that the starting node has consistent fragment replicas. We will prepare the
starting node's DBLQH for the copying phase by sending PREPARE_COPY_FRAG_REQ
for each fragment replica we will copy over. This is a sequential process that
could be parallelised a bit.

The process to take over a fragment replica is quite involved. It starts by
sending PREPARE_COPY_FRAGREQ/CONF to the starting DBLQH, then we send
UPDATE_TOREQ/CONF to the master DBDIH to ensure we lock the fragment
information before the take over starts. After receiving confirmation of this
fragment lock, the starting node send UPDATE_FRAG_STATEREQ/CONF to all nodes to
include the new node into all operations on the fragment.

After completing this we again send UPDATE_TOREQ/CONF to the master node to
inform of the new status and unlock the lock on the fragment information. Then
we're ready to perform the actual copying of the fragment. This is done by
sending COPY_FRAGREQ/CONF to the node that will copy the data. When this
copying is done we send COPY_ACTIVEREQ/CONF to the starting node to activate
the fragment replica.

Next we again send UPDATE_TOREQ/CONF to the master informing about that we're
about to install the commit the take over of the new fragment replica. Next we
commit the new fragment replica by sending UPDATE_FRAG_STATEREQ/CONF to all
nodes informing them about completion of the copying of the fragment replica.
Finally we send another update to the master node with UPDATE_TOREQ/CONF.
Now we're finally complete with copying of this fragment.

The idea with this scheme is that the first UPDATE_FRAG_STATEREQ ensures that
we're a part of all transactions on the fragment. After doing the COPY_FRAGREQ
that synchronises the starting node's fragment replica with the alive node's
fragment replica on a row by row basis, we're sure that the two fragment
replicas are entirely synchronised and we can do a new UPDATE_FRAG_STATEREQ to
ensure all nodes know that we're done with the synchronisation.

  COMPLETED RESTORING ON-LINE NOT RECOVERABLE DATABASE
------------------------------------------------------------------------------
| At this point we have restored an online variant of the database by        |
| bringing one fragment at a time online. The database is still not          |
| recoverable since we haven't enabled logging yet and there is no local     |
| checkpoint of the data in the starting node.                               |
------------------------------------------------------------------------------

Next step is to enable logging on all fragments, after completing this step
we will send END_TOREQ to the master DBDIH. At this point we will wait until a
local checkpoint is completed where this node have been involved. Finally when
the local checkpoint have been completed we will send END_TOCONF to the
starting node and then we will send START_COPYCONF and that will complete
this phase of the restart.

  COMPLETED RESTORING ON-LINE RECOVERABLE DATABASE
------------------------------------------------------------------------------
| At this point we have managed to restored all data and we have brought it  |
| online and now we have also executed a local checkpoint after enabling     |
| logging and so now data in the starting node is also recoverable. So this  |
| means that the database is now fully online again.                         |
------------------------------------------------------------------------------

After completing NDB_STTOR phase 5 then all nodes that have been synchronised
in a waitpoint here are started again and NDBCNTR continues by running
phase 6 of NDB_STTOR.

In this phase DBLQH, DBDICT and DBTC sets some status variables indicating
that now the start has completed (it's not fully completed yet, but all
services required for those modules to operate are completed. DBDIH also
starts global checkpoint protocol for cluster start/restarts where it has
become the master node.

Yet one more waiting point for all nodes is now done in the case of a cluster
start/restart.

The final step in STTOR phase 5 is SUMA that reads the configured nodes,
gets the node group members and if there is node restart it asks another
node to recreate subscriptions for it.

STTOR Phase 6
-------------
We now move onto STTOR phase 6. In this phase NDBCNTR gets the node group of
the node, DBUTIL gets the systable id, prepares a set of operations for later
use and connects to TC to enable it to run key operations on behalf of other
modules later on.

STTOR Phase 7
-------------
Next we move onto STTOR phase 7. DBDICT now starts the index statistics loop
that will run as long as the node lives.

QMGR will start arbitration handling to handle a case where we are at risk of
network partitioning.

BACKUP will update the disk checkpoint speed (there is one config variable
for speed during restarts and one for normal operation, here we install the
normal operation speed). If initial start BACKUP will also create a backup
sequence through DBUTIL.

SUMA will create a sequence if it's running in a master node and it's an
initial start. SUMA will also always calculate which buckets it is
responsible to handle. Finally DBTUX will start monitoring of ordered indexes.

STTOR Phase 8
-------------
We then move onto STTOR phase 8. First thing here is to run phase 7 of
NDB_STTOR in which DBDICT enables foreign keys. Next NDBCNTR will also wait
for all nodes to come here if we're doing a cluster start/restart.

Next CMVMI will set state to STARTED and QMGR will enable communication to
all API nodes.

STTOR Phase 101
---------------
After this phase the only remaining phase is STTOR phase 101 in which SUMA
takes over responsibility of the buckets it is responsible for in the
asynchronous replication handling.

Major potential consumers of time so far:

All steps in the memory allocation (all steps of the READ_CONFIG_REQ).
CMVMI STTOR phase 1 that could lock memory. QMGR phase 1 that runs the
node inclusion protocol.

NDBCNTR STTOR phase 2 that waits for CNTR_START_REQ, DBLQH REDO log
initialisation for initial start types that happens in STTOR phase 2.
Given that only one node can be in this phase at a time, this can be
stalled by a local checkpoint wait of another node starting. So this
wait can be fairly long.

DBLQH sets up connections to DBACC and DBTUP, this is NDB_STTOR phase 2.
DBDIH in NDB_STTOR phase 2 also can wait for the meta data to be locked
and it can wait for response to START_PERMREQ.

For initial starts waiting for DBLQH to complete NDB_STTOR phase 3 where
it initialises set-up of the REDO logs. NDBCNTR for cluster start/restarts
in STTOR phase 4 after completing NDB_STTOR phase 3 have to wait for all
nodes to reach this point and then it has to wait for NDB_STARTREQ to
complete.

For node restarts we have delays in waiting for response to START_MEREQ
signal and START_COPYREQ, this is actually where most of the real work of
the restart is done. SUMA STTOR phase 5 where subscriptions are recreated
is another potential time consumer.

All waitpoints are obvious potential consumers of time. Those are mainly
located in NDBCNTR (waitpoint 5.2, 5,1 and 6).

Historical anecdotes:
1) The NDB kernel run-time environment was originally designed for an
AXE virtual machine. In AXE the starts were using the module MISSRA to
drive the STTOR/STTORRY signals for the various startup phases.
The MISSRA was later merged into NDBCNTR and is a submodule of NDBCNTR
nowadays. The name of STTOR and STTORRY has some basis in the AXE systems
way of naming signals in early days but has been forgotten now. At least
the ST had something to do with Start/Restart.

2) The reason for introducing the NDB_STTOR was since we envisioned a system
where the NDB kernel was just one subsystem within the run-time environment.
So therefore we introduced separate start phases for the NDB subsystem.
Over time the need for such a subsystem startup phases are no longer there,
but the software is already engineered for this and thus it's been kept in
this manner.

3) Also the responsibility for the distributed parts of the database start
is divided. QMGR is responsible for discovering when nodes are up and down.
NDBCNTR maintains the protocols for failure handling and other changes of the
node configuration. Finally DBDIH is responsible for the distributed start of
the database parts. It interacts a lot with DBLQH that have the local
responsibility of starting one nodes database part as directed by DBDIH.

Local checkpoint processing in MySQL Cluster
--------------------------------------------

This comment attempts to describe the processing of checkpoints as it happens
in MySQL Cluster. It also clarifies where potential bottlenecks are. This
comment is mainly intended as internal documentation of the open source code
of MySQL Cluster.

The reason for local checkpoints in MySQL Cluster is to ensure that we have
copy of data on disk which can be used to run the REDO log against to restore
the data in MySQL Cluster after a crash.

We start by introducing different restart variants in MySQL Cluster. The first
variant is a normal node restart, this means that the node have been missing
for a short time, but is now back on line again. We start by installing a
checkpointed version of all tables (including executing proper parts of the
REDO log against it). Next step is to use the replica which are still online
to make the checkpointed version up to date. Replicas are always organised in
node groups, the most common size of a node group is two nodes. So when a
node starts up, it uses the other node in the same node group to get an
online version of the tables back online. In a normal node restart we have
first restored a somewhat old version of all tables before using the other
node to synchronize it. This means that we only need to ship the latest
version of the rows that have been updated since the node failed before the
node restart. We also have the case of initial node restarts where all data
have to be restored from the other node since the checkpoint in the starting
node is either too old to be reused or it's not there at all when a completely
new node is started up.

The third variant of restart is a so called system restart, this means that
the entire cluster is starting up after a cluster crash or after a controlled
stop of the cluster. In this restart type we first restore a checkpoint on all
nodes before running the REDO log to get the system in a consistent and
up-to-date state. If any node was restored to an older global checkpoint than
the one to restart from, then it is necessary to use the same code used in
node restarts to bring those node to an online state.

The system restart will restore a so called global checkpoint. A set of
transactions are grouped together into a global checkpoint, when this global
checkpoint has been completed the transactions belonging to it are safe and
will survive a cluster crash. We run global checkpoints on a second level,
local checkpoints write the entire data set to disk and is a longer process
taking at least minutes.

Before a starting node can be declared as fully restored it has to participate
in a local checkpoint. The crashing node misses a set of REDO log record
needed to restore the cluster, thus the node isn't fully restored until it can
be used to restore all data it owns in a system restart.

So when performing a rolling node restart where all nodes in the cluster are
restarted (e.g. to upgrade the software in MySQL Cluster), it makes sense to
restart a set of nodes at a time since we can only have one set of nodes
restarted at a time.

This was a bit of prerequisite to understand the need for local checkpoints.
We now move to the description of how a local checkpoint is processed.

The local checkpoint is a distributed process. It is controlled by a
software module called DBDIH (or DIH for short, DIstribution Handler).
DIH contains all the information about where various replicas of each fragment
(synonym with partition) are placed and various data on these replicas.
DIH stores distribution information in one file per table. This file is
actually two files, this is to ensure that we can do careful writing of the
file. We first write file 0, when this is completed, we write file 1,
in this manner we can easily handle any crashes while writing the table
description.

When a local checkpoint have been completed, DIH immediately starts the
process to start the next checkpoint. At least one global checkpoint have
to be completed since starting the local checkpoint before we will start a
new local checkpoint.

The first step in the next local checkpoint is to check if we're ready to
run it yet. This is performed by sending the message TCGETOPSIZEREQ to all
TC's in the cluster. This will report back the amount of REDO log information
generated by checking the information received in TC for all write
transactions. The message will be sent by the master DIH. The role of the
master is assigned to the oldest surviving data node, this makes it easy to
select a new master whenever a data node currently acting as master dies.
All nodes agree on the order of nodes entering the cluster, so the age of
a node is consistent in all nodes in the cluster.

When all messages have returned the REDO log write size to the master
DIH we will compare it to the config variable TimeBetweenLocalCheckpoints
(this variable is set in logarithm of size, so e.g. 25 means we wait
2^25 words of REDO log has been created in the cluster which is 128 MByte
of REDO log info).

When sufficient amount of REDO log is generated, then we start the next local
checkpoint, the first step is to clear all TC counters, this is done by
sending TC_CLOPSIZEREQ to all TC's in the cluster.

The next step is to calculate the keep GCI (this is the oldest global
checkpoint id that needs to be retained in the REDO log). This number is very
important since it's the point where we can move the tail of the REDO log
forward. If we run out of REDO log space we will not be able to run any
writing transactions until we have started the next local checkpoint and
thereby moved the REDO log tail forward.

We calculate this number by checking each fragment what GCI it needs to be
restored. We currently keep two old local checkpoints still valid, so we
won't move the GCI back to invalidate the two oldest local checkpoints per
fragment. The GCI that will be restorable after completing this calculation
is the minimum GCI found on all fragments when looping over them.

Next we write this number and the new local checkpoint id and some other
information in the Sysfile of all nodes in the cluster. This Sysfile is the
first thing we look at when starting a restore of the cluster in a system
restart, so it's important to have this type of information correct in this
file.

When this is done we will calculate which nodes that will participate in the
local checkpoint (nodes currently performing the early parts of a restart is
not part of the local checkpoint and obviously also not dead nodes).

We send the information about the starting local checkpoint to all other DIH's
in the system. We must keep all other DIH's up-to-date all the time to ensure
it is easy to continue the local checkpoint also when the master DIH crashes
or is stopped in the middle of the local checkpoint process. Each DIH records
the set of nodes participating in the local checkpoint. They also set a flag
on each replica record indicating a local checkpoint is ongoing, on each
fragment record we also set the number of replicas that are part of this local
checkpoint.

Now we have completed the preparations for the local checkpoint, it is now
time to start doing the actual checkpoint writing of the actual data. The
master DIH controls this process by sending off a LCP_FRAG_ORD for each
fragment replica that should be checkpointed. DIH can currently have 2 such
LCP_FRAG_ORD outstanding per node and 2 fragment replicas queued. Each LDM
thread can process writing of one fragment replica at a time and it can
have one request for the next fragment replica queued. It's fairly
straightforward to extend this number such that more fragment replicas can
be written in parallel and more can be queued.

LCP_FRAG_REP is sent to all DIH's when the local checkpoint for a fragment
replica is completed. When a DIH discovers that all fragment replicas of a
table have completed the local checkpoint, then it's time to write the table
description to the file system. This will record the interesting local
checkpoint information for all of the fragment replicas. There are two things
that can cause this to wait. First writing and reading of the entire table
description is something that can only happen one at a time, this mainly
happens when there is some node failure handling ongoing while the local
checkpoint is being processed.

The second thing that can block the writing of a table description is that
currently a maximum of 4 table descriptions can be written in parallel. This
could easily become a bottleneck since each write a file can take in the order
of fifty milliseconds. So this means we can currently only write about 80 such
tables per second. In a system with many tables and little data this could
become a bottleneck. It should however not be a difficult bottleneck.

When the master DIH has sent all requests to checkpoint all fragment replicas
it will send a special LCP_FRAG_ORD to all nodes indicating that no more
fragment replicas will be sent out.
*/

void Ndbcntr::execREAD_CONFIG_REQ(Signal *signal) {
  jamEntry();

  const ReadConfigReq *req = (ReadConfigReq *)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Uint32 encrypted_filesystem = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_ENCRYPTED_FILE_SYSTEM,
                            &encrypted_filesystem);
  assert(encrypted_filesystem == 0 || encrypted_filesystem == 1);
  c_encrypted_filesystem = encrypted_filesystem;
  if (encrypted_filesystem == 1 && !ndb_openssl_evp::is_aeskw256_supported()) {
    progError(__LINE__, NDBD_EXIT_INVALID_CONFIG,
              "EncryptedFileSystem=1 requires OpenSSL 1.0.2 or newer");
  }

  Uint32 dl = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_DISCLESS, &dl);
  if (dl == 0) {
    const char *lgspec = 0;
    char buf[1024];
    if (!ndb_mgm_get_string_parameter(p, CFG_DB_DD_LOGFILEGROUP_SPEC,
                                      &lgspec)) {
      jam();

      if (parse_spec(f_dd, lgspec, DictTabInfo::LogfileGroup)) {
        BaseString::snprintf(buf, sizeof(buf),
                             "Unable to parse InitialLogfileGroup: %s", lgspec);
        progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
      }
    }

    const char *tsspec = 0;
    if (!ndb_mgm_get_string_parameter(p, CFG_DB_DD_TABLEPACE_SPEC, &tsspec)) {
      if (f_dd.size() == 0) {
        warningEvent(
            "InitialTablespace specified, "
            "but InitialLogfileGroup is not!");
        warningEvent("Ignoring InitialTablespace: %s", tsspec);
      } else {
        if (parse_spec(f_dd, tsspec, DictTabInfo::Tablespace)) {
          BaseString::snprintf(buf, sizeof(buf),
                               "Unable to parse InitialTablespace: %s", tsspec);
          progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
        }
      }
    }
  }

  struct ddentry empty;
  empty.type = ~0;
  f_dd.push_back(empty);

  if (true) {
    // TODO: add config parameter
    // remove ATTRIBUTE_MASK2
    g_sysTable_NDBEVENTS_0.columnCount--;
  }

  ReadConfigConf *conf = (ReadConfigConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, ReadConfigConf::SignalLength,
             JBB);
}

void Ndbcntr::execSTTOR(Signal *signal) {
  jamEntry();
  cstartPhase = signal->theData[1];

  cndbBlocksCount = 0;
  cinternalStartphase = cstartPhase - 1;

  switch (cstartPhase) {
    case 0:
      init_secretsfile_vars();
      if (m_ctx.m_config.getInitialStart()) {
        jam();
        g_eventLogger->info("Clearing filesystem in initial start");
        c_fsRemoveCount = 0;
        clearFilesystem(signal);
        return;
      }
      g_eventLogger->info("Not initial start");

      /**
       * If filesystem is encrypted initialize secrets file handling.
       * When secrets file handling is completed starts the sysfile
       * handling (in CLOSECONF).
       * If encryption is not used no need to handle secrets file
       * so starts sysfile handling immediately.
       **/
      if (c_encrypted_filesystem) {
        ////Open secrets files for reading
        open_secretsfile(signal, 0, true, false);
      } else {
        /** Checks if secrets file exists in the FS.
         * On a non-initial node restart the secrets file must not exist in the
         * FS. If secrets file is found OPENCONF will raise an error
         */
        open_secretsfile(signal, 0, true, true);
      }
      break;
    case ZSTART_PHASE_1:
      jam();

      // filesystemPassword is no longer necessary
      memset(globalData.filesystemPassword, 0,
             globalData.filesystemPasswordLength);
      globalData.filesystemPasswordLength = 0;

      startPhase1Lab(signal);
      break;
    case ZSTART_PHASE_2:
      jam();
      startPhase2Lab(signal);
      break;
    case ZSTART_PHASE_3:
      jam();
      startPhase3Lab(signal);
      break;
    case ZSTART_PHASE_4:
      jam();
      startPhase4Lab(signal);
      break;
    case ZSTART_PHASE_5:
      jam();
      startPhase5Lab(signal);
      break;
    case 6:
      jam();
      getNodeGroup(signal);
      sendSttorry(signal);
      break;
    case ZSTART_PHASE_8:
      jam();
      startPhase8Lab(signal);
      break;
    case ZSTART_PHASE_9:
      jam();
      startPhase9Lab(signal);
      break;
    default:
      jam();
      sendSttorry(signal);
      break;
  }  // switch
}  // Ndbcntr::execSTTOR()

void Ndbcntr::getNodeGroup(Signal *signal) {
  jam();
  CheckNodeGroups *sd = (CheckNodeGroups *)signal->getDataPtrSend();
  sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::GetNodeGroup;
  EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal,
                 CheckNodeGroups::SignalLength);
  jamEntry();
  c_nodeGroup = sd->output;
}

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
void Ndbcntr::execNDB_STTORRY(Signal *signal) {
  jamEntry();
  switch (cstartPhase) {
    case ZSTART_PHASE_2:
      jam();
      ph2GLab(signal);
      return;
      break;
    case ZSTART_PHASE_3:
      jam();
      ph3ALab(signal);
      return;
      break;
    case ZSTART_PHASE_4:
      jam();
      ph4BLab(signal);
      return;
      break;
    case ZSTART_PHASE_5:
      jam();
      ph5ALab(signal);
      return;
      break;
    case ZSTART_PHASE_6:
      jam();
      ph6ALab(signal);
      return;
      break;
    case ZSTART_PHASE_7:
      jam();
      ph6BLab(signal);
      return;
      break;
    case ZSTART_PHASE_8:
      jam();
      ph7ALab(signal);
      return;
      break;
    case ZSTART_PHASE_9:
      jam();
      g_eventLogger->info("NDB start phase 8 completed");
      ph8ALab(signal);
      return;
      break;
    default:
      jam();
      systemErrorLab(signal, __LINE__);
      return;
      break;
  }  // switch
}  // Ndbcntr::execNDB_STTORRY()

void Ndbcntr::startPhase1Lab(Signal *signal) {
  jamEntry();

  initData(signal);

  cdynamicNodeId = 0;

  NdbBlocksRecPtr ndbBlocksPtr;
  ndbBlocksPtr.i = 0;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBLQH_REF;
  ndbBlocksPtr.i = 1;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBDICT_REF;
  ndbBlocksPtr.i = 2;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBTUP_REF;
  ndbBlocksPtr.i = 3;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBACC_REF;
  ndbBlocksPtr.i = 4;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBTC_REF;
  ndbBlocksPtr.i = 5;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBDIH_REF;
  sendSttorry(signal);
  return;
}

void Ndbcntr::execREAD_NODESREF(Signal *signal) {
  jamEntry();
  systemErrorLab(signal, __LINE__);
  return;
}  // Ndbcntr::execREAD_NODESREF()

/*******************************/
/*  NDB_STARTREF               */
/*******************************/
void Ndbcntr::execNDB_STARTREF(Signal *signal) {
  jamEntry();
  systemErrorLab(signal, __LINE__);
  return;
}  // Ndbcntr::execNDB_STARTREF()

/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::startPhase2Lab(Signal *signal) {
  c_start.m_lastGci = 0;
  c_start.m_lastGciNodeId = getOwnNodeId();
  c_start.m_lastLcpId = 0;

  DihRestartReq *req = CAST_PTR(DihRestartReq, signal->getDataPtrSend());
  req->senderRef = reference();
  if (ERROR_INSERTED(1021)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(DBDIH_REF, GSN_DIH_RESTARTREQ, signal, 30000,
                        DihRestartReq::SignalLength);
  } else {
    sendSignal(DBDIH_REF, GSN_DIH_RESTARTREQ, signal,
               DihRestartReq::SignalLength, JBB);
  }
  return;
}

/*******************************/
/*  DIH_RESTARTCONF            */
/*******************************/
void Ndbcntr::execDIH_RESTARTCONF(Signal *signal) {
  jamEntry();

  /* Ignore bitmask sent in response */
  SectionHandle handle(this, signal);
  releaseSections(handle);

  const DihRestartConf *conf =
      CAST_CONSTPTR(DihRestartConf, signal->getDataPtrSend());
  c_start.m_lastGci = conf->latest_gci;
  c_start.m_lastLcpId = conf->latest_lcp_id;

  signal->theData[0] = c_start.m_lastLcpId;
  send_to_all_lqh(signal, GSN_LOCAL_LATEST_LCP_ID_REP, 1);

  /* Check for 'nothing read' values from local sysfile */
  if (unlikely((c_local_sysfile.m_restorable_flag ==
                ReadLocalSysfileReq::NODE_RESTORABLE_ON_ITS_OWN) &&
               (c_local_sysfile.m_max_restorable_gci == 1))) {
    jam();
    /**
     * In this case, we were unable to read a local sysfile at all,
     * but the distributed sysfile was readable.
     * This looks like an upgrade scenario, and we require an
     * explicit --initial for that.
     * Tell user via a process exit code, they must decide
     * themselves whether or not to use --initial.
     */
    if (!m_ctx.m_config.getInitialStart())  // TODO : Always?
    {
      jam();
      g_eventLogger->error(
          "Upgrading to a newer version with a newer "
          "LCP file format. Data node needs to be started "
          "with --initial");
      // in debug mode crash rather than exit
      CRASH_INSERTION(1007);
      progError(__LINE__, NDBD_EXIT_UPGRADE_INITIAL_REQUIRED);
      /* Never reach here */
      return;
    }
  }

  if (unlikely(ctypeOfStart == NodeState::ST_SYSTEM_RESTART_NOT_RESTORABLE &&
               c_local_sysfile.m_max_restorable_gci < c_start.m_lastGci)) {
    jam();
    /**
     * We were able to write the distributed sysfile with a restorable
     * GCI higher than the one recorded in the local sysfile. This can
     * happen with a crash directly after completing a restart and
     * before writing the local sysfile. The distributed sysfile will have
     * precedence here.
     */
    ctypeOfStart = NodeState::ST_SYSTEM_RESTART;
    c_local_sysfile.m_max_restorable_gci = c_start.m_lastGci;
    c_local_sysfile.m_restorable_flag =
        ReadLocalSysfileReq::NODE_RESTORABLE_ON_ITS_OWN;
    g_eventLogger->info(
        "Distributed sysfile more recent:"
        " Local sysfile: %s, gci: %u, version: %x",
        get_restorable_flag_string(c_local_sysfile.m_restorable_flag),
        c_local_sysfile.m_max_restorable_gci, c_local_sysfile.m_data[0]);
  }
  cdihStartType = ctypeOfStart;

  ph2ALab(signal);
  return;
}

void Ndbcntr::sendReadLocalSysfile(Signal *signal) {
  ReadLocalSysfileReq *req = (ReadLocalSysfileReq *)signal->getDataPtrSend();
  req->userPointer = 0;
  req->userReference = reference();
  sendSignal(NDBCNTR_REF, GSN_READ_LOCAL_SYSFILE_REQ, signal,
             ReadLocalSysfileReq::SignalLength, JBB);
}

void Ndbcntr::execREAD_LOCAL_SYSFILE_CONF(Signal *signal) {
  jamEntry();
  /**
   * No need to see results of the read since we have access to
   * the c_local_sysfile struct and can read them directly.
   * Set start type to SYSTEM_RESTART_NOT_RESTORABLE if not
   * restorable on our own.
   *
   * We will not become master node unless we can restore on our
   * own.
   */
  if (c_local_sysfile.m_restorable_flag ==
      ReadLocalSysfileReq::NODE_RESTORABLE_ON_ITS_OWN) {
    jam();
    ctypeOfStart = NodeState::ST_SYSTEM_RESTART;
  } else if (c_local_sysfile.m_restorable_flag ==
             ReadLocalSysfileReq::NODE_NOT_RESTORABLE_ON_ITS_OWN) {
    jam();
    ctypeOfStart = NodeState::ST_SYSTEM_RESTART_NOT_RESTORABLE;
  } else if (c_local_sysfile.m_restorable_flag ==
             ReadLocalSysfileReq::NODE_REQUIRE_INITIAL_RESTART) {
    jam();
    /**
     * It is possible that the crash occurred after writing the
     * distributed sysfile and before writing the local sysfile.
     * In this case the local sysfile will have precedence. This
     * case will be so rare that it is not important to handle
     * in the most optimal manner.
     */
    c_start.m_lastGci = 0;
    ctypeOfStart = NodeState::ST_INITIAL_START;
    ndbrequire(!m_ctx.m_config.getInitialStart());
    g_eventLogger->info("Clearing filesystem in initial restart");
    c_fsRemoveCount = 0;
    clearFilesystem(signal);
    return;
  } else {
    ndbabort();
  }
  sendSttorry(signal);
}

/*******************************/
/*  DIH_RESTARTREF             */
/*******************************/
void Ndbcntr::execDIH_RESTARTREF(Signal *signal) {
  jamEntry();
  /* Ignore bitmask sent in response */
  SectionHandle handle(this, signal);
  releaseSections(handle);

  ctypeOfStart = NodeState::ST_INITIAL_START;
  cdihStartType = ctypeOfStart;
  c_local_sysfile.m_initial_read_done = true;
  sendWriteLocalSysfile_initial(signal);
}

void Ndbcntr::sendWriteLocalSysfile_initial(Signal *signal) {
  WriteLocalSysfileReq *req = (WriteLocalSysfileReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = 0;
  req->nodeRestorableOnItsOwn =
      ReadLocalSysfileReq::NODE_REQUIRE_INITIAL_RESTART;
  req->maxGCIRestorable = 1;
  req->lastWrite = 0;
  sendSignal(NDBCNTR_REF, GSN_WRITE_LOCAL_SYSFILE_REQ, signal,
             WriteLocalSysfileReq::SignalLength, JBB);
}

void Ndbcntr::execWRITE_LOCAL_SYSFILE_CONF(Signal *signal) {
  WriteLocalSysfileConf *conf =
      (WriteLocalSysfileConf *)signal->getDataPtrSend();
  jamEntry();
  if (conf->userPointer == 0) {
    jam();
    if (m_local_lcp_started) {
      jam();
      write_local_sysfile_start_lcp_done(signal);
      return;
    }
    ph2ALab(signal);
  } else if (conf->userPointer == 1) {
    jam();
    send_to_all_lqh(signal, GSN_COPY_FRAG_NOT_IN_PROGRESS_REP, 1);
  } else {
    ndbabort();
  }
  return;
}

void Ndbcntr::ph2ALab(Signal *signal) {
  /******************************/
  /* request configured nodes   */
  /* from QMGR                  */
  /*  READ_NODESREQ             */
  /******************************/
  ReadNodesReq *req = (ReadNodesReq *)&signal->theData[0];
  req->myRef = reference();
  req->myVersion = NDB_VERSION_D;
  sendSignal(QMGR_REF, GSN_READ_NODESREQ, signal, ReadNodesReq::SignalLength,
             JBB);
  return;
}  // Ndbcntr::ph2ALab()

inline Uint64 setTimeout(Uint32 timeoutValue) {
  return (timeoutValue != 0) ? timeoutValue : ~(Uint64)0;
}

/*******************************/
/*  READ_NODESCONF             */
/*******************************/
void Ndbcntr::execREAD_NODESCONF(Signal *signal) {
  jamEntry();
  const ReadNodesConf *readNodes = (ReadNodesConf *)&signal->theData[0];

  cmasterNodeId = readNodes->masterNodeId;
  cdynamicNodeId = readNodes->ndynamicId;

  {
    ndbrequire(signal->getNoOfSections() == 1);
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    ndbrequire(ptr.sz == 5 * NdbNodeBitmask::Size);
    copy((Uint32 *)&readNodes->definedNodes.rep.data, ptr);
    releaseSections(handle);
  }
  /**
   * All defined nodes...
   *
   * QMGR have decided to start up using a set of nodes that
   * was deemed as appropriate by QMGR. These are the nodes
   * in c_clusterNodes. So all the nodes in c_clusterNodes
   * have registered into the heartbeat service handled by
   * QMGR.
   *
   * More nodes can arrive in the process in which case they
   * are reported in CM_ADD_REP by QMGR.
   */
  c_allDefinedNodes = readNodes->definedNodes;
  c_clusterNodes = readNodes->clusterNodes;

  Uint32 to_1 = 30000;
  Uint32 to_2 = 0;
  Uint32 to_3 = 0;

  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();

  ndbrequire(p != 0);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_PARTIAL_TIMEOUT, &to_1);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_PARTITION_TIMEOUT, &to_2);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_FAILURE_TIMEOUT, &to_3);

  c_start.m_startTime = NdbTick_getCurrentTicks();
  c_start.m_startPartialTimeout = setTimeout(to_1);
  c_start.m_startPartitionedTimeout = setTimeout(to_2);
  c_start.m_startFailureTimeout = setTimeout(to_3);

  sendCntrStartReq(signal);

  signal->theData[0] = ZSTARTUP;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 1);

  return;
}

void Ndbcntr::execCM_ADD_REP(Signal *signal) {
  jamEntry();
  ndbrequire(signal->theData[0] < MAX_NDB_NODES);
  c_clusterNodes.set(signal->theData[0]);
}

void Ndbcntr::sendCntrStartReq(Signal *signal) {
  jamEntry();

  if (getOwnNodeId() == cmasterNodeId) {
    jam();
    g_eventLogger->info(
        "Asking master node to accept our start "
        "(we are master, GCI = %u)",
        c_start.m_lastGci);
  } else {
    jam();
    g_eventLogger->info(
        "Asking master node to accept our start "
        "(nodeId = %u is master), GCI = %u",
        cmasterNodeId, c_start.m_lastGci);
  }

  if (c_start.m_lastGci > m_max_completed_gci) {
    jam();
    m_max_completed_gci = c_start.m_lastGci;
  }

  CntrStartReq *req = (CntrStartReq *)signal->getDataPtrSend();
  req->startType = ctypeOfStart;
  req->lastGci = c_start.m_lastGci;
  req->nodeId = getOwnNodeId();
  req->lastLcpId = c_start.m_lastLcpId;
  sendSignal(calcNdbCntrBlockRef(cmasterNodeId), GSN_CNTR_START_REQ, signal,
             CntrStartReq::SignalLength, JBB);
}

void Ndbcntr::execCNTR_START_REF(Signal *signal) {
  jamEntry();
  const CntrStartRef *ref = (CntrStartRef *)signal->getDataPtr();

  switch (ref->errorCode) {
    case CntrStartRef::NotMaster:
      jam();
      cmasterNodeId = ref->masterNodeId;
      sendCntrStartReq(signal);
      return;
    case CntrStartRef::StopInProgress:
      jam();
      progError(__LINE__, NDBD_EXIT_RESTART_DURING_SHUTDOWN);
  }
  ndbabort();
}

void Ndbcntr::StartRecord::reset() {
  m_starting.clear();
  m_waiting.clear();
  m_withLog.clear();
  m_withLogNotRestorable.clear();
  m_withoutLog.clear();
  m_waitTO.clear();
  m_lastGci = m_lastGciNodeId = 0;
  m_lastLcpId = 0;
  m_startPartialTimeout = ~0;
  m_startPartitionedTimeout = ~0;
  m_startFailureTimeout = ~0;

  m_logNodesCount = 0;
  std::memset(m_wait_sp, 0, sizeof(m_wait_sp));
}

void Ndbcntr::execCNTR_START_CONF(Signal *signal) {
  jamEntry();
  const CntrStartConf *conf = (CntrStartConf *)signal->getDataPtr();

  cnoStartNodes = conf->noStartNodes;
  ctypeOfStart = (NodeState::StartType)conf->startType;
  cdihStartType = ctypeOfStart;
  c_start.m_lastGci = conf->startGci;
  cmasterNodeId = conf->masterNodeId;
  NdbNodeBitmask tmp;

  if (signal->getNoOfSections() >= 1) {
    Uint32 num_sections = signal->getNoOfSections();
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);

    ndbrequire(handle.getSection(ptr, 0));
    memset(&signal->theData[CntrStartConf::SignalLength], 0,
           NdbNodeBitmask::Size * sizeof(Uint32));
    copy(&signal->theData[CntrStartConf::SignalLength], ptr);
    c_start.m_starting.assign(NdbNodeBitmask::Size,
                              &signal->theData[CntrStartConf::SignalLength]);

    ndbrequire(handle.getSection(ptr, 1));
    memset(&signal->theData[CntrStartConf::SignalLength], 0,
           NdbNodeBitmask::Size * sizeof(Uint32));
    copy(&signal->theData[CntrStartConf::SignalLength], ptr);
    tmp.assign(NdbNodeBitmask::Size,
               &signal->theData[CntrStartConf::SignalLength]);
    c_cntr_startedNodeSet.bitOR(tmp);

    const Uint32 version = getNodeInfo(cmasterNodeId).m_version;
    if (ndbd_send_started_bitmask(version)) {
      NdbNodeBitmask tmp2;
      jam();
      ndbrequire(num_sections == 3);
      ndbrequire(handle.getSection(ptr, 2));
      memset(&signal->theData[CntrStartConf::SignalLength], 0,
             NdbNodeBitmask::Size * sizeof(Uint32));
      copy(&signal->theData[CntrStartConf::SignalLength], ptr);
      tmp2.assign(NdbNodeBitmask::Size,
                  &signal->theData[CntrStartConf::SignalLength]);
      c_startedNodeSet.bitOR(tmp2);
    } else {
      jam();
      ndbrequire(num_sections == 2);
      c_startedNodeSet.bitOR(tmp);
    }
    releaseSections(handle);
  } else {
    jam();
    tmp.assign(NdbNodeBitmask48::Size, conf->startedNodes_v1);
    c_cntr_startedNodeSet.bitOR(tmp);
    c_startedNodeSet.bitOR(tmp);
    c_start.m_starting.assign(NdbNodeBitmask48::Size, conf->startingNodes_v1);
  }
  m_cntr_start_conf = true;
  g_eventLogger->info(
      "NDBCNTR master accepted us into cluster,"
      " start NDB start phase 1");

  switch (ctypeOfStart) {
    case NodeState::ST_INITIAL_START: {
      g_eventLogger->info("We are performing initial start of cluster");
      break;
    }
    case NodeState::ST_INITIAL_NODE_RESTART: {
      g_eventLogger->info("We are performing initial node restart");
      break;
    }
    case NodeState::ST_NODE_RESTART: {
      g_eventLogger->info("We are performing a node restart");
      break;
    }
    case NodeState::ST_SYSTEM_RESTART: {
      g_eventLogger->info(
          "We are performing a restart of the cluster"
          ", restoring GCI = %u",
          c_start.m_lastGci);
      if (c_start.m_lastGci > m_max_completed_gci) {
        jam();
        m_max_completed_gci = c_start.m_lastGci;
      }
      send_restorable_gci_rep_to_backup(signal, c_start.m_lastGci);
      break;
    }
    default: {
      ndbabort();
    }
  }
  ph2GLab(signal);
}

void Ndbcntr::execCNTR_START_REP(Signal *signal) {
  jamEntry();
  Uint32 nodeId = signal->theData[0];

  c_cntr_startedNodeSet.set(nodeId);
  c_startedNodeSet.set(nodeId);
  c_start.m_starting.clear(nodeId);

  /**
   * Inform all interested blocks that node has started
   */
  for (Uint32 i = 0; i < ALL_BLOCKS_SZ; i++) {
    if (isNdbMtLqh() || ALL_BLOCKS[i].in_ndbd)
      sendSignal(ALL_BLOCKS[i].Ref, GSN_NODE_START_REP, signal, 1, JBB);
  }

  g_eventLogger->info("Node %u has completed its restart", nodeId);

  StartPermRep *rep = (StartPermRep *)signal->getDataPtrSend();
  rep->startNodeId = nodeId;
  rep->reason = StartPermRep::CompletedStart;
  execSTART_PERMREP(signal);
}

void Ndbcntr::execSTART_PERMREP(Signal *signal) {
  jamEntry();
  StartPermRep rep = *(StartPermRep *)signal->getDataPtr();
  Uint32 nodeId = rep.startNodeId;
  c_cntr_startedNodeSet.set(nodeId);
  c_start.m_starting.clear(nodeId);

  if (!c_start.m_starting.isclear()) {
    jam();
    return;
  }

  if (cmasterNodeId != getOwnNodeId()) {
    jam();
    c_start.reset();
    return;
  }

  if (c_start.m_waiting.isclear()) {
    jam();
    c_start.reset();
    return;
  }

  if (rep.reason == StartPermRep::CompletedStart) {
    jam();
    DEB_NODE_GROUP_START(
        ("Node %u got permission to start,"
         " ready to start another node",
         nodeId));
  }
  startWaitingNodes(signal);
}

void Ndbcntr::execCNTR_START_REQ(Signal *signal) {
  jamEntry();
  CntrStartReq *req = (CntrStartReq *)signal->getDataPtr();

  const Uint32 nodeId = req->nodeId;
  Uint32 lastGci = req->lastGci;
  const NodeState::StartType st = (NodeState::StartType)req->startType;

  if (signal->getLength() == CntrStartReq::OldSignalLength) {
    jam();
    req->lastLcpId = 0;
  }
  const Uint32 lastLcpId = req->lastLcpId;
  if (cmasterNodeId == 0) {
    jam();
    // Has not completed READNODES yet
    sendSignalWithDelay(reference(), GSN_CNTR_START_REQ, signal, 100,
                        signal->getLength());
    return;
  }

  if (cmasterNodeId != getOwnNodeId()) {
    jam();
    sendCntrStartRef(signal, nodeId, CntrStartRef::NotMaster);
    return;
  }

  const NodeState &nodeState = getNodeState();
  switch (nodeState.startLevel) {
    case NodeState::SL_NOTHING:
    case NodeState::SL_CMVMI:
      jam();
      ndbabort();
    case NodeState::SL_STARTING:
    case NodeState::SL_STARTED:
      jam();
      break;

    case NodeState::SL_STOPPING_1:
    case NodeState::SL_STOPPING_2:
    case NodeState::SL_STOPPING_3:
    case NodeState::SL_STOPPING_4:
      jam();
      sendCntrStartRef(signal, nodeId, CntrStartRef::StopInProgress);
      return;
  }

  /**
   * Am I starting (or started)
   */
  const bool starting = (nodeState.startLevel != NodeState::SL_STARTED);

  c_start.m_waiting.set(nodeId);
  switch (st) {
    case NodeState::ST_INITIAL_START:
      jam();
      c_start.m_withoutLog.set(nodeId);
      break;
    case NodeState::ST_SYSTEM_RESTART_NOT_RESTORABLE:
      jam();
      /**
       * The node have done an unsuccessful attempt to restart.
       * It can only be restarted by aid of other nodes. So
       * it cannot be restored in first set of nodes in system
       * restart that restarts from REDO log. It is restored
       * later by synchronising with the other nodes in the
       * same node group.
       */
      c_start.m_withLogNotRestorable.set(nodeId);
      break;
    case NodeState::ST_SYSTEM_RESTART:
      jam();
      c_start.m_withLog.set(nodeId);
      ndbrequire(!(starting && lastGci > c_start.m_lastGci));
      if (starting) {
        jam();
        signal->theData[0] = nodeId;
        EXECUTE_DIRECT(DBDIH, GSN_GET_LATEST_GCI_REQ, signal, 1);
        Uint32 gci = signal->theData[0];
        if (gci > lastGci) {
          jam();
          g_eventLogger->info("Node had seen lastGci: %u, we saw %u",
                              req->lastGci, gci);
          lastGci = gci;
        }
        Uint32 i = c_start.m_logNodesCount++;
        c_start.m_logNodes[i].m_nodeId = nodeId;
        c_start.m_logNodes[i].m_lastGci = lastGci;

        /**
         * We will be the master, ensure that we don't start off with an LCP id
         * that have already been used. This can potentially happen in a system
         * restart where both nodes crashed in the same GCI, but one of the
         * nodes had started up an LCP before crashing. No need to switch
         * master, just ensure that the master uses an appropriate LCP id in its
         * first LCP.
         */
        signal->theData[0] = nodeId;
        signal->theData[1] = lastLcpId;
        EXECUTE_DIRECT(DBDIH, GSN_SET_LATEST_LCP_ID, signal, 2);
      }
      break;
    case NodeState::ST_NODE_RESTART:
    case NodeState::ST_INITIAL_NODE_RESTART:
    case NodeState::ST_ILLEGAL_TYPE:
      ndbabort();
  }

  const bool startInProgress = !c_start.m_starting.isclear();

  if (startInProgress) {
    jam();
    /**
     * We're already starting together with a bunch of nodes
     * Let this node wait...
     *
     * We will report the wait to DBDIH to keep track of waiting times in
     * the restart. We only report when a node restart is ongoing (that is
     * we are not starting ourselves).
     */
    if (!starting) {
      NdbcntrStartWaitRep *rep =
          (NdbcntrStartWaitRep *)signal->getDataPtrSend();
      rep->nodeId = nodeId;
      EXECUTE_DIRECT(DBDIH, GSN_NDBCNTR_START_WAIT_REP, signal,
                     NdbcntrStartWaitRep::SignalLength);
      return;
    }
  }

  if (starting) {
    jam();
    trySystemRestart(signal);
  } else {
    jam();
    startWaitingNodes(signal);
  }
  return;
}

void Ndbcntr::startWaitingNodes(Signal *signal) {
  if (!c_start.m_waitTO.isclear()) {
    jam();

    {
      char buf[NdbNodeBitmask::TextLength + 1];
      g_eventLogger->info("starting (TO) %s", c_start.m_waitTO.getText(buf));
    }

    /**
     * TO during SR
     *   this can run in parallel (nowadays :-)
     */
    for (Uint32 node_id = 1; node_id < MAX_NDB_NODES; node_id++) {
      if (c_start.m_waitTO.get(node_id)) {
        if (is_nodegroup_starting(signal, node_id)) {
          jam();
          g_eventLogger->info(
              "SR:Node %u has to wait for other node in"
              " same node group to complete start",
              node_id);
          /**
           * The node is waiting to be started, but another node
           * is still busy starting in this node group. So cannot
           * parallelise this with the other node restart within
           * the same node group.
           */
          continue;
        }
        c_start.m_waitTO.clear(node_id);
        c_start.m_starting.set(node_id);
        ndbrequire(c_start.m_waiting.get(node_id));
        c_start.m_waiting.clear(node_id);
        /**
         * They are stuck in CntrWaitRep::ZWAITPOINT_4_1
         *   have all meta data ok...but needs START_COPYREQ
         */
        BlockReference ref = calcNdbCntrBlockRef(node_id);
        CntrWaitRep *rep = (CntrWaitRep *)signal->getDataPtrSend();
        rep->nodeId = getOwnNodeId();
        rep->waitPoint = CntrWaitRep::ZWAITPOINT_4_2_TO;
        sendSignal(ref, GSN_CNTR_WAITREP, signal, 2, JBB);
      }
    }
    return;
  }

  bool first = true;
  Uint32 nodeId = 0;
  while (true) {
    jam();
    nodeId = c_start.m_waiting.find(nodeId + 1);
    if (nodeId == c_start.m_waiting.NotFound) {
      jam();
      ndbrequire(!first);
      /**
       * All the waiting nodes already have a node in their node group
       * performing a node restart. So no more nodes will be started
       * now. We will wait for those nodes to complete their node
       * restarts before we can move on.
       */
      return;
    }
    first = false;
    if (is_nodegroup_starting(signal, nodeId)) {
      jam();
      g_eventLogger->info(
          "NR:Node %u has to wait for other node in"
          " same node group to complete start",
          nodeId);
      /**
       * Another node in the same nodegroup is already starting.
       * We must wait to start this node since only one node per
       * node group is allowed to perform a node restart. This
       * ensures that we get back to a replicated state as soon
       * as possible and minimises the load on the live node(s)
       * during node restart handling.
       */
      continue;
    }
    const Uint32 Tref = calcNdbCntrBlockRef(nodeId);

    NodeState::StartType nrType = NodeState::ST_NODE_RESTART;
    const char *start_type_str = "node restart";
    if (c_start.m_withoutLog.get(nodeId)) {
      jam();
      nrType = NodeState::ST_INITIAL_NODE_RESTART;
      start_type_str = "initial node restart";
    } else if (c_start.m_withLogNotRestorable.get(nodeId)) {
      jam();
      start_type_str = "node restart (node not restorable on its own)";
    }

    /**
     * Let node perform restart
     */
    infoEvent("Start node: %u using %s", nodeId, start_type_str);
    g_eventLogger->info("Start node: %u using %s", nodeId, start_type_str);

    CntrStartConf *conf = (CntrStartConf *)signal->getDataPtrSend();
    conf->noStartNodes = 1;
    conf->startType = nrType;
    conf->startGci = ~0;  // Not used
    conf->masterNodeId = getOwnNodeId();
    Uint32 *startingNodes = &signal->theData[CntrStartConf::SignalLength];
    BitmaskImpl::clear(NdbNodeBitmask::Size, startingNodes);
    BitmaskImpl::set(NdbNodeBitmask::Size, startingNodes, nodeId);

    Uint32 packed_length1 =
        NdbNodeBitmask::getPackedLengthInWords(startingNodes);
    Uint32 packed_length2 = c_cntr_startedNodeSet.getPackedLengthInWords();
    const Uint32 version = getNodeInfo(refToNode(Tref)).m_version;
    if (ndbd_send_node_bitmask_in_section(version)) {
      static_assert(CntrStartConf::SignalLength + NdbNodeBitmask::Size <=
                    NDB_ARRAY_SIZE(signal->theData));
      LinearSectionPtr lsptr[3];
      Uint32 num_sections;
      lsptr[0].p = startingNodes;
      lsptr[0].sz = packed_length1;
      lsptr[1].p = c_cntr_startedNodeSet.rep.data;
      lsptr[1].sz = packed_length2;
      if (ndbd_send_started_bitmask(version)) {
        num_sections = 3;
        Uint32 packed_length3 = c_startedNodeSet.getPackedLengthInWords();
        lsptr[2].p = c_startedNodeSet.rep.data;
        lsptr[2].sz = packed_length3;
      } else {
        jam();
        num_sections = 2;
      }
      sendSignal(Tref, GSN_CNTR_START_CONF, signal, CntrStartConf::SignalLength,
                 JBB, lsptr, num_sections);
    } else if ((packed_length1 <= 2) && (packed_length2 <= 2)) {
      BitmaskImpl::clear(2, conf->startingNodes_v1);
      BitmaskImpl::set(2, conf->startingNodes_v1, nodeId);
      c_cntr_startedNodeSet.copyto(2, conf->startedNodes_v1);
      sendSignal(Tref, GSN_CNTR_START_CONF, signal,
                 CntrStartConf::SignalLength_v1, JBB);
    } else {
      ndbabort();
    }
    /**
     * A node restart is ongoing where we are master and we just accepted this
     * node to proceed with his node restart. Inform DBDIH about this event in
     * the node restart.
     */
    NdbcntrStartedRep *rep = (NdbcntrStartedRep *)signal->getDataPtrSend();
    rep->nodeId = nodeId;
    EXECUTE_DIRECT(DBDIH, GSN_NDBCNTR_STARTED_REP, signal,
                   NdbcntrStartedRep::SignalLength);

    c_start.m_waiting.clear(nodeId);
    c_start.m_withLog.clear(nodeId);
    c_start.m_withLogNotRestorable.clear(nodeId);
    c_start.m_withoutLog.clear(nodeId);
    c_start.m_starting.set(nodeId);
    return;
  }
}

void Ndbcntr::sendCntrStartRef(Signal *signal, Uint32 nodeId,
                               CntrStartRef::ErrorCode code) {
  CntrStartRef *ref = (CntrStartRef *)signal->getDataPtrSend();
  ref->errorCode = code;
  ref->masterNodeId = cmasterNodeId;
  sendSignal(calcNdbCntrBlockRef(nodeId), GSN_CNTR_START_REF, signal,
             CntrStartRef::SignalLength, JBB);
}

CheckNodeGroups::Output Ndbcntr::checkNodeGroups(Signal *signal,
                                                 const NdbNodeBitmask &mask) {
  CheckNodeGroups *sd = (CheckNodeGroups *)&signal->theData[0];
  sd->blockRef = reference();
  sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
  sd->mask = mask;
  EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal,
                 CheckNodeGroups::SignalLengthArbitCheckShort);
  jamEntry();
  return (CheckNodeGroups::Output)sd->output;
}

void Ndbcntr::update_withLog(void) {
  Uint32 latest_gci = c_start.m_lastGci;
  for (Uint32 i = 0; i < c_start.m_logNodesCount; i++) {
    jam();
    Uint32 nodeId = c_start.m_logNodes[i].m_nodeId;
    jamLine(nodeId);
    Uint32 node_last_gci = c_start.m_logNodes[i].m_lastGci;
    ndbrequire(node_last_gci <= latest_gci);
    if (node_last_gci < latest_gci) {
      jam();
      /**
       * Node cannot be restored from only its own REDO log.
       * We use a node restart after completing the restart
       * of the other nodes that have enough REDO log to
       * restart.
       *
       * This code makes it possible to always avoid using
       * the remote REDO log execution code by always using
       * node restart in these cases.
       */
      c_start.m_withLog.clear(nodeId);
      c_start.m_withLogNotRestorable.set(nodeId);
    }
  }
}

bool Ndbcntr::trySystemRestart(Signal *signal) {
  /**
   * System restart something
   */
  const bool allNodes = c_start.m_waiting.equal(c_allDefinedNodes);
  const bool allClusterNodes = c_start.m_waiting.equal(c_clusterNodes);

  if (!allClusterNodes) {
    jam();
    return false;
  }

  NodeState::StartType srType = NodeState::ST_SYSTEM_RESTART;
  if (c_start.m_waiting.equal(c_start.m_withoutLog)) {
    /**
     * A normal Initial Cluster Start, all nodes are starting without
     * any previous data.
     */
    jam();
    srType = NodeState::ST_INITIAL_START;
    c_start.m_starting = c_start.m_withoutLog;  // Used for starting...
    c_start.m_withoutLog.clear();
  } else {
    update_withLog();
    CheckNodeGroups::Output wLog = checkNodeGroups(signal, c_start.m_withLog);

    switch (wLog) {
      case CheckNodeGroups::Win:
        jam();
        break;
      case CheckNodeGroups::Lose:
        jam();
        // If we lose with all nodes, then we're in trouble
        ndbrequire(!allNodes);
        return false;
      case CheckNodeGroups::Partitioning: {
        jam();
        NdbNodeBitmask tmp = c_start.m_withLog;
        tmp.bitOR(c_start.m_withLogNotRestorable);
        tmp.bitOR(c_start.m_withoutLog);

        /**
         * tmp now contains all starting nodes. If we win the election with
         * those nodes then a partitioning isn't possible, so we can safely
         * continue the restart since we have enough nodes to get started.
         */
        CheckNodeGroups::Output wAll = checkNodeGroups(signal, tmp);
        switch (wAll) {
          case CheckNodeGroups::Win: {
            /* Ok to continue start */
            jam();
            break;
          }
          case CheckNodeGroups::Lose: {
            ndbabort();  // Cannot happen
          }
          case CheckNodeGroups::Partitioning: {
            jam();
            /**
             * Even with all nodes accounted for we are still in a partitioned
             * state. Only continue if we are ever allowed to start in a
             * partitioned state. Otherwise fail reporting that partitioned
             * start was only option and this wasn't allowed.
             */
            bool allowPartition =
                (c_start.m_startPartitionedTimeout != (Uint64)~0);

            if (allNodes) {
              if (allowPartition) {
                jam();
                break;
              }
              progError(__LINE__, NDBD_EXIT_PARTITIONED_SHUTDOWN,
                        "System restart not allowed in partitioned state");
            }
            break;
          }
          default: {
            ndbabort();
          }
        }
        break;
      }
      default: {
        ndbabort();
      }
    }

    // For now start only with the "logged"-ones.
    // Let the others do node restart afterwards...
    c_start.m_starting = c_start.m_withLog;
    c_start.m_withLog.clear();
  }

  /**
   * Okidoki, we try to start
   */
  CntrStartConf *conf = (CntrStartConf *)signal->getDataPtr();
  conf->noStartNodes = c_start.m_starting.count();
  conf->startType = srType;
  conf->startGci = c_start.m_lastGci;
  conf->masterNodeId = c_start.m_lastGciNodeId;

  ndbrequire(c_start.m_lastGciNodeId == getOwnNodeId());

  infoEvent("System Restart: master node: %u, num starting: %u, gci: %u",
            conf->masterNodeId, conf->noStartNodes, conf->startGci);
  g_eventLogger->info(
      "System Restart: master node: %u, num starting: %u,"
      " gci: %u",
      conf->masterNodeId, conf->noStartNodes, conf->startGci);
  char buf[NdbNodeBitmask::TextLength + 1];
  infoEvent("CNTR_START_CONF: started: %s", c_startedNodeSet.getText(buf));
  infoEvent("CNTR_START_CONF: cntr_started: %s",
            c_cntr_startedNodeSet.getText(buf));
  infoEvent("CNTR_START_CONF: starting: %s", c_start.m_starting.getText(buf));
  g_eventLogger->info("CNTR_START_CONF: started: %s",
                      c_startedNodeSet.getText(buf));
  g_eventLogger->info("CNTR_START_CONF: cntr_started: %s",
                      c_cntr_startedNodeSet.getText(buf));
  g_eventLogger->info("CNTR_START_CONF: starting: %s",
                      c_start.m_starting.getText(buf));

  Uint32 recNode = 0;
  NodeBitmask tmp_m_starting;
  tmp_m_starting = c_start.m_starting;
  while (!tmp_m_starting.isclear()) {
    recNode = tmp_m_starting.find(recNode + 1);
    tmp_m_starting.clear(recNode);

    const Uint32 ref = calcNdbCntrBlockRef(recNode);
    Uint32 packed_length1 = c_start.m_starting.getPackedLengthInWords();
    Uint32 packed_length2 = c_cntr_startedNodeSet.getPackedLengthInWords();
    const Uint32 version = getNodeInfo(recNode).m_version;

    if (ndbd_send_node_bitmask_in_section(version)) {
      Uint32 num_sections;
      LinearSectionPtr lsptr[3];
      lsptr[0].p = c_start.m_starting.rep.data;
      lsptr[0].sz = packed_length1;
      lsptr[1].p = c_cntr_startedNodeSet.rep.data;
      lsptr[1].sz = packed_length2;
      if (ndbd_send_started_bitmask(version)) {
        jam();
        num_sections = 3;
        Uint32 packed_length3 = c_startedNodeSet.getPackedLengthInWords();
        lsptr[2].p = c_startedNodeSet.rep.data;
        lsptr[2].sz = packed_length3;
      } else {
        jam();
        num_sections = 2;
      }
      sendSignal(ref, GSN_CNTR_START_CONF, signal, CntrStartConf::SignalLength,
                 JBB, lsptr, num_sections);
    } else if ((packed_length1 <= 2) && (packed_length2 <= 2)) {
      c_start.m_starting.copyto(NdbNodeBitmask::Size, conf->startingNodes_v1);
      c_cntr_startedNodeSet.copyto(NdbNodeBitmask::Size, conf->startedNodes_v1);
      sendSignal(ref, GSN_CNTR_START_CONF, signal,
                 CntrStartConf::SignalLength_v1, JBB);
    } else {
      ndbabort();
    }
  }

  c_start.m_waiting.bitANDC(c_start.m_starting);

  return true;
}

void Ndbcntr::ph2GLab(Signal *signal) {
  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }  // if
  g_eventLogger->info("NDB start phase 1 completed");
  sendSttorry(signal);
  return;
}  // Ndbcntr::ph2GLab()

/*
4.4  START PHASE 3 */
/*###########################################################################*/
// SEND SIGNAL NDBSTTOR TO ALL BLOCKS, ACC, DICT, DIH, LQH, TC AND TUP
// WHEN ALL BLOCKS HAVE RETURNED THEIR NDB_STTORRY ALL BLOCK HAVE FINISHED
// THEIR LOCAL CONNECTIONs SUCCESSFULLY
// AND THEN WE CAN SEND APPL_STARTREG TO INFORM QMGR THAT WE ARE READY TO
// SET UP DISTRIBUTED CONNECTIONS.
/*--------------------------------------------------------------*/
// THIS IS NDB START PHASE 3.
/*--------------------------------------------------------------*/
/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::startPhase3Lab(Signal *signal) {
  g_eventLogger->info("Start NDB start phase 2");
  /**
   * NDB start phase 2 runs in STTOR start phase 3.
   * At this point we have set up communication to all nodes that will
   * be part of the startup. Before proceeding with the rest of the
   * restart/start we will now set up multiple transporters to those
   * nodes that require this.
   *
   * To avoid doing this concurrently with other start phases we will
   * do it now, we want to have communication setup already, but we
   * want as little activity on the channels as possible to make it
   * easier to setup the new transporters between nodes in the same
   * node group.
   *
   * When coming back to NDBCNTR from QMGR (QMGR controls this set up
   * of multiple transporters) we have connection to other nodes set
   * up with multiple transporters. This will have impact on the
   * update rate we can sustain and also on the copy fragment phase
   * that will be faster than with only one transporter.
   */
  if (ctypeOfStart != NodeState::ST_INITIAL_NODE_RESTART &&
      ctypeOfStart != NodeState::ST_NODE_RESTART) {
    jam();
    signal->theData[0] = reference();
    sendSignal(QMGR_REF, GSN_SET_UP_MULTI_TRP_REQ, signal, 1, JBB);
    return;
  } else {
    jam();
    ph3ALab(signal);
  }
}

void Ndbcntr::execSET_UP_MULTI_TRP_CONF(Signal *signal) {
  g_eventLogger->info(
      "Completed setting up multiple transporters to nodes"
      " in the same node group");
  ph3ALab(signal);
  return;
}  // Ndbcntr::startPhase3Lab()

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
void Ndbcntr::ph3ALab(Signal *signal) {
  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }  // if
  g_eventLogger->info("NDB start phase 2 completed");
  sendSttorry(signal);
  return;
}  // Ndbcntr::ph3ALab()

/*
4.5  START PHASE 4      */
/*###########################################################################*/
// WAIT FOR ALL NODES IN CLUSTER TO CHANGE STATE INTO ZSTART ,
// APPL_CHANGEREP IS ALWAYS SENT WHEN SOMEONE HAVE
// CHANGED THEIR STATE. APPL_STARTCONF INDICATES THAT ALL NODES ARE IN START
// STATE SEND NDB_STARTREQ TO DIH AND THEN WAIT FOR NDB_STARTCONF
/*---------------------------------------------------------------------------*/
/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::startPhase4Lab(Signal *signal) {
  g_eventLogger->info("Start NDB start phase 3");
  ph4ALab(signal);
}  // Ndbcntr::startPhase4Lab()

void Ndbcntr::ph4ALab(Signal *signal) {
  ph4BLab(signal);
  return;
}  // Ndbcntr::ph4ALab()

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
void Ndbcntr::ph4BLab(Signal *signal) {
  /*--------------------------------------*/
  /* CASE: CSTART_PHASE = ZSTART_PHASE_4  */
  /*--------------------------------------*/
  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }  // if
  if (ERROR_INSERTED(1010)) {
    /* Just delay things for 10 seconds */
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(reference(), GSN_NDB_STTORRY, signal, 10000, 1);
    return;
  }
  g_eventLogger->info("NDB start phase 3 completed");
  if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
      (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    sendSttorry(signal);
    return;
  }  // if
  waitpoint41Lab(signal);
  return;
}  // Ndbcntr::ph4BLab()

void Ndbcntr::waitpoint41Lab(Signal *signal) {
  if (getOwnNodeId() == cmasterNodeId) {
    jam();
    /*--------------------------------------*/
    /* MASTER WAITS UNTIL ALL SLAVES HAS    */
    /* SENT THE REPORTS                     */
    /*--------------------------------------*/
    cnoWaitrep++;
    if (cnoWaitrep == cnoStartNodes) {
      jam();
      cnoWaitrep = 0;
      /*---------------------------------------------------------------------------*/
      // NDB_STARTREQ STARTS UP ALL SET UP OF DISTRIBUTION INFORMATION IN DIH
      // AND DICT. AFTER SETTING UP THIS DATA IT USES THAT DATA TO SET UP WHICH
      // FRAGMENTS THAT ARE TO START AND WHERE THEY ARE TO START. THEN IT SETS
      // UP THE FRAGMENTS AND RECOVERS THEM BY:
      //  1) READING A LOCAL CHECKPOINT FROM DISK.
      //  2) EXECUTING THE UNDO LOG ON INDEX AND DATA.
      //  3) EXECUTING THE FRAGMENT REDO LOG FROM ONE OR SEVERAL NODES TO
      //     RESTORE THE RESTART CONFIGURATION OF DATA IN NDB CLUSTER.
      /*---------------------------------------------------------------------------*/
      signal->theData[0] = reference();
      signal->theData[1] = ctypeOfStart;
      sendSignal(DBDIH_REF, GSN_NDB_STARTREQ, signal, 2, JBB);
    }  // if
  } else {
    jam();
    /*--------------------------------------*/
    /* SLAVE NODES WILL PASS HERE ONCE AND  */
    /* SEND A WAITPOINT REPORT TO MASTER.   */
    /* SLAVES WON'T DO ANYTHING UNTIL THEY  */
    /* RECEIVE A WAIT REPORT FROM THE MASTER*/
    /*--------------------------------------*/
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = CntrWaitRep::ZWAITPOINT_4_1;
    sendSignal(calcNdbCntrBlockRef(cmasterNodeId), GSN_CNTR_WAITREP, signal, 2,
               JBB);
  }  // if
  return;
}  // Ndbcntr::waitpoint41Lab()

void Ndbcntr::waitpoint42To(Signal *signal) {
  jam();

  /**
   * This is a ugly hack
   * To "easy" enable TO during SR
   *   a better solution would be to move "all" start handling
   *   from DIH to cntr...which knows what's going on
   */
  cdihStartType = NodeState::ST_SYSTEM_RESTART;
  ctypeOfStart = NodeState::ST_NODE_RESTART;

  /**
   * This is immensely ugly...but makes TUX work (yuck)
   */
  {
    NodeStateRep *rep = (NodeStateRep *)signal->getDataPtrSend();
    rep->nodeState = getNodeState();
    rep->nodeState.masterNodeId = cmasterNodeId;
    rep->nodeState.setNodeGroup(c_nodeGroup);
    rep->nodeState.starting.restartType = NodeState::ST_NODE_RESTART;

    sendSignal(DBTUX_REF, GSN_NODE_STATE_REP, signal,
               NodeStateRep::SignalLength, JBB);
  }

  /**
   * We were forced to perform TO
   */
  StartCopyReq *req = (StartCopyReq *)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = RNIL;
  req->flags = StartCopyReq::WAIT_LCP;
  req->startingNodeId = getOwnNodeId();
  sendSignal(DBDIH_REF, GSN_START_COPYREQ, signal, StartCopyReq::SignalLength,
             JBB);
}

void Ndbcntr::execSTART_COPYREF(Signal *signal) { jamEntry(); }

void Ndbcntr::execSTART_COPYCONF(Signal *signal) {
  jamEntry();
  sendSttorry(signal);
}

/*******************************/
/*  NDB_STARTCONF              */
/*******************************/
void Ndbcntr::execNDB_STARTCONF(Signal *signal) {
  jamEntry();

  NdbNodeBitmask tmp;
  if ((signal->getLength() >= 1 + NdbNodeBitmask48::Size) ||
      (signal->getNoOfSections() > 0)) {
    jam();
    if (signal->getNoOfSections() > 0) {
      ndbrequire(ndbd_send_node_bitmask_in_section(
          getNodeInfo(refToNode(signal->getSendersBlockRef())).m_version));
      SegmentedSectionPtr ptr;
      SectionHandle handle(this, signal);
      ndbrequire(handle.getSection(ptr, 0));
      ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
      copy(tmp.rep.data, ptr);
      releaseSections(handle);
    } else {
      tmp.assign(NdbNodeBitmask48::Size, signal->theData + 1);
    }

    if (!c_start.m_starting.equal(tmp)) {
      /**
       * Some nodes has been "excluded" from SR
       */
      char buf0[NdbNodeBitmask::TextLength + 1];
      char buf1[NdbNodeBitmask::TextLength + 1];
      g_eventLogger->info("execNDB_STARTCONF: changing from %s to %s",
                          c_start.m_starting.getText(buf0), tmp.getText(buf1));

      NdbNodeBitmask waiting = c_start.m_starting;
      waiting.bitANDC(tmp);

      c_start.m_waiting.bitOR(waiting);
      c_start.m_waitTO.bitOR(waiting);

      c_start.m_starting.assign(tmp);
      cnoStartNodes = c_start.m_starting.count();
    }
  }

  signal->theData[0] = getOwnNodeId();
  signal->theData[1] = CntrWaitRep::ZWAITPOINT_4_2;
  LinearSectionPtr ptr[3];
  ptr[0].p = c_start.m_starting.rep.data;
  ptr[0].sz = c_start.m_starting.getPackedLengthInWords();
  Uint32 node_id = 0;
  while ((node_id = c_start.m_starting.find(node_id + 1)) !=
         NdbNodeBitmask::NotFound) {
    const Uint32 ref = calcNdbCntrBlockRef(node_id);
    if (ndbd_send_node_bitmask_in_section(getNodeInfo(node_id).m_version)) {
      sendSignal(ref, GSN_CNTR_WAITREP, signal, 2, JBB, ptr, 1);
    } else {
      ndbrequire(ptr[0].sz <= NdbNodeBitmask48::Size);
      c_start.m_starting.copyto(NdbNodeBitmask48::Size, signal->theData + 2);
      sendSignal(ref, GSN_CNTR_WAITREP, signal, 2 + NdbNodeBitmask48::Size,
                 JBB);
    }
  }
  return;
}  // Ndbcntr::execNDB_STARTCONF()

/*
4.6  START PHASE 5      */
/*###########################################################################*/
// SEND APPL_RUN TO THE QMGR IN THIS BLOCK
// SEND NDB_STTOR ALL BLOCKS ACC, DICT, DIH, LQH, TC AND TUP THEN WAIT FOR
// THEIR NDB_STTORRY
/*---------------------------------------------------------------------------*/
/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::startPhase5Lab(Signal *signal) {
  g_eventLogger->info("Start NDB start phase 4");
  ph5ALab(signal);
  return;
}  // Ndbcntr::startPhase5Lab()

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
/*---------------------------------------------------------------------------*/
// THIS IS NDB START PHASE 5.
/*---------------------------------------------------------------------------*/
// IN THIS START PHASE TUP INITIALISES DISK FILES FOR DISK STORAGE IF INITIAL
// START. DIH WILL START UP
// THE GLOBAL CHECKPOINT PROTOCOL AND WILL CONCLUDE ANY UNFINISHED TAKE OVERS
// THAT STARTED BEFORE THE SYSTEM CRASH.
/*---------------------------------------------------------------------------*/
void Ndbcntr::ph5ALab(Signal *signal) {
  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }  // if
  g_eventLogger->info("NDB start phase 4 completed");

  cstartPhase = cstartPhase + 1;
  cinternalStartphase = cstartPhase - 1;
  if (getOwnNodeId() == cmasterNodeId) {
    switch (ctypeOfStart) {
      case NodeState::ST_INITIAL_START:
        jam();
        /*--------------------------------------*/
        /* MASTER CNTR IS RESPONSIBLE FOR       */
        /* CREATING SYSTEM TABLES               */
        /*--------------------------------------*/
        g_eventLogger->info(
            "Creating System Tables Starting"
            " as part of initial start");
        beginSchemaTransLab(signal);
        return;
      case NodeState::ST_SYSTEM_RESTART:
        jam();
        g_eventLogger->info(
            "As master we will wait for other nodes to reach"
            " the state waitpoint52 as well");
        waitpoint52Lab(signal);
        return;
      case NodeState::ST_NODE_RESTART:
      case NodeState::ST_INITIAL_NODE_RESTART:
        jam();
        break;
      case NodeState::ST_ILLEGAL_TYPE:
      case NodeState::ST_SYSTEM_RESTART_NOT_RESTORABLE:
        jam();
        break;
    }
    ndbabort();
  }

  /**
   * Not master
   */
  NdbSttor *const req = (NdbSttor *)signal->getDataPtrSend();
  switch (ctypeOfStart) {
    case NodeState::ST_NODE_RESTART:
    case NodeState::ST_INITIAL_NODE_RESTART:
      jam();
      /*----------------------------------------------------------------------*/
      // SEND NDB START PHASE 5 IN NODE RESTARTS TO COPY DATA TO THE NEWLY
      // STARTED NODE.
      /*----------------------------------------------------------------------*/
      req->senderRef = reference();
      req->nodeId = getOwnNodeId();
      req->internalStartPhase = cinternalStartphase;
      req->typeOfStart = cdihStartType;
      req->masterNodeId = cmasterNodeId;

      g_eventLogger->info("Start NDB start phase 5 (only to DBDIH)");
      //#define TRACE_STTOR
#ifdef TRACE_STTOR
      g_eventLogger->info("sending NDB_STTOR(%d) to DIH", cinternalStartphase);
#endif
      sendSignal(DBDIH_REF, GSN_NDB_STTOR, signal, NdbSttor::SignalLength, JBB);
      return;
    case NodeState::ST_INITIAL_START:
    case NodeState::ST_SYSTEM_RESTART:
      jam();
      /*--------------------------------------*/
      /* DURING SYSTEMRESTART AND INITALSTART:*/
      /* SLAVE NODES WILL PASS HERE ONCE AND  */
      /* SEND A WAITPOINT REPORT TO MASTER.   */
      /* SLAVES WON'T DO ANYTHING UNTIL THEY  */
      /* RECEIVE A WAIT REPORT FROM THE MASTER*/
      /* WHEN THE MASTER HAS FINISHED HIS WORK*/
      /*--------------------------------------*/
      g_eventLogger->info(
          "During cluster start/restart only master runs"
          " phase 5 of NDB start phases");
      g_eventLogger->info(
          "Report to master node our state and wait for master");

      signal->theData[0] = getOwnNodeId();
      signal->theData[1] = CntrWaitRep::ZWAITPOINT_5_2;
      sendSignal(calcNdbCntrBlockRef(cmasterNodeId), GSN_CNTR_WAITREP, signal,
                 2, JBB);
      return;
    default:
      ndbabort();
  }
}  // Ndbcntr::ph5ALab()

void Ndbcntr::waitpoint52Lab(Signal *signal) {
  cnoWaitrep = cnoWaitrep + 1;
  /*---------------------------------------------------------------------------*/
  // THIS WAITING POINT IS ONLY USED BY A MASTER NODE. WE WILL EXECUTE NDB START
  // PHASE 5 FOR DIH IN THE
  // MASTER. THIS WILL START UP LOCAL CHECKPOINTS AND WILL ALSO CONCLUDE ANY
  // UNFINISHED LOCAL CHECKPOINTS
  // BEFORE THE SYSTEM CRASH. THIS WILL ENSURE THAT WE ALWAYS RESTART FROM A
  // WELL KNOWN STATE.
  /*---------------------------------------------------------------------------*/
  /*--------------------------------------*/
  /* MASTER WAITS UNTIL HE RECEIVED WAIT  */
  /* REPORTS FROM ALL SLAVE CNTR          */
  /*--------------------------------------*/
  if (cnoWaitrep == cnoStartNodes) {
    jam();
    cnoWaitrep = 0;

    g_eventLogger->info("Start NDB start phase 5 (only to DBDIH)");
    NdbSttor *const req = (NdbSttor *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->nodeId = getOwnNodeId();
    req->internalStartPhase = cinternalStartphase;
    req->typeOfStart = cdihStartType;
    req->masterNodeId = cmasterNodeId;
#ifdef TRACE_STTOR
    g_eventLogger->info("sending NDB_STTOR(%d) to DIH", cinternalStartphase);
#endif
    sendSignal(DBDIH_REF, GSN_NDB_STTOR, signal, NdbSttor::SignalLength, JBB);
  }  // if
  return;
}  // Ndbcntr::waitpoint52Lab()

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
void Ndbcntr::ph6ALab(Signal *signal) {
  g_eventLogger->info("NDB start phase 5 completed");
  if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
      (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    waitpoint51Lab(signal);
    return;
  }  // if

  NodeReceiverGroup rg(NDBCNTR, c_start.m_starting);
  rg.m_nodes.clear(getOwnNodeId());
  signal->theData[0] = getOwnNodeId();
  signal->theData[1] = CntrWaitRep::ZWAITPOINT_5_1;
  sendSignal(rg, GSN_CNTR_WAITREP, signal, 2, JBB);

  waitpoint51Lab(signal);
  return;
}  // Ndbcntr::ph6ALab()

void Ndbcntr::waitpoint51Lab(Signal *signal) {
  cstartPhase = cstartPhase + 1;
  /*---------------------------------------------------------------------------*/
  // A FINAL STEP IS NOW TO SEND NDB_STTOR TO TC. THIS MAKES IT POSSIBLE TO
  // CONNECT TO TC FOR APPLICATIONS.
  // THIS IS NDB START PHASE 6 WHICH IS FOR ALL BLOCKS IN ALL NODES.
  /*---------------------------------------------------------------------------*/
  g_eventLogger->info("Start NDB start phase 6");
  cinternalStartphase = cstartPhase - 1;
  cndbBlocksCount = 0;
  ph6BLab(signal);
  return;
}  // Ndbcntr::waitpoint51Lab()

void Ndbcntr::ph6BLab(Signal *signal) {
  // c_missra.currentStartPhase - cstartPhase - cinternalStartphase =
  // 5 - 7 - 6
  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }  // if
  g_eventLogger->info("NDB start phase 6 completed");
  if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
      (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    sendSttorry(signal);
    return;
  }
  waitpoint61Lab(signal);
}

void Ndbcntr::waitpoint61Lab(Signal *signal) {
  if (getOwnNodeId() == cmasterNodeId) {
    jam();
    cnoWaitrep6++;
    if (cnoWaitrep6 == cnoStartNodes) {
      jam();
      NodeReceiverGroup rg(NDBCNTR, c_start.m_starting);
      rg.m_nodes.clear(getOwnNodeId());
      signal->theData[0] = getOwnNodeId();
      signal->theData[1] = CntrWaitRep::ZWAITPOINT_6_2;
      sendSignal(rg, GSN_CNTR_WAITREP, signal, 2, JBB);
      sendSttorry(signal);
    }
  } else {
    jam();
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = CntrWaitRep::ZWAITPOINT_6_1;
    sendSignal(calcNdbCntrBlockRef(cmasterNodeId), GSN_CNTR_WAITREP, signal, 2,
               JBB);
  }
}

// Start phase 8 (internal 7)
void Ndbcntr::startPhase8Lab(Signal *signal) {
  g_eventLogger->info("Start NDB start phase 7");
  cinternalStartphase = cstartPhase - 1;
  cndbBlocksCount = 0;
  ph7ALab(signal);
}

void Ndbcntr::ph7ALab(Signal *signal) {
  while (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }
  g_eventLogger->info("NDB start phase 7 completed");
  if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
      (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    sendSttorry(signal);
    return;
  }
  waitpoint71Lab(signal);
}

void Ndbcntr::waitpoint71Lab(Signal *signal) {
  if (getOwnNodeId() == cmasterNodeId) {
    jam();
    cnoWaitrep7++;
    if (cnoWaitrep7 == cnoStartNodes) {
      jam();
      NodeReceiverGroup rg(NDBCNTR, c_start.m_starting);
      rg.m_nodes.clear(getOwnNodeId());
      signal->theData[0] = getOwnNodeId();
      signal->theData[1] = CntrWaitRep::ZWAITPOINT_7_2;
      sendSignal(rg, GSN_CNTR_WAITREP, signal, 2, JBB);
      sendSttorry(signal);
    }
  } else {
    jam();
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = CntrWaitRep::ZWAITPOINT_7_1;
    sendSignal(calcNdbCntrBlockRef(cmasterNodeId), GSN_CNTR_WAITREP, signal, 2,
               JBB);
  }
}

// Start phase 9 (internal 8)
void Ndbcntr::startPhase9Lab(Signal *signal) {
  cinternalStartphase = cstartPhase - 1;
  cndbBlocksCount = 0;
  ph8ALab(signal);
}

void Ndbcntr::ph8ALab(Signal *signal) {
  sendSttorry(signal);
  resetStartVariables(signal);
  return;
}  // Ndbcntr::ph8BLab()

bool Ndbcntr::wait_sp(Signal *signal, Uint32 sp) {
  if (sp <= 2) return false;

  switch (ctypeOfStart) {
    case NodeState::ST_SYSTEM_RESTART:
    case NodeState::ST_INITIAL_START:
      /**
       * synchronized...
       */
      break;
    default:
      return false;
  }

  CntrWaitRep *rep = (CntrWaitRep *)signal->getDataPtrSend();
  rep->nodeId = getOwnNodeId();
  rep->waitPoint = RNIL;
  rep->request = CntrWaitRep::WaitFor;
  rep->sp = sp;

  sendSignal(calcNdbCntrBlockRef(cmasterNodeId), GSN_CNTR_WAITREP, signal,
             CntrWaitRep::SignalLength, JBB);

  return true;  // wait
}

void Ndbcntr::wait_sp_rep(Signal *signal) {
  CntrWaitRep rep = *(CntrWaitRep *)signal->getDataPtrSend();
  switch (rep.request) {
    case CntrWaitRep::WaitFor:
      jam();
      ndbrequire(cmasterNodeId == getOwnNodeId());
      break;
    case CntrWaitRep::Grant:
      jam();
      /**
       * We're allowed to proceed
       */
      c_missra.sendNextSTTOR(signal);
      return;
  }

  ndbrequire(rep.nodeId < NDB_ARRAY_SIZE(c_start.m_wait_sp));
  c_start.m_wait_sp[rep.nodeId] = rep.sp;

  /**
   * Check if we should allow someone to start...
   */
  Uint32 node = c_start.m_starting.find(0);
  ndbrequire(node < NDB_ARRAY_SIZE(c_start.m_wait_sp));
  Uint32 min = c_start.m_wait_sp[node];
  for (; node != NdbNodeBitmask::NotFound;
       node = c_start.m_starting.find(node + 1)) {
    if (c_start.m_wait_sp[node] < min) {
      min = c_start.m_wait_sp[node];
    }
  }

  if (min == 0) {
    /**
     * wait for more
     */
    return;
  }

  NdbNodeBitmask grantnodes;
  node = c_start.m_starting.find(0);
  for (; node != NdbNodeBitmask::NotFound;
       node = c_start.m_starting.find(node + 1)) {
    if (c_start.m_wait_sp[node] == min) {
      grantnodes.set(node);
      c_start.m_wait_sp[node] = 0;
    }
  }

  char buf[NdbNodeBitmask::TextLength + 1];
  g_eventLogger->info("Grant nodes to start phase: %u, nodes: %s", min,
                      grantnodes.getText(buf));

  NodeReceiverGroup rg(NDBCNTR, grantnodes);
  CntrWaitRep *conf = (CntrWaitRep *)signal->getDataPtrSend();
  conf->nodeId = getOwnNodeId();
  conf->waitPoint = RNIL;
  conf->request = CntrWaitRep::Grant;
  conf->sp = min;
  sendSignal(rg, GSN_CNTR_WAITREP, signal, CntrWaitRep::SignalLength, JBB);
}

/*******************************/
/*  CNTR_WAITREP               */
/*******************************/
void Ndbcntr::execCNTR_WAITREP(Signal *signal) {
  jamEntry();
  CntrWaitRep *rep = (CntrWaitRep *)signal->getDataPtr();

  Uint32 twaitPoint = rep->waitPoint;
  switch (twaitPoint) {
    case CntrWaitRep::ZWAITPOINT_4_1:
      jam();
      waitpoint41Lab(signal);
      break;
    case CntrWaitRep::ZWAITPOINT_4_2:
      jam();
      c_start.m_starting.clear();
      if (signal->getNoOfSections() >= 1) {
        SectionHandle handle(this, signal);
        SegmentedSectionPtr ptr;
        ndbrequire(handle.getSection(ptr, 0));
        ndbrequire(ptr.sz <= c_start.m_starting.Size);
        copy(c_start.m_starting.rep.data, ptr);
        releaseSections(handle);
      } else {
        c_start.m_starting.assign(NdbNodeBitmask48::Size, signal->theData + 2);
      }
      sendSttorry(signal);
      break;
    case CntrWaitRep::ZWAITPOINT_5_1:
      jam();
      g_eventLogger->info(
          "Master node %u have reached completion of NDB start"
          " phase 5",
          signal->theData[0]);
      waitpoint51Lab(signal);
      break;
    case CntrWaitRep::ZWAITPOINT_5_2:
      jam();
      g_eventLogger->info(
          "Node %u have reached completion of NDB start"
          " phase 4",
          signal->theData[0]);
      waitpoint52Lab(signal);
      break;
    case CntrWaitRep::ZWAITPOINT_6_1:
      jam();
      waitpoint61Lab(signal);
      break;
    case CntrWaitRep::ZWAITPOINT_6_2:
      jam();
      sendSttorry(signal);
      break;
    case CntrWaitRep::ZWAITPOINT_7_1:
      jam();
      waitpoint71Lab(signal);
      break;
    case CntrWaitRep::ZWAITPOINT_7_2:
      jam();
      sendSttorry(signal);
      break;
    case CntrWaitRep::ZWAITPOINT_4_2_TO:
      jam();
      waitpoint42To(signal);
      break;
    case RNIL:
      ndbrequire(signal->getLength() >= CntrWaitRep::SignalLength);
      wait_sp_rep(signal);
      return;
    default:
      jam();
      systemErrorLab(signal, __LINE__);
      break;
  }  // switch
}  // Ndbcntr::execCNTR_WAITREP()

/*******************************/
/*  NODE_FAILREP               */
/*******************************/
void Ndbcntr::execNODE_FAILREP(Signal *signal) {
  jamEntry();

  CRASH_INSERTION(1006);

  Uint32 senderRef = signal->getSendersBlockRef();
  Uint32 senderVersion = getNodeInfo(refToNode(senderRef)).m_version;

  NodeFailRep *nodeFail = (NodeFailRep *)&signal->theData[0];
  NdbNodeBitmask allFailed;
  Uint32 packed_nodebitmask_length = 0;

  if (signal->getNoOfSections() >= 1) {
    jam();
    ndbrequire(ndbd_send_node_bitmask_in_section(senderVersion));
    SectionHandle handle(this, signal);
    SegmentedSectionPtr ptr;
    ndbrequire(handle.getSection(ptr, 0));

    if (ERROR_INSERTED(1001)) {
      sendSignalWithDelay(reference(), GSN_NODE_FAILREP, signal, 100,
                          signal->getLength(), &handle);
      return;
    }
    ndbrequire(ptr.sz <= NdbNodeBitmask::Size);
    copy(allFailed.rep.data, ptr);
    releaseSections(handle);
  } else {
    if (ERROR_INSERTED(1001)) {
      sendSignalWithDelay(reference(), GSN_NODE_FAILREP, signal, 100,
                          signal->getLength());
      return;
    }
    allFailed.assign(NdbNodeBitmask48::Size, nodeFail->theNodes);
  }
  packed_nodebitmask_length = allFailed.getPackedLengthInWords();

  NdbNodeBitmask failedStarted = c_startedNodeSet;
  NdbNodeBitmask failedStarting = c_start.m_starting;
  NdbNodeBitmask failedWaiting = c_start.m_waiting;

  failedStarted.bitAND(allFailed);
  failedStarting.bitAND(allFailed);
  failedWaiting.bitAND(allFailed);

  const bool tMasterFailed = allFailed.get(cmasterNodeId);
  const bool tStarted = !failedStarted.isclear();
  const bool tStarting = !failedStarting.isclear();

  if (tMasterFailed) {
    jam();
    /**
     * If master has failed choose qmgr president as master
     */
    cmasterNodeId = nodeFail->masterNodeId;
  }

  /**
   * Clear node bitmasks from failed nodes
   */
  c_start.m_starting.bitANDC(allFailed);
  c_start.m_waiting.bitANDC(allFailed);
  c_start.m_withLog.bitANDC(allFailed);
  c_start.m_withoutLog.bitANDC(allFailed);
  c_start.m_withLogNotRestorable.bitANDC(allFailed);
  c_start.m_waitTO.bitANDC(allFailed);
  c_clusterNodes.bitANDC(allFailed);
  c_cntr_startedNodeSet.bitANDC(allFailed);
  c_startedNodeSet.bitANDC(allFailed);

  const NodeState &st = getNodeState();
  if (st.startLevel == st.SL_STARTING) {
    jam();

    const Uint32 phase = st.starting.startPhase;

    const bool tStartConf = (phase > 2) || (phase == 2 && cndbBlocksCount > 0);

    if (tMasterFailed) {
      progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED,
                "Unhandled node failure during restart");
    }

    if (tStartConf && tStarting) {
      // One of other starting nodes has crashed...
      progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED,
                "Unhandled node failure of starting node during restart");
    }

    if (tStartConf && tStarted) {
      // One of other started nodes has crashed...
      progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED,
                "Unhandled node failure of started node during restart");
    }

    Uint32 nodeId = 0;
    while (!allFailed.isclear()) {
      nodeId = allFailed.find(nodeId + 1);
      allFailed.clear(nodeId);
      signal->theData[0] = nodeId;
      sendSignal(QMGR_REF, GSN_NDB_FAILCONF, signal, 1, JBB);
    }  // for

    return;
  }

  ndbrequire(!allFailed.get(getOwnNodeId()));

  NodeFailRep *rep = (NodeFailRep *)&signal->theData[0];
  rep->masterNodeId = cmasterNodeId;

  // sending signals to self
  // Send node bitmask in linear section.
  LinearSectionPtr lsptr[3];

  lsptr[0].p = allFailed.rep.data;
  lsptr[0].sz = packed_nodebitmask_length;

  /* QMGR and DBDIH are notified first since some of the other block will
   * send NF_COMPLETREP when they completed NODE_FAILREP.
   */
  sendSignal(QMGR_REF, GSN_NODE_FAILREP, signal, NodeFailRep::SignalLength, JBB,
             lsptr, 1);

  sendSignal(DBDIH_REF, GSN_NODE_FAILREP, signal, NodeFailRep::SignalLength,
             JBB, lsptr, 1);

  /* DBDIH sends NODE_FAILREP to the other blocks that needs it. */

  if (c_stopRec.stopReq.senderRef) {
    jam();
    switch (c_stopRec.m_state) {
      case StopRecord::SR_WAIT_NODE_FAILURES: {
        jam();
        NdbNodeBitmask tmp;
        tmp.assign(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
        tmp.bitANDC(allFailed);
        tmp.copyto(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);

        if (tmp.isclear()) {
          jam();
          if (c_stopRec.stopReq.senderRef != RNIL) {
            jam();
            StopConf *const stopConf = (StopConf *)&signal->theData[0];
            stopConf->senderData = c_stopRec.stopReq.senderData;
            stopConf->nodeState = (Uint32)NodeState::SL_SINGLEUSER;
            sendSignal(c_stopRec.stopReq.senderRef, GSN_STOP_CONF, signal,
                       StopConf::SignalLength, JBB);
          }

          c_stopRec.stopReq.senderRef = 0;
          WaitGCPReq *req = (WaitGCPReq *)&signal->theData[0];
          req->senderRef = reference();
          req->senderData = StopRecord::SR_UNBLOCK_GCP_START_GCP;
          req->requestType = WaitGCPReq::UnblockStartGcp;
          sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal,
                     WaitGCPReq::SignalLength, JBA);
        }
        break;
      }
      case StopRecord::SR_QMGR_STOP_REQ: {
        NdbNodeBitmask tmp;
        tmp.assign(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
        tmp.bitANDC(allFailed);

        if (tmp.isclear()) {
          Uint32 nodeId = allFailed.find(0);
          tmp.set(nodeId);

          StopConf *conf = (StopConf *)signal->getDataPtrSend();
          conf->senderData = c_stopRec.stopReq.senderData;
          conf->nodeId = nodeId;
          sendSignal(reference(), GSN_STOP_CONF, signal, StopConf::SignalLength,
                     JBB);
        }

        tmp.copyto(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);

        break;
      }
      case StopRecord::SR_BLOCK_GCP_START_GCP:
      case StopRecord::SR_WAIT_COMPLETE_GCP:
      case StopRecord::SR_UNBLOCK_GCP_START_GCP:
      case StopRecord::SR_CLUSTER_SHUTDOWN:
        break;
    }
  }

  signal->theData[0] = NDB_LE_NODE_FAILREP;
  signal->theData[2] = 0;

  Uint32 nodeId = 0;
  while (!allFailed.isclear()) {
    nodeId = allFailed.find(nodeId + 1);
    allFailed.clear(nodeId);
    signal->theData[1] = nodeId;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }  // for

  return;
}  // Ndbcntr::execNODE_FAILREP()

/*******************************/
/*  READ_NODESREQ              */
/*******************************/
void Ndbcntr::execREAD_NODESREQ(Signal *signal) {
  jamEntry();

  /*----------------------------------------------------------------------*/
  // ANY BLOCK MAY SEND A REQUEST ABOUT NDB NODES AND VERSIONS IN THE
  // SYSTEM. THIS REQUEST CAN ONLY BE HANDLED IN
  // ABSOLUTE STARTPHASE 3 OR LATER
  /*----------------------------------------------------------------------*/
  BlockReference TuserBlockref = signal->theData[0];
  ReadNodesConf *const readNodes = (ReadNodesConf *)&signal->theData[0];

  /**
   * Prepare inactiveNodes bitmask.
   * The concept as such is by the way pretty useless.
   * It makes parallel starts more or less impossible...
   */
  NdbNodeBitmask tmp1;
  tmp1.bitOR(c_cntr_startedNodeSet);
  if (!getNodeState().getNodeRestartInProgress()) {
    tmp1.bitOR(c_start.m_starting);
  } else {
    tmp1.set(getOwnNodeId());
  }

  NdbNodeBitmask tmp2;
  tmp2.bitOR(c_allDefinedNodes);
  tmp2.bitANDC(tmp1);
  /**
   * Fill in return signal
   */
  readNodes->inactiveNodes = tmp2;
  readNodes->definedNodes = c_allDefinedNodes;
  readNodes->clusterNodes = c_clusterNodes;
  readNodes->startedNodes = c_cntr_startedNodeSet;
  readNodes->startingNodes = c_start.m_starting;

  readNodes->noOfNodes = c_allDefinedNodes.count();
  readNodes->masterNodeId = cmasterNodeId;
  readNodes->ndynamicId = cdynamicNodeId;
  if (m_cntr_start_conf) {
    jam();
    LinearSectionPtr lsptr[3];
    lsptr[0].p = readNodes->definedNodes.rep.data;
    lsptr[0].sz = 5 * NdbNodeBitmask::Size;
    sendSignal(TuserBlockref, GSN_READ_NODESCONF, signal,
               ReadNodesConf::SignalLength, JBB, lsptr, 1);
  } else {
    jam();
    signal->theData[0] = ZNOT_AVAILABLE;
    sendSignal(TuserBlockref, GSN_READ_NODESREF, signal, 1, JBB);
  }  // if
}  // Ndbcntr::execREAD_NODESREQ()

/*----------------------------------------------------------------------*/
// SENDS APPL_ERROR TO QMGR AND THEN SET A POINTER OUT OF BOUNDS
/*----------------------------------------------------------------------*/
void Ndbcntr::systemErrorLab(Signal *signal, int line) {
  progError(line, NDBD_EXIT_NDBREQUIRE); /* BUG INSERTION */
}  // Ndbcntr::systemErrorLab()

/*###########################################################################*/
/* CNTR MASTER CREATES AND INITIALIZES A SYSTEMTABLE AT INITIALSTART         */
/*       |-2048| # 1 00000001    |                                           */
/*       |  :  |   :             |                                           */
/*       | -1  | # 1 00000001    |                                           */
/*       |  1  |   0             | tupleid sequence now created on first use */
/*       |  :  |   :             |                   v                       */
/*       | 2048|   0             |                   v                       */
/*---------------------------------------------------------------------------*/
void Ndbcntr::beginSchemaTransLab(Signal *signal) {
  c_schemaTransId = reference();

  SchemaTransBeginReq *req = (SchemaTransBeginReq *)signal->getDataPtrSend();
  req->clientRef = reference();
  req->transId = c_schemaTransId;
  req->requestInfo = 0;
  sendSignal(DBDICT_REF, GSN_SCHEMA_TRANS_BEGIN_REQ, signal,
             SchemaTransBeginReq::SignalLength, JBB);
}

void Ndbcntr::execSCHEMA_TRANS_BEGIN_CONF(Signal *signal) {
  jamEntry();
  const SchemaTransBeginConf *conf =
      (SchemaTransBeginConf *)signal->getDataPtr();
  ndbrequire(conf->transId == c_schemaTransId);
  c_schemaTransKey = conf->transKey;

  createHashMap(signal, 0);
}

void Ndbcntr::execSCHEMA_TRANS_BEGIN_REF(Signal *signal) {
  jamEntry();
  ndbabort();
}

void Ndbcntr::createHashMap(Signal *signal, Uint32 idx) {
  CreateHashMapReq *const req = (CreateHashMapReq *)signal->getDataPtrSend();
  req->clientRef = reference();
  req->clientData = idx;
  req->requestInfo = CreateHashMapReq::CreateDefault;
  req->transId = c_schemaTransId;
  req->transKey = c_schemaTransKey;
  req->buckets = 0;
  req->fragments = NDB_PARTITION_BALANCE_FOR_RP_BY_LDM;
  sendSignal(DBDICT_REF, GSN_CREATE_HASH_MAP_REQ, signal,
             CreateHashMapReq::SignalLength, JBB);
}

void Ndbcntr::execCREATE_HASH_MAP_REF(Signal *signal) {
  jamEntry();

  ndbabort();
}

void Ndbcntr::execCREATE_HASH_MAP_CONF(Signal *signal) {
  jamEntry();
  CreateHashMapConf *conf = (CreateHashMapConf *)signal->getDataPtrSend();

  if (conf->senderData == 0) {
    jam();
    c_objectId = conf->objectId;
    c_objectVersion = conf->objectVersion;
  }

  createSystableLab(signal, 0);
}

void Ndbcntr::endSchemaTransLab(Signal *signal) {
  SchemaTransEndReq *req = (SchemaTransEndReq *)signal->getDataPtrSend();
  req->clientRef = reference();
  req->transId = c_schemaTransId;
  req->requestInfo = 0;
  req->transKey = c_schemaTransKey;
  req->flags = 0;
  sendSignal(DBDICT_REF, GSN_SCHEMA_TRANS_END_REQ, signal,
             SchemaTransEndReq::SignalLength, JBB);
}

void Ndbcntr::execSCHEMA_TRANS_END_CONF(Signal *signal) {
  jamEntry();
  c_schemaTransId = 0;
  c_schemaTransKey = RNIL;
  waitpoint52Lab(signal);
}

void Ndbcntr::execSCHEMA_TRANS_END_REF(Signal *signal) {
  jamEntry();
  SchemaTransEndRef *ref = (SchemaTransEndRef *)signal->getDataPtr();
  char buf[256];
  BaseString::snprintf(buf, sizeof(buf),
                       "Failed to commit schema trans, err: %u",
                       ref->errorCode);
  progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
}

void Ndbcntr::createDDObjects(Signal *signal, unsigned index) {
  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Uint32 propPage[256];
  LinearWriter w(propPage, 256);

  const ddentry *entry = &f_dd[index];

  switch (entry->type) {
    case DictTabInfo::LogfileGroup:
    case DictTabInfo::Tablespace: {
      jam();

      DictFilegroupInfo::Filegroup fg;
      fg.init();
      BaseString::snprintf(fg.FilegroupName, sizeof(fg.FilegroupName), "%s",
                           entry->name);
      fg.FilegroupType = entry->type;
      if (entry->type == DictTabInfo::LogfileGroup) {
        jam();
        fg.LF_UndoBufferSize = Uint32(entry->size);
      } else {
        jam();
        fg.TS_ExtentSize = Uint32(entry->size);
        fg.TS_LogfileGroupId = c_objectId;
        fg.TS_LogfileGroupVersion = c_objectVersion;
      }

      SimpleProperties::UnpackStatus s;
      s = SimpleProperties::pack(w, &fg, DictFilegroupInfo::Mapping,
                                 DictFilegroupInfo::MappingSize);

      Uint32 length = w.getWordsUsed();
      LinearSectionPtr ptr[3];
      ptr[0].p = &propPage[0];
      ptr[0].sz = length;

      CreateFilegroupReq *req = (CreateFilegroupReq *)signal->getDataPtrSend();
      req->senderRef = reference();
      req->senderData = index;
      req->objType = entry->type;
      req->transId = c_schemaTransId;
      req->transKey = c_schemaTransKey;
      req->requestInfo = 0;
      sendSignal(DBDICT_REF, GSN_CREATE_FILEGROUP_REQ, signal,
                 CreateFilegroupReq::SignalLength, JBB, ptr, 1);
      return;
    }
    case DictTabInfo::Undofile:
    case DictTabInfo::Datafile: {
      jam();
      Uint32 propPage[256];
      LinearWriter w(propPage, 256);
      DictFilegroupInfo::File f;
      f.init();
      BaseString::snprintf(f.FileName, sizeof(f.FileName), "%s", entry->name);
      f.FileType = entry->type;
      f.FilegroupId = c_objectId;
      f.FilegroupVersion = c_objectVersion;
      f.FileSizeHi = Uint32(entry->size >> 32);
      f.FileSizeLo = Uint32(entry->size);

      SimpleProperties::UnpackStatus s;
      s = SimpleProperties::pack(w, &f, DictFilegroupInfo::FileMapping,
                                 DictFilegroupInfo::FileMappingSize);

      Uint32 length = w.getWordsUsed();
      LinearSectionPtr ptr[3];
      ptr[0].p = &propPage[0];
      ptr[0].sz = length;

      CreateFileReq *req = (CreateFileReq *)signal->getDataPtrSend();
      req->senderRef = reference();
      req->senderData = index;
      req->objType = entry->type;
      req->transId = c_schemaTransId;
      req->transKey = c_schemaTransKey;
      req->requestInfo = CreateFileReq::ForceCreateFile;
      sendSignal(DBDICT_REF, GSN_CREATE_FILE_REQ, signal,
                 CreateFileReq::SignalLength, JBB, ptr, 1);
      return;
    }
    default:
      break;
  }

  endSchemaTransLab(signal);
}

void Ndbcntr::execCREATE_FILEGROUP_REF(Signal *signal) {
  jamEntry();
  CreateFilegroupRef *ref = (CreateFilegroupRef *)signal->getDataPtr();
  char buf[1024];

  const ddentry *entry = &f_dd[ref->senderData];

  if (entry->type == DictTabInfo::LogfileGroup) {
    BaseString::snprintf(buf, sizeof(buf), "create logfilegroup err %u",
                         ref->errorCode);
  } else if (entry->type == DictTabInfo::Tablespace) {
    BaseString::snprintf(buf, sizeof(buf), "create tablespace err %u",
                         ref->errorCode);
  }
  progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
}

void Ndbcntr::execCREATE_FILEGROUP_CONF(Signal *signal) {
  jamEntry();
  CreateFilegroupConf *conf = (CreateFilegroupConf *)signal->getDataPtr();
  c_objectId = conf->filegroupId;
  c_objectVersion = conf->filegroupVersion;
  createDDObjects(signal, conf->senderData + 1);
}

void Ndbcntr::execCREATE_FILE_REF(Signal *signal) {
  jamEntry();
  CreateFileRef *ref = (CreateFileRef *)signal->getDataPtr();
  char buf[1024];

  const ddentry *entry = &f_dd[ref->senderData];

  if (entry->type == DictTabInfo::Undofile) {
    BaseString::snprintf(buf, sizeof(buf), "create undofile %s err %u",
                         entry->name, ref->errorCode);
  } else if (entry->type == DictTabInfo::Datafile) {
    BaseString::snprintf(buf, sizeof(buf), "create datafile %s err %u",
                         entry->name, ref->errorCode);
  }
  progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
}

void Ndbcntr::execCREATE_FILE_CONF(Signal *signal) {
  jamEntry();
  CreateFileConf *conf = (CreateFileConf *)signal->getDataPtr();
  createDDObjects(signal, conf->senderData + 1);
}

void Ndbcntr::createSystableLab(Signal *signal, unsigned index) {
  if (index >= g_sysTableCount) {
    ndbassert(index == g_sysTableCount);
    createDDObjects(signal, 0);
    return;
  }
  const SysTable &table = *g_sysTableList[index];
  Uint32 propPage[256];
  LinearWriter w(propPage, 256);

  // XXX remove commented-out lines later

  w.first();
  w.add(DictTabInfo::TableName, table.name);
  w.add(DictTabInfo::TableLoggedFlag, table.tableLoggedFlag);
  // w.add(DictTabInfo::TableKValue, 6);
  // w.add(DictTabInfo::MinLoadFactor, 70);
  // w.add(DictTabInfo::MaxLoadFactor, 80);
  w.add(DictTabInfo::FragmentTypeVal, (Uint32)table.fragmentType);
  // w.add(DictTabInfo::NoOfKeyAttr, 1);
  w.add(DictTabInfo::NoOfAttributes, (Uint32)table.columnCount);
  // w.add(DictTabInfo::NoOfNullable, (Uint32)0);
  // w.add(DictTabInfo::NoOfVariable, (Uint32)0);
  // w.add(DictTabInfo::KeyLength, 1);
  w.add(DictTabInfo::TableTypeVal, (Uint32)table.tableType);
  w.add(DictTabInfo::SingleUserMode, (Uint32)NDB_SUM_READ_WRITE);
  w.add(DictTabInfo::HashMapObjectId, c_objectId);
  w.add(DictTabInfo::HashMapVersion, c_objectVersion);

  for (unsigned i = 0; i < table.columnCount; i++) {
    const SysColumn &column = table.columnList[i];
    ndbassert(column.pos == i);
    w.add(DictTabInfo::AttributeName, column.name);
    w.add(DictTabInfo::AttributeId, (Uint32)i);
    w.add(DictTabInfo::AttributeKeyFlag, (Uint32)column.keyFlag);
    w.add(DictTabInfo::AttributeStorageType, (Uint32)NDB_STORAGETYPE_MEMORY);
    switch (column.type) {
      case DictTabInfo::ExtVarbinary:
        jam();
        w.add(DictTabInfo::AttributeArrayType, (Uint32)NDB_ARRAYTYPE_SHORT_VAR);
        break;
      case DictTabInfo::ExtLongvarbinary:
        jam();
        w.add(DictTabInfo::AttributeArrayType,
              (Uint32)NDB_ARRAYTYPE_MEDIUM_VAR);
        break;
      default:
        jam();
        w.add(DictTabInfo::AttributeArrayType, (Uint32)NDB_ARRAYTYPE_FIXED);
        break;
    }
    w.add(DictTabInfo::AttributeNullableFlag, (Uint32)column.nullable);
    w.add(DictTabInfo::AttributeExtType, (Uint32)column.type);
    w.add(DictTabInfo::AttributeExtLength, (Uint32)column.length);
    w.add(DictTabInfo::AttributeEnd, (Uint32) true);
  }
  w.add(DictTabInfo::TableEnd, (Uint32) true);

  Uint32 length = w.getWordsUsed();
  LinearSectionPtr ptr[3];
  ptr[0].p = &propPage[0];
  ptr[0].sz = length;

  CreateTableReq *const req = (CreateTableReq *)signal->getDataPtrSend();
  req->clientRef = reference();
  req->clientData = index;
  req->requestInfo = 0;
  req->transId = c_schemaTransId;
  req->transKey = c_schemaTransKey;
  sendSignal(DBDICT_REF, GSN_CREATE_TABLE_REQ, signal,
             CreateTableReq::SignalLength, JBB, ptr, 1);
  return;
}  // Ndbcntr::createSystableLab()

void Ndbcntr::execCREATE_TABLE_REF(Signal *signal) {
  jamEntry();
  progError(__LINE__, NDBD_EXIT_NDBREQUIRE, "CREATE_TABLE_REF");
  return;
}  // Ndbcntr::execDICTTABREF()

void Ndbcntr::execCREATE_TABLE_CONF(Signal *signal) {
  jamEntry();
  const CreateTableConf *conf = (const CreateTableConf *)signal->getDataPtr();
  // csystabId = conf->tableId;
  ndbrequire(conf->transId == c_schemaTransId);
  ndbrequire(conf->senderData < g_sysTableCount);
  const SysTable &table = *g_sysTableList[conf->senderData];
  table.tableId = conf->tableId;
  table.tableVersion = conf->tableVersion;
  createSystableLab(signal, conf->senderData + 1);
  return;
}  // Ndbcntr::execDICTTABCONF()

/*******************************/
/*  GETGCICONF                 */
/*******************************/
void Ndbcntr::execGETGCICONF(Signal *signal) {
  jamEntry();

  waitpoint52Lab(signal);
  return;
}  // Ndbcntr::execGETGCICONF()

/*---------------------------------------------------------------------------*/
/*INITIALIZE VARIABLES AND RECORDS                                           */
/*---------------------------------------------------------------------------*/
void Ndbcntr::initData(Signal *signal) {
  c_start.reset();
  cmasterNodeId = 0;
  cnoStartNodes = 0;
  cnoWaitrep = 0;
}  // Ndbcntr::initData()

/*---------------------------------------------------------------------------*/
/*RESET VARIABLES USED DURING THE START                                      */
/*---------------------------------------------------------------------------*/
void Ndbcntr::resetStartVariables(Signal *signal) {
  cnoStartNodes = 0;
  cnoWaitrep6 = cnoWaitrep7 = 0;
}  // Ndbcntr::resetStartVariables()

/*---------------------------------------------------------------------------*/
// SEND THE SIGNAL
// INPUT                  CNDB_BLOCKS_COUNT
/*---------------------------------------------------------------------------*/
void Ndbcntr::sendNdbSttor(Signal *signal) {
  NdbBlocksRecPtr ndbBlocksPtr;

  ndbBlocksPtr.i = cndbBlocksCount;
  ptrCheckGuard(ndbBlocksPtr, ZSIZE_NDB_BLOCKS_REC, ndbBlocksRec);

  NdbSttor *const req = (NdbSttor *)signal->getDataPtrSend();
  req->senderRef = reference();
  req->nodeId = getOwnNodeId();
  req->internalStartPhase = cinternalStartphase;
  req->typeOfStart = ctypeOfStart;
  req->masterNodeId = cmasterNodeId;

  for (int i = 0; i < 16; i++) {
    // Garbage
    req->config[i] = 0x88776655;
  }

  //#define MAX_STARTPHASE 2
#ifdef TRACE_STTOR
  g_eventLogger->info("sending NDB_STTOR(%d) to %s", cinternalStartphase,
                      getBlockName(refToBlock(ndbBlocksPtr.p->blockref)));
#endif
  if (refToBlock(ndbBlocksPtr.p->blockref) == DBDIH)
    req->typeOfStart = cdihStartType;
  sendSignal(ndbBlocksPtr.p->blockref, GSN_NDB_STTOR, signal, 22, JBB);
  cndbBlocksCount++;
}  // Ndbcntr::sendNdbSttor()

/*---------------------------------------------------------------------------*/
// JUST SEND THE SIGNAL
/*---------------------------------------------------------------------------*/
void Ndbcntr::sendSttorry(Signal *signal, Uint32 delayed) {
  signal->theData[3] = ZSTART_PHASE_1;
  signal->theData[4] = ZSTART_PHASE_2;
  signal->theData[5] = ZSTART_PHASE_3;
  signal->theData[6] = ZSTART_PHASE_4;
  signal->theData[7] = ZSTART_PHASE_5;
  signal->theData[8] = ZSTART_PHASE_6;
  // skip simulated phase 7
  signal->theData[9] = ZSTART_PHASE_8;
  signal->theData[10] = ZSTART_PHASE_9;
  signal->theData[11] = ZSTART_PHASE_END;
  if (delayed == 0) {
    sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 12, JBB);
    return;
  }
  sendSignalWithDelay(NDBCNTR_REF, GSN_STTORRY, signal, delayed, 12);
}  // Ndbcntr::sendSttorry()

void Ndbcntr::execDUMP_STATE_ORD(Signal *signal) {
  jamEntry();
  DumpStateOrd *const &dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg = dumpState->args[0];

  if (arg == 13) {
    infoEvent("Cntr: cstartPhase = %d, cinternalStartphase = %d, block = %d",
              cstartPhase, cinternalStartphase, cndbBlocksCount);
    infoEvent("Cntr: cmasterNodeId = %d", cmasterNodeId);
  }

  if (arg == DumpStateOrd::NdbcntrTestStopOnError) {
    if (m_ctx.m_config.stopOnError() == true)
      ((Configuration &)m_ctx.m_config).stopOnError(false);

    const BlockReference tblockref = calcNdbCntrBlockRef(getOwnNodeId());

    SystemError *const sysErr = (SystemError *)&signal->theData[0];
    sysErr->errorCode = SystemError::TestStopOnError;
    sysErr->errorRef = reference();
    sendSignal(tblockref, GSN_SYSTEM_ERROR, signal, SystemError::SignalLength,
               JBA);
  }

  if (arg == DumpStateOrd::NdbcntrStopNodes) {
    NdbNodeBitmask mask;
    for (Uint32 i = 1; i < signal->getLength(); i++)
      mask.set(signal->theData[i]);

    StopReq *req = (StopReq *)signal->getDataPtrSend();
    req->senderRef = RNIL;
    req->senderData = 123;
    req->requestInfo = 0;
    req->singleuser = 0;
    req->singleUserApi = 0;
    mask.copyto(NdbNodeBitmask::Size, req->nodes);
    StopReq::setPerformRestart(req->requestInfo, 1);
    StopReq::setNoStart(req->requestInfo, 1);
    StopReq::setStopNodes(req->requestInfo, 1);
    StopReq::setStopAbort(req->requestInfo, 1);

    LinearSectionPtr lsptr[3];
    lsptr[0].p = req->nodes;
    lsptr[0].sz = mask.getPackedLengthInWords();
    sendSignal(reference(), GSN_STOP_REQ, signal, StopReq::SignalLength, JBB,
               lsptr, 1);
    return;
  }

  if (arg == 71) {
#ifdef ERROR_INSERT
    if (signal->getLength() == 2) {
      c_error_insert_extra = signal->theData[1];
      SET_ERROR_INSERT_VALUE(1002);
    } else if (ERROR_INSERTED(1002)) {
      CLEAR_ERROR_INSERT_VALUE;
    }
#endif
  }

}  // Ndbcntr::execDUMP_STATE_ORD()

void Ndbcntr::updateNodeState(Signal *signal, const NodeState &newState) const {
  NodeStateRep *const stateRep = (NodeStateRep *)&signal->theData[0];

  if (newState.startLevel == NodeState::SL_STARTED) {
    CRASH_INSERTION(1000);
  }

  stateRep->nodeState = newState;
  stateRep->nodeState.masterNodeId = cmasterNodeId;
  stateRep->nodeState.setNodeGroup(c_nodeGroup);

  for (Uint32 i = 0; i < ALL_BLOCKS_SZ; i++) {
    if (isNdbMtLqh() || ALL_BLOCKS[i].in_ndbd)
      sendSignal(ALL_BLOCKS[i].Ref, GSN_NODE_STATE_REP, signal,
                 NodeStateRep::SignalLength, JBB);
  }
}

void Ndbcntr::execRESUME_REQ(Signal *signal) {
  // ResumeReq * const req = (ResumeReq *)&signal->theData[0];
  // ResumeRef * const ref = (ResumeRef *)&signal->theData[0];

  jamEntry();

  signal->theData[0] = NDB_LE_SingleUser;
  signal->theData[1] = 2;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

  // Uint32 senderData = req->senderData;
  // BlockReference senderRef = req->senderRef;
  NodeState newState(NodeState::SL_STARTED);
  updateNodeState(signal, newState);
  c_stopRec.stopReq.senderRef = 0;
  send_node_started_rep(signal);
}

void Ndbcntr::execSTOP_REQ(Signal *signal) {
  StopReq *const req = (StopReq *)&signal->theData[0];
  StopRef *const ref = (StopRef *)&signal->theData[0];
  Uint32 singleuser = req->singleuser;
  jamEntry();
  Uint32 senderData = req->senderData;
  BlockReference senderRef = req->senderRef;
  bool abort = StopReq::getStopAbort(req->requestInfo);
  bool stopnodes = StopReq::getStopNodes(req->requestInfo);

  if (signal->getNoOfSections() >= 1) {
    ndbrequire(ndbd_send_node_bitmask_in_section(
        getNodeInfo(req->senderRef).m_version));
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    NdbNodeBitmask::clear(req->nodes);
    copy(req->nodes, ptr);
    releaseSections(handle);
  } else {
    memset(&req->nodes[NdbNodeBitmask48::Size], 0, _NDB_NBM_DIFF_BYTES);
  }

  if (!singleuser && (getNodeState().startLevel < NodeState::SL_STARTED ||
                      (abort && !stopnodes))) {
    /**
     * Node is not started yet
     *
     * So stop it quickly
     */
    jam();
    const Uint32 reqInfo = req->requestInfo;
    if (StopReq::getPerformRestart(reqInfo)) {
      jam();
      StartOrd *startOrd = (StartOrd *)&signal->theData[0];
      startOrd->restartInfo = reqInfo;
      sendSignal(CMVMI_REF, GSN_START_ORD, signal, 1, JBA);
    } else {
      jam();
      sendSignal(CMVMI_REF, GSN_STOP_ORD, signal, 1, JBA);
    }
    return;
  }

  if (c_stopRec.stopReq.senderRef != 0 ||
      (cmasterNodeId == getOwnNodeId() && !c_start.m_starting.isclear())) {
    /**
     * Requested a system shutdown
     */
    if (!singleuser && StopReq::getSystemStop(req->requestInfo)) {
      jam();
      LinearSectionPtr lsptr[3];
      lsptr[0].p = req->nodes;
      lsptr[0].sz = NdbNodeBitmask::getPackedLengthInWords(req->nodes);
      SectionHandle handle(this);
      ndbrequire(import(handle.m_ptr[0], lsptr[0].p, lsptr[0].sz));
      handle.m_cnt = 1;

      sendSignalWithDelay(reference(), GSN_STOP_REQ, signal, 100,
                          StopReq::SignalLength, &handle);
      return;
    }

    /**
     * Requested a node shutdown
     */
    if (c_stopRec.stopReq.senderRef &&
        StopReq::getSystemStop(c_stopRec.stopReq.requestInfo))
      ref->errorCode = StopRef::SystemShutdownInProgress;
    else
      ref->errorCode = StopRef::NodeShutdownInProgress;
    ref->senderData = senderData;
    ref->masterNodeId = cmasterNodeId;

    if (senderRef != RNIL)
      sendSignal(senderRef, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);
    return;
  }

  if (stopnodes && !abort) {
    jam();
    ref->errorCode = StopRef::UnsupportedNodeShutdown;
    ref->senderData = senderData;
    ref->masterNodeId = cmasterNodeId;
    if (senderRef != RNIL)
      sendSignal(senderRef, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);
    return;
  }

  if (stopnodes && cmasterNodeId != getOwnNodeId()) {
    jam();
    ref->errorCode = StopRef::MultiNodeShutdownNotMaster;
    ref->senderData = senderData;
    ref->masterNodeId = cmasterNodeId;
    if (senderRef != RNIL)
      sendSignal(senderRef, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);
    return;
  }

  c_stopRec.stopReq = *req;
  c_stopRec.stopInitiatedTime = NdbTick_getCurrentTicks();

  if (ERROR_INSERTED(1022) || ERROR_INSERTED(1023) || ERROR_INSERTED(1024)) {
    jam();
    g_eventLogger->info("Extending TcTimeout by 5000 millis");
    c_stopRec.stopReq.transactionTimeout += 5000;
  }

  if (stopnodes) {
    jam();

    if (!c_stopRec.checkNodeFail(signal)) {
      jam();
      return;
    }

    char buf[NdbNodeBitmask::TextLength + 1];
    NdbNodeBitmask mask;
    mask.assign(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
    infoEvent("Initiating shutdown abort of %s", mask.getText(buf));
    g_eventLogger->info("Initiating shutdown abort of %s", mask.getText(buf));

    WaitGCPReq *req = (WaitGCPReq *)&signal->theData[0];
    req->senderRef = reference();
    req->senderData = StopRecord::SR_BLOCK_GCP_START_GCP;
    req->requestType = WaitGCPReq::BlockStartGcp;
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, WaitGCPReq::SignalLength,
               JBB);
    return;
  } else if (!singleuser) {
    if (StopReq::getSystemStop(c_stopRec.stopReq.requestInfo)) {
      jam();
      if (StopReq::getPerformRestart(c_stopRec.stopReq.requestInfo)) {
        ((Configuration &)m_ctx.m_config).stopOnError(false);
      }
    }
    if (!c_stopRec.checkNodeFail(signal)) {
      jam();
      return;
    }
    signal->theData[0] = NDB_LE_NDBStopStarted;
    signal->theData[1] =
        StopReq::getSystemStop(c_stopRec.stopReq.requestInfo) ? 1 : 0;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  } else {
    signal->theData[0] = NDB_LE_SingleUser;
    signal->theData[1] = 0;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  }

  DEB_NODE_STOP(("Setting node state to SL_STOPPING_1"));
  NodeState newState(NodeState::SL_STOPPING_1,
                     StopReq::getSystemStop(c_stopRec.stopReq.requestInfo));

  if (singleuser) {
    newState.setSingleUser(true);
    newState.setSingleUserApi(c_stopRec.stopReq.singleUserApi);
  }
  updateNodeState(signal, newState);
  signal->theData[0] = ZSHUTDOWN;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::StopRecord::checkTimeout(Signal *signal) {
  jamEntry();

  if (!cntr.getNodeState().getSingleUserMode())
    if (!checkNodeFail(signal)) {
      jam();
      return;
    }

  switch (cntr.getNodeState().startLevel) {
    case NodeState::SL_STOPPING_1:
      checkApiTimeout(signal);
      break;
    case NodeState::SL_STOPPING_2:
      checkTcTimeout(signal);
      break;
    case NodeState::SL_STOPPING_3:
      checkLqhTimeout_1(signal);
      break;
    case NodeState::SL_STOPPING_4:
      checkLqhTimeout_2(signal);
      break;
    case NodeState::SL_SINGLEUSER:
      break;
    default:
      ndbabort();
  }
}

bool Ndbcntr::StopRecord::checkNodeFail(Signal *signal) {
  jam();
  if (StopReq::getSystemStop(stopReq.requestInfo)) {
    jam();
    return true;
  }

  /**
   * Check if I can survive me stopping
   */
  NdbNodeBitmask ndbMask;
  ndbMask.assign(cntr.c_startedNodeSet);

  if (StopReq::getStopNodes(stopReq.requestInfo)) {
    NdbNodeBitmask tmp;
    tmp.assign(NdbNodeBitmask::Size, stopReq.nodes);

    NdbNodeBitmask ndbStopNodes;
    ndbStopNodes.assign(NdbNodeBitmask::Size, stopReq.nodes);
    ndbStopNodes.bitAND(ndbMask);
    ndbStopNodes.copyto(NdbNodeBitmask::Size, stopReq.nodes);

    ndbMask.bitANDC(tmp);

    bool allNodesStopped = true;
    int i;
    for (i = 0; i < (int)NdbNodeBitmask::Size; i++) {
      if (stopReq.nodes[i] != 0) {
        allNodesStopped = false;
        break;
      }
    }

    if (allNodesStopped) {
      StopConf *const stopConf = (StopConf *)&signal->theData[0];
      stopConf->senderData = stopReq.senderData;
      stopConf->nodeState = (Uint32)NodeState::SL_NOTHING;
      cntr.sendSignal(stopReq.senderRef, GSN_STOP_CONF, signal,
                      StopConf::SignalLength, JBB);
      stopReq.senderRef = 0;
      return false;
    }

  } else {
    ndbMask.clear(cntr.getOwnNodeId());
  }

  CheckNodeGroups *sd = (CheckNodeGroups *)&signal->theData[0];
  sd->blockRef = cntr.reference();
  sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck |
                    CheckNodeGroups::UseBeforeFailMask;
  sd->mask = ndbMask;
  sd->before_fail_mask = cntr.c_startedNodeSet;
  cntr.EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal,
                      CheckNodeGroups::SignalLengthArbitCheckLong);
  jamEntry();
  switch (sd->output) {
    case CheckNodeGroups::Win:
    case CheckNodeGroups::Partitioning:
      return true;
      break;
  }

  StopRef *const ref = (StopRef *)&signal->theData[0];

  ref->senderData = stopReq.senderData;
  ref->errorCode = StopRef::NodeShutdownWouldCauseSystemCrash;
  ref->masterNodeId = cntr.cmasterNodeId;

  const BlockReference bref = stopReq.senderRef;
  if (bref != RNIL)
    cntr.sendSignal(bref, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);

  stopReq.senderRef = 0;

  if (cntr.getNodeState().startLevel != NodeState::SL_SINGLEUSER) {
    NodeState newState(NodeState::SL_STARTED);
    cntr.updateNodeState(signal, newState);
    cntr.send_node_started_rep(signal);
  }

  signal->theData[0] = NDB_LE_NDBStopAborted;
  cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB);

  return false;
}

void Ndbcntr::StopRecord::checkApiTimeout(Signal *signal) {
  const Int32 timeout = stopReq.apiTimeout;
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  if (timeout >= 0 &&
      NdbTick_Elapsed(stopInitiatedTime, now).milliSec() >= (Uint64)timeout) {
    // || checkWithApiInSomeMagicWay)
    jam();
    DEB_NODE_STOP(("Setting node state to SL_STOPPING_2"));
    NodeState newState(NodeState::SL_STOPPING_2,
                       StopReq::getSystemStop(stopReq.requestInfo));
    if (stopReq.singleuser) {
      newState.setSingleUser(true);
      newState.setSingleUserApi(stopReq.singleUserApi);
    }
    cntr.updateNodeState(signal, newState);

    stopInitiatedTime = now;
  }

  signal->theData[0] = ZSHUTDOWN;
  cntr.sendSignalWithDelay(cntr.reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::StopRecord::checkTcTimeout(Signal *signal) {
  const Int32 timeout = stopReq.transactionTimeout;
  const NDB_TICKS now = NdbTick_getCurrentTicks();
#ifdef DEBUG_NODE_STOP
  const Int32 elapsed = NdbTick_Elapsed(stopInitiatedTime, now).milliSec();
  DEB_NODE_STOP(("timeout: %d, elapsed: %d", timeout, elapsed));
#endif
  if (timeout >= 0 &&
      NdbTick_Elapsed(stopInitiatedTime, now).milliSec() >= (Uint64)timeout) {
    // || checkWithTcInSomeMagicWay)
    jam();
    if (stopReq.getSystemStop(stopReq.requestInfo) || stopReq.singleuser) {
      jam();
      if (stopReq.singleuser) {
        jam();
        AbortAllReq *req = (AbortAllReq *)&signal->theData[0];
        req->senderRef = cntr.reference();
        req->senderData = 12;
        cntr.sendSignal(DBTC_REF, GSN_ABORT_ALL_REQ, signal,
                        AbortAllReq::SignalLength, JBB);
      } else {
        DEB_NODE_STOP(("WAIT_GCP_REQ ShutdownSync"));
#ifdef ERROR_INSERT
        if (cntr.ERROR_INSERT_VALUE == 1023) {
          /* Test failure of laggard */
          jamNoBlock();
          cntr.progError(__LINE__, NDBD_EXIT_ERROR_INSERT, __FILE__);
          return;
        }
        if (cntr.ERROR_INSERT_VALUE == 1024) {
          jamNoBlock();
          /* Test failure of Master during WAIT_GCP_REQ */
          signal->theData[0] = 9999;
          cntr.sendSignal(numberToRef(CMVMI, cntr.cmasterNodeId),
                          GSN_TAMPER_ORD, signal, 1, JBB);
        }
#endif
        WaitGCPReq *req = (WaitGCPReq *)&signal->theData[0];
        req->senderRef = cntr.reference();
        req->senderData = StopRecord::SR_CLUSTER_SHUTDOWN;
        req->requestType = WaitGCPReq::ShutdownSync;
        cntr.sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal,
                        WaitGCPReq::SignalLength, JBB);
      }
    } else {
      jam();
      StopPermReq *req = (StopPermReq *)&signal->theData[0];
      req->senderRef = cntr.reference();
      req->senderData = 12;
      cntr.sendSignal(DBDIH_REF, GSN_STOP_PERM_REQ, signal,
                      StopPermReq::SignalLength, JBB);
    }
    return;
  }
  signal->theData[0] = ZSHUTDOWN;
  cntr.sendSignalWithDelay(cntr.reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::execSTOP_PERM_REF(Signal *signal) {
  // StopPermRef* const ref = (StopPermRef*)&signal->theData[0];

  jamEntry();

  signal->theData[0] = ZSHUTDOWN;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::execSTOP_PERM_CONF(Signal *signal) {
  jamEntry();

  AbortAllReq *req = (AbortAllReq *)&signal->theData[0];
  req->senderRef = reference();
  req->senderData = 12;
  sendSignal(DBTC_REF, GSN_ABORT_ALL_REQ, signal, AbortAllReq::SignalLength,
             JBB);
}

void Ndbcntr::execABORT_ALL_CONF(Signal *signal) {
  jamEntry();
  if (c_stopRec.stopReq.singleuser) {
    jam();

    NodeState newState(NodeState::SL_SINGLEUSER);
    newState.setSingleUser(true);
    newState.setSingleUserApi(c_stopRec.stopReq.singleUserApi);
    updateNodeState(signal, newState);
    c_stopRec.stopInitiatedTime = NdbTick_getCurrentTicks();

    StopConf *const stopConf = (StopConf *)&signal->theData[0];
    stopConf->senderData = c_stopRec.stopReq.senderData;
    stopConf->nodeState = (Uint32)NodeState::SL_SINGLEUSER;
    sendSignal(c_stopRec.stopReq.senderRef, GSN_STOP_CONF, signal,
               StopConf::SignalLength, JBB);

    c_stopRec.stopReq.senderRef = 0;  // the command is done

    signal->theData[0] = NDB_LE_SingleUser;
    signal->theData[1] = 1;
    signal->theData[2] = c_stopRec.stopReq.singleUserApi;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  } else {
    jam();
    DEB_NODE_STOP(("Setting node state to SL_STOPPING_3"));
    NodeState newState(NodeState::SL_STOPPING_3,
                       StopReq::getSystemStop(c_stopRec.stopReq.requestInfo));
    updateNodeState(signal, newState);

    c_stopRec.stopInitiatedTime = NdbTick_getCurrentTicks();

    signal->theData[0] = ZSHUTDOWN;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
  }
}

void Ndbcntr::execABORT_ALL_REF(Signal *signal) {
  jamEntry();

  StopRef *const stopRef = (StopRef *)&signal->theData[0];
  stopRef->senderData = c_stopRec.stopReq.senderData;
  stopRef->errorCode = StopRef::TransactionAbortFailed;
  stopRef->masterNodeId = cmasterNodeId;
  sendSignal(c_stopRec.stopReq.senderRef, GSN_STOP_REF, signal,
             StopRef::SignalLength, JBB);
}

void Ndbcntr::StopRecord::checkLqhTimeout_1(Signal *signal) {
  const Int32 timeout = stopReq.readOperationTimeout;
  const NDB_TICKS now = NdbTick_getCurrentTicks();

  if (timeout >= 0 &&
      NdbTick_Elapsed(stopInitiatedTime, now).milliSec() >= (Uint64)timeout) {
    // || checkWithLqhInSomeMagicWay)
    jam();

    ChangeNodeStateReq *req = (ChangeNodeStateReq *)&signal->theData[0];

    DEB_NODE_STOP(("Setting node state to SL_STOPPING_4"));
    NodeState newState(NodeState::SL_STOPPING_4,
                       StopReq::getSystemStop(stopReq.requestInfo));
    req->nodeState = newState;
    req->senderRef = cntr.reference();
    req->senderData = 12;
    cntr.sendSignal(DBLQH_REF, GSN_CHANGE_NODE_STATE_REQ, signal,
                    ChangeNodeStateReq::SignalLength, JBB);
    return;
  }
  signal->theData[0] = ZSHUTDOWN;
  cntr.sendSignalWithDelay(cntr.reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::execCHANGE_NODE_STATE_CONF(Signal *signal) {
  jamEntry();

  /**
   * stop replication stream
   */
  signal->theData[0] = reference();
  signal->theData[1] = 12;
  sendSignal(SUMA_REF, GSN_STOP_ME_REQ, signal, 2, JBB);
}

void Ndbcntr::execSTOP_ME_REF(Signal *signal) {
  jamEntry();
  ndbabort();
}

void Ndbcntr::execSTOP_ME_CONF(Signal *signal) {
  jamEntry();

  const StopMeConf *conf = CAST_CONSTPTR(StopMeConf, signal->getDataPtr());
  if (conf->senderData == 12) {
    /**
     * Remove node from transactions
     */
    signal->theData[0] = reference();
    signal->theData[1] = 13;
    sendSignal(DBDIH_REF, GSN_STOP_ME_REQ, signal, 2, JBB);
    return;
  }

  DEB_NODE_STOP(("2:Setting node state to SL_STOPPING_4"));
  NodeState newState(NodeState::SL_STOPPING_4,
                     StopReq::getSystemStop(c_stopRec.stopReq.requestInfo));
  updateNodeState(signal, newState);

  c_stopRec.stopInitiatedTime = NdbTick_getCurrentTicks();
  signal->theData[0] = ZSHUTDOWN;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::StopRecord::checkLqhTimeout_2(Signal *signal) {
  const Int32 timeout = stopReq.operationTimeout;
  const NDB_TICKS now = NdbTick_getCurrentTicks();

  if (timeout >= 0 &&
      NdbTick_Elapsed(stopInitiatedTime, now).milliSec() >= (Uint64)timeout) {
    // || checkWithLqhInSomeMagicWay)
    jam();
    if (StopReq::getPerformRestart(stopReq.requestInfo)) {
      jam();
      StartOrd *startOrd = (StartOrd *)&signal->theData[0];
      startOrd->restartInfo = stopReq.requestInfo;
      cntr.sendSignal(CMVMI_REF, GSN_START_ORD, signal, 2, JBA);
    } else {
      jam();
      cntr.sendSignal(CMVMI_REF, GSN_STOP_ORD, signal, 1, JBA);
    }
    return;
  }
  signal->theData[0] = ZSHUTDOWN;
  cntr.sendSignalWithDelay(cntr.reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::execWAIT_GCP_REF(Signal *signal) {
  jamEntry();

  // WaitGCPRef* const ref = (WaitGCPRef*)&signal->theData[0];

  WaitGCPReq *req = (WaitGCPReq *)&signal->theData[0];
  req->senderRef = reference();
  req->senderData = StopRecord::SR_CLUSTER_SHUTDOWN;
  req->requestType = WaitGCPReq::ShutdownSync;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, WaitGCPReq::SignalLength,
             JBB);
}

void Ndbcntr::execWAIT_GCP_CONF(Signal *signal) {
  jamEntry();

  WaitGCPConf *conf = (WaitGCPConf *)signal->getDataPtr();

  switch (conf->senderData) {
    case StopRecord::SR_BLOCK_GCP_START_GCP: {
      jam();
      /**
       *
       */
      if (!c_stopRec.checkNodeFail(signal)) {
        jam();
        goto unblock;
      }

      WaitGCPReq *req = (WaitGCPReq *)&signal->theData[0];
      req->senderRef = reference();
      req->senderData = StopRecord::SR_WAIT_COMPLETE_GCP;
      req->requestType = WaitGCPReq::CompleteIfRunning;

      sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, WaitGCPReq::SignalLength,
                 JBB);
      return;
    }
    case StopRecord::SR_UNBLOCK_GCP_START_GCP: {
      jam();
      return;
    }
    case StopRecord::SR_WAIT_COMPLETE_GCP: {
      jam();
      if (!c_stopRec.checkNodeFail(signal)) {
        jam();
        goto unblock;
      }

      Uint32 recNode = 0;
      NdbNodeBitmask tmp;
      tmp.assign(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
      c_stopRec.m_stop_req_counter = tmp;
      StopReq *stopReq = (StopReq *)&signal->theData[0];
      *stopReq = c_stopRec.stopReq;
      stopReq->senderRef = reference();
      Uint32 temp_node_bitmask[NdbNodeBitmask::Size];
      NdbNodeBitmask::clear(temp_node_bitmask);
      NdbNodeBitmask::assign(temp_node_bitmask, stopReq->nodes);

      while (!tmp.isclear()) {
        recNode = tmp.find(recNode + 1);
        tmp.clear(recNode);
        const Uint32 ref = calcQmgrBlockRef(recNode);
        Uint32 packed_length =
            NdbNodeBitmask::getPackedLengthInWords(stopReq->nodes);
        const Uint32 receiverVersion = getNodeInfo(recNode).m_version;
        if (ndbd_send_node_bitmask_in_section(receiverVersion)) {
          jam();
          LinearSectionPtr lsptr[3];
          lsptr[0].p = stopReq->nodes;
          lsptr[0].sz = packed_length;
          sendSignal(ref, GSN_STOP_REQ, signal, StopReq::SignalLength, JBA,
                     lsptr, 1);
          NdbNodeBitmask::assign(stopReq->nodes, temp_node_bitmask);
        } else if (packed_length <= NdbNodeBitmask48::Size) {
          jam();
          sendSignal(ref, GSN_STOP_REQ, signal, StopReq::SignalLength_v1, JBA);
        } else {
          ndbabort();
        }
      }

      c_stopRec.m_state = StopRecord::SR_QMGR_STOP_REQ;
      return;
    }
    case StopRecord::SR_CLUSTER_SHUTDOWN: {
      jam();
      break;
    }
  }

  {
    ndbrequire(StopReq::getSystemStop(c_stopRec.stopReq.requestInfo));
    DEB_NODE_STOP(("2:Setting node state to SL_STOPPING_3"));
    NodeState newState(NodeState::SL_STOPPING_3, true);

    /**
     * Inform QMGR so that arbitrator won't kill us
     */
    NodeStateRep *rep = (NodeStateRep *)&signal->theData[0];
    rep->nodeState = newState;
    rep->nodeState.masterNodeId = cmasterNodeId;
    rep->nodeState.setNodeGroup(c_nodeGroup);
    EXECUTE_DIRECT(QMGR, GSN_NODE_STATE_REP, signal,
                   NodeStateRep::SignalLength);

    if (StopReq::getPerformRestart(c_stopRec.stopReq.requestInfo)) {
      jam();
      StartOrd *startOrd = (StartOrd *)&signal->theData[0];
      startOrd->restartInfo = c_stopRec.stopReq.requestInfo;
      sendSignalWithDelay(CMVMI_REF, GSN_START_ORD, signal, 500,
                          StartOrd::SignalLength);
    } else {
      jam();
      sendSignalWithDelay(CMVMI_REF, GSN_STOP_ORD, signal, 500, 1);
    }
    return;
  }

unblock:
  WaitGCPReq *req = (WaitGCPReq *)&signal->theData[0];
  req->senderRef = reference();
  req->senderData = StopRecord::SR_UNBLOCK_GCP_START_GCP;
  req->requestType = WaitGCPReq::UnblockStartGcp;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, WaitGCPReq::SignalLength,
             JBB);
}

void Ndbcntr::execSTOP_CONF(Signal *signal) {
  jamEntry();
  StopConf *conf = (StopConf *)signal->getDataPtr();
  ndbrequire(c_stopRec.m_state == StopRecord::SR_QMGR_STOP_REQ);
  c_stopRec.m_stop_req_counter.clearWaitingFor(conf->nodeId);
  if (c_stopRec.m_stop_req_counter.done()) {
    char buf[NdbNodeBitmask::TextLength + 1];
    NdbNodeBitmask mask;
    mask.assign(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
    infoEvent("Stopping of %s", mask.getText(buf));
    g_eventLogger->info("Stopping of %s", mask.getText(buf));

    /**
     * Kill any node...
     */
    FailRep *const failRep = (FailRep *)&signal->theData[0];
    failRep->failCause = FailRep::ZMULTI_NODE_SHUTDOWN;
    failRep->failSourceNodeId = getOwnNodeId();
    NodeReceiverGroup rg(QMGR, c_clusterNodes);
    Uint32 nodeId = 0;
    while (
        (nodeId = NdbNodeBitmask::find(c_stopRec.stopReq.nodes, nodeId + 1)) !=
        NdbNodeBitmask::NotFound) {
      failRep->failNodeId = nodeId;
      sendSignal(rg, GSN_FAIL_REP, signal, FailRep::SignalLength, JBA);
    }
    c_stopRec.m_state = StopRecord::SR_WAIT_NODE_FAILURES;
    return;
  }
}

void Ndbcntr::execSTTORRY(Signal *signal) {
  jamEntry();
  c_missra.execSTTORRY(signal);
}

void Ndbcntr::execREAD_CONFIG_CONF(Signal *signal) {
  jamEntry();
  c_missra.execREAD_CONFIG_CONF(signal);
}

void Ndbcntr::execSTART_ORD(Signal *signal) {
  jamEntry();
  c_missra.execSTART_ORD(signal);
}

#define CLEAR_DX (8 + 1 + NDB_MAX_LOG_PARTS)
#define CLEAR_LCP (BackupFormat::NDB_MAX_LCP_FILES)
#define CLEAR_DD 2
// FileSystemPathDataFiles FileSystemPathUndoFiles

void Ndbcntr::clearFilesystem(Signal *signal) {
  jam();
  FsRemoveReq *req = (FsRemoveReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = 0;
  req->directory = 1;
  req->ownDirectory = 1;

  const Uint32 DX = CLEAR_DX;
  const Uint32 LCP = CLEAR_DX + CLEAR_LCP;
  const Uint32 DD = CLEAR_DX + CLEAR_LCP + CLEAR_DD;

  if (c_fsRemoveCount < DX) {
    FsOpenReq::setVersion(req->fileNumber, 3);
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);  // Can by any...
    FsOpenReq::v1_setDisk(req->fileNumber, c_fsRemoveCount);
  } else if (c_fsRemoveCount < LCP) {
    FsOpenReq::setVersion(req->fileNumber, 5);
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
    FsOpenReq::v5_setLcpNo(req->fileNumber, c_fsRemoveCount - CLEAR_DX);
    FsOpenReq::v5_setTableId(req->fileNumber, 0);
    FsOpenReq::v5_setFragmentId(req->fileNumber, 0);
  } else if (c_fsRemoveCount < DD) {
    req->ownDirectory = 0;
    FsOpenReq::setVersion(req->fileNumber, 6);
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
    FsOpenReq::v5_setLcpNo(req->fileNumber,
                           FsOpenReq::BP_DD_DF + c_fsRemoveCount - LCP);
  } else {
    ndbabort();
  }

  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, FsRemoveReq::SignalLength,
             JBA);
  c_fsRemoveCount++;
}

void Ndbcntr::execFSREMOVECONF(Signal *signal) {
  jamEntry();
  if (c_fsRemoveCount == CLEAR_DX + CLEAR_LCP + CLEAR_DD) {
    jam();
    if (c_encrypted_filesystem) {
      // After initialising the data node filesystem, a new secrets file is
      // created
      create_secrets_file(signal);
      return;
    }
    sendSttorry(signal);
  } else {
    jam();
    ndbrequire(c_fsRemoveCount < CLEAR_DX + CLEAR_LCP + CLEAR_DD);
    clearFilesystem(signal);
  }
}

void Ndbcntr::Missra::execSTART_ORD(Signal *signal) {
  signal->theData[0] = NDB_LE_NDBStartStarted;
  signal->theData[1] = NDB_VERSION;
  signal->theData[2] = NDB_MYSQL_VERSION_D;
  cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

  currentBlockIndex = 0;
  sendNextREAD_CONFIG_REQ(signal);
}

void Ndbcntr::Missra::sendNextREAD_CONFIG_REQ(Signal *signal) {
  if (currentBlockIndex < ALL_BLOCKS_SZ) {
    if (globalData.getBlock(refToBlock(readConfigOrder[currentBlockIndex])) ==
        nullptr) {
      /**
       * The block isn't used in this data node, skip to next.
       * This can happen for Query thread blocks when no query threads
       * or recover threads are configured.
       */
      currentBlockIndex++;
      sendNextREAD_CONFIG_REQ(signal);
      return;
    }

    jam();

    ReadConfigReq *req = (ReadConfigReq *)signal->getDataPtrSend();
    req->senderData = 0;
    req->senderRef = cntr.reference();
    req->noOfParameters = 0;

    const BlockReference ref = readConfigOrder[currentBlockIndex];

#ifdef DEBUG_RSS
    {
      ndb_rusage ru;
      if (Ndb_GetRUsage(&ru, true) != 0) {
        g_eventLogger->error("Failed to get rusage");
      } else {
        g_eventLogger->info("NDBCNTR : RSS : %llu kB", ru.ru_rss);
      }
    }
#endif
    g_eventLogger->info("Sending READ_CONFIG_REQ to index = %d, name = %s",
                        currentBlockIndex, getBlockName(refToBlock(ref)));

    /**
     * send delayed so that alloc gets "time-sliced"
     */
    cntr.sendSignalWithDelay(ref, GSN_READ_CONFIG_REQ, signal, 1,
                             ReadConfigReq::SignalLength);
    return;
  }

  g_eventLogger->info(
      "READ_CONFIG_REQ phase completed, this phase is"
      " used to read configuration and to calculate"
      " various sizes and allocate almost all memory"
      " needed by the data node in its lifetime");
  /**
   * Finished...
   */
  currentStartPhase = 0;
  currentBlockIndex = 0;
  sendNextSTTOR(signal);
}

void Ndbcntr::Missra::execREAD_CONFIG_CONF(Signal *signal) {
  const ReadConfigConf *conf = (ReadConfigConf *)signal->getDataPtr();

  const Uint32 ref = conf->senderRef;
  ndbrequire(refToBlock(readConfigOrder[currentBlockIndex]) == refToBlock(ref));

  currentBlockIndex++;
  sendNextREAD_CONFIG_REQ(signal);
}

void Ndbcntr::Missra::execSTTORRY(Signal *signal) {
  const BlockReference ref = signal->senderBlockRef();
  ndbrequire(refToBlock(ref) == refToBlock(ALL_BLOCKS[currentBlockIndex].Ref));

  /**
   * Update next start phase
   */
  for (Uint32 i = 3; i < 25; i++) {
    jam();
    if (signal->theData[i] > currentStartPhase) {
      jam();
      ALL_BLOCKS[currentBlockIndex].NextSP = signal->theData[i];
      break;
    }
  }

  currentBlockIndex++;
#ifdef ERROR_INSERT
  if (cntr.cerrorInsert == 1029) {
    signal->theData[0] = ZBLOCK_STTOR;
    g_eventLogger->info(
        "NdbCntrMain stalling Next STTOR on phase %u blockIndex %u",
        currentStartPhase, currentBlockIndex);
    cntr.sendSignalWithDelay(cntr.reference(), GSN_CONTINUEB, signal, 100, 1);
    return;
  }
#endif
  sendNextSTTOR(signal);
}

void Ndbcntr::Missra::sendNextSTTOR(Signal *signal) {
  if (currentBlockIndex < ALL_BLOCKS_SZ &&
      globalData.getBlock(refToBlock(ALL_BLOCKS[currentBlockIndex].Ref)) ==
          nullptr) {
    currentBlockIndex++;
    sendNextSTTOR(signal);
    return;
  }
  for (; currentStartPhase < 255;
       currentStartPhase++, g_currentStartPhase = currentStartPhase) {
    jam();

#ifdef ERROR_INSERT
    if (cntr.cerrorInsert == 1002 &&
        cntr.c_error_insert_extra == currentStartPhase) {
      signal->theData[0] = ZBLOCK_STTOR;
      cntr.sendSignalWithDelay(cntr.reference(), GSN_CONTINUEB, signal, 100, 1);
      return;
    }
#endif

    const Uint32 start = currentBlockIndex;
    for (; currentBlockIndex < ALL_BLOCKS_SZ; currentBlockIndex++) {
      jam();
      if (ALL_BLOCKS[currentBlockIndex].NextSP == currentStartPhase) {
        jam();
        signal->theData[0] = 0;
        signal->theData[1] = currentStartPhase;
        signal->theData[2] = 0;
        signal->theData[3] = 0;
        signal->theData[4] = 0;
        signal->theData[5] = 0;
        signal->theData[6] = 0;
        signal->theData[7] = cntr.ctypeOfStart;

        const BlockReference ref = ALL_BLOCKS[currentBlockIndex].Ref;

#ifdef MAX_STARTPHASE
        ndbrequire(currentStartPhase <= MAX_STARTPHASE);
#endif

#ifdef TRACE_STTOR
        g_eventLogger->info("sending STTOR(%d) to %s(ref=%x index=%d)",
                            currentStartPhase, getBlockName(refToBlock(ref)),
                            ref, currentBlockIndex);
#endif
        if (refToBlock(ref) == DBDIH) signal->theData[7] = cntr.cdihStartType;

        cntr.sendSignal(ref, GSN_STTOR, signal, 8, JBB);

        return;
      }
    }

    currentBlockIndex = 0;

    NodeState newState(NodeState::SL_STARTING, currentStartPhase,
                       (NodeState::StartType)cntr.ctypeOfStart);
    cntr.updateNodeState(signal, newState);

    if (start != 0) {
      /**
       * At least one wanted this start phase, record & report it
       */
      jam();
      g_eventLogger->info("Start phase %u completed", currentStartPhase);
      switch (currentStartPhase) {
        case 0:
          g_eventLogger->info(
              "Phase 0 has made some file system"
              " initialisations");
          break;
        case 1:
          g_eventLogger->info(
              "Phase 1 initialised some variables and"
              " included node in cluster, locked memory"
              " if configured to do so");
          break;
        case 2:
          switch (cntr.ctypeOfStart) {
            case NodeState::ST_INITIAL_START:
            case NodeState::ST_INITIAL_NODE_RESTART:
              g_eventLogger->info(
                  "Phase 2 did more initialisations, master"
                  " accepted our start, we initialised the"
                  " REDO log");
              break;
            case NodeState::ST_SYSTEM_RESTART:
            case NodeState::ST_NODE_RESTART:
              g_eventLogger->info(
                  "Phase 2 did more initialisations, master"
                  " accepted our start, we started REDO log"
                  " initialisations");
              break;
            default:
              break;
          }
          break;
        case 3:
          switch (cntr.ctypeOfStart) {
            case NodeState::ST_INITIAL_START:
            case NodeState::ST_SYSTEM_RESTART:
              g_eventLogger->info("Phase 3 performed local connection setups");
              break;
            case NodeState::ST_INITIAL_NODE_RESTART:
            case NodeState::ST_NODE_RESTART:
              g_eventLogger->info(
                  "Phase 3 locked the data dictionary, "
                  "performed local connection setups, we "
                  " asked for permission to start our node");
              break;
            default:
              break;
          }
          break;
        case 4:
          switch (cntr.ctypeOfStart) {
            case NodeState::ST_SYSTEM_RESTART:
              g_eventLogger->info(
                  "Phase 4 restored all fragments from local"
                  " disk up to a consistent global checkpoint"
                  " id");
              break;
            case NodeState::ST_NODE_RESTART:
            case NodeState::ST_INITIAL_START:
            case NodeState::ST_INITIAL_NODE_RESTART:
              g_eventLogger->info(
                  "Phase 4 continued preparations of the REDO"
                  " log");
              break;
            default:
              break;
          }
          break;
        case 5:
          switch (cntr.ctypeOfStart) {
            case NodeState::ST_INITIAL_NODE_RESTART:
            case NodeState::ST_NODE_RESTART:
              g_eventLogger->info(
                  "Phase 5 restored local fragments in its"
                  " first NDB phase, then copied metadata to"
                  " our node, and"
                  " then actual data was copied over to our"
                  " node, and finally we waited for a local"
                  " checkpoint to complete");
              break;
            case NodeState::ST_INITIAL_START:
              g_eventLogger->info("Phase 5 Created the System Table");
              [[fallthrough]];
            case NodeState::ST_SYSTEM_RESTART:
              g_eventLogger->info(
                  "Phase 5 waited for local checkpoint to"
                  " complete");
              break;
            default:
              break;
          }
          break;
        case 6:
          g_eventLogger->info(
              "Phase 6 updated blocks about that we've now"
              " reached the started state.");
          break;
        case 7:
          g_eventLogger->info(
              "Phase 7 mainly activated the asynchronous"
              " change events process, and some other"
              " background processes");
          break;
        case 8:
          switch (cntr.ctypeOfStart) {
            case NodeState::ST_INITIAL_START:
            case NodeState::ST_SYSTEM_RESTART: {
              g_eventLogger->info(
                  "Phase 8 enabled foreign keys and waited for"
                  "all nodes to complete start up to this point");
              break;
            }
            default:
              break;
          }
          break;
        case 9:
          g_eventLogger->info("Phase 9 enabled APIs to start connecting");
          break;
        case 101:
          g_eventLogger->info(
              "Phase 101 was used by SUMA to take over"
              " responsibility for sending some of the"
              " asynchronous change events");
          break;
        default:
          break;
      }

      signal->theData[0] = NDB_LE_StartPhaseCompleted;
      signal->theData[1] = currentStartPhase;
      signal->theData[2] = cntr.ctypeOfStart;
      cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

      /**
       * Check if we should wait before proceeding with
       *   next startphase
       *
       * New code guarantees that before starting X
       *   that all other nodes (in system restart/initial start)
       *   want to start a startphase >= X
       */
      if (cntr.wait_sp(signal, currentStartPhase + 1)) {
        jam();
        currentStartPhase++;
        g_currentStartPhase = currentStartPhase;
        return;
      }
    }
  }
#ifdef DEBUG_RSS
  {
    ndb_rusage ru;
    if (Ndb_GetRUsage(&ru, true) != 0) {
      g_eventLogger->error("Failed to get rusage");
    } else {
      g_eventLogger->info("NDBCNTR : RSS : %llu kB", ru.ru_rss);
    }
  }
#endif

  g_eventLogger->info("Node started");

  signal->theData[0] = NDB_LE_NDBStartCompleted;
  signal->theData[1] = NDB_VERSION;
  signal->theData[2] = NDB_MYSQL_VERSION_D;
  cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

  NodeState newState(NodeState::SL_STARTED);
  cntr.updateNodeState(signal, newState);
  cntr.send_node_started_rep(signal);

  NodeReceiverGroup rg(NDBCNTR, cntr.c_clusterNodes);
  signal->theData[0] = cntr.getOwnNodeId();
  cntr.sendSignal(rg, GSN_CNTR_START_REP, signal, 1, JBB);
}

void Ndbcntr::send_node_started_rep(Signal *signal) {
  signal->theData[0] = getOwnNodeId();
  sendSignal(QMGR_REF, GSN_NODE_STARTED_REP, signal, 1, JBB);
}

void Ndbcntr::execCREATE_NODEGROUP_IMPL_REQ(Signal *signal) {
  jamEntry();

  CreateNodegroupImplReq reqCopy =
      *(CreateNodegroupImplReq *)signal->getDataPtr();
  CreateNodegroupImplReq *req = &reqCopy;

  if (req->requestType == CreateNodegroupImplReq::RT_COMMIT) {
    jam();
    Uint32 save = c_nodeGroup;
    getNodeGroup(signal);
    if (save != c_nodeGroup) {
      jam();
      updateNodeState(signal, getNodeState());
    }
  }

  {
    CreateNodegroupImplConf *conf =
        (CreateNodegroupImplConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req->senderData;
    sendSignal(req->senderRef, GSN_CREATE_NODEGROUP_IMPL_CONF, signal,
               CreateNodegroupImplConf::SignalLength, JBB);
  }
}

void Ndbcntr::execDROP_NODEGROUP_IMPL_REQ(Signal *signal) {
  jamEntry();

  DropNodegroupImplReq reqCopy = *(DropNodegroupImplReq *)signal->getDataPtr();
  DropNodegroupImplReq *req = &reqCopy;

  if (req->requestType == DropNodegroupImplReq::RT_COMPLETE) {
    jam();
    Uint32 save = c_nodeGroup;
    getNodeGroup(signal);

    if (save != c_nodeGroup) {
      jam();
      updateNodeState(signal, getNodeState());
    }
  }

  {
    DropNodegroupImplConf *conf =
        (DropNodegroupImplConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req->senderData;
    sendSignal(req->senderRef, GSN_DROP_NODEGROUP_IMPL_CONF, signal,
               DropNodegroupImplConf::SignalLength, JBB);
  }
}

void Ndbcntr::send_restorable_gci_rep_to_backup(Signal *signal, Uint32 gci) {
  /**
   * During system restart we don't perform any GCP operations.
   * So in order to ensure that LCP files can be deleted up to
   * the restorable GCI we need to initialise the BACKUP blocks
   * the restorable GCI.
   *
   * Without this signal we are not able to perform deletes and
   * worse the entire system restart could hang due to a deletion
   * of a table during system restart.
   *
   * This method is also used at every write of the local sysfile
   * to inform the backup block about the new restorable GCI.
   */
  Uint32 ldm_workers =
      globalData.ndbMtLqhWorkers == 0 ? 1 : globalData.ndbMtLqhWorkers;
  signal->theData[0] = gci;
  if (isNdbMtLqh()) {
    jam();
    for (Uint32 i = 1; i <= ldm_workers; i++) {
      jam();
      BlockReference ref = numberToRef(BACKUP, i, getOwnNodeId());
      sendSignal(ref, GSN_RESTORABLE_GCI_REP, signal, 1, JBB);
    }
  } else {
    jam();
    sendSignal(BACKUP_REF, GSN_RESTORABLE_GCI_REP, signal, 1, JBB);
  }
}
#define ZVAR_SECRETSFILE_BAT_INDEX 0
#define ZVAR_LOCAL_SYSFILE_BAT_INDEX 1

void Ndbcntr::alloc_local_bat() {
  NewVARIABLE *bat = allocateBat(2);
  bat[ZVAR_SECRETSFILE_BAT_INDEX].WA = &c_secretsfile.m_data[0];
  bat[ZVAR_SECRETSFILE_BAT_INDEX].nrr = sizeof(c_secretsfile.m_data);
  bat[ZVAR_SECRETSFILE_BAT_INDEX].ClusterSize = sizeof(c_secretsfile.m_data);
  bat[ZVAR_SECRETSFILE_BAT_INDEX].bits.q = 7; /* 128 words 2^7 = 128 */
  bat[ZVAR_SECRETSFILE_BAT_INDEX].bits.v = 5; /* Word size 32 bits, 2^5 = 32 */
  bat[ZVAR_LOCAL_SYSFILE_BAT_INDEX].WA = &c_local_sysfile.m_data[0];
  bat[ZVAR_LOCAL_SYSFILE_BAT_INDEX].nrr = 1;
  bat[ZVAR_LOCAL_SYSFILE_BAT_INDEX].ClusterSize =
      sizeof(c_local_sysfile.m_data);
  bat[ZVAR_LOCAL_SYSFILE_BAT_INDEX].bits.q = 7; /* 128 words 2^7 = 128 */
  bat[ZVAR_LOCAL_SYSFILE_BAT_INDEX].bits.v =
      5; /* Word size 32 bits, 2^5 = 32 */
}

void Ndbcntr::init_secretsfile() {
  jam();
  c_secretsfile.m_sender_ref = 0;
  c_secretsfile.m_state = SecretsFileOperationRecord::NOT_USED;
  std::memset(&c_secretsfile.m_data[0], 0, sizeof(c_secretsfile.m_data));
}

void Ndbcntr::init_secretsfile_vars() {
  jam();
  c_secretsfile.m_sender_ref = 0;
  c_secretsfile.m_state = SecretsFileOperationRecord::NOT_USED;
}

void Ndbcntr::init_local_sysfile() {
  c_local_sysfile.m_sender_ref = 0;
  c_local_sysfile.m_state = LocalSysfile::NOT_USED;
  c_local_sysfile.m_initial_read_done = false;
  c_local_sysfile.m_last_write_done = false;
  c_local_sysfile.m_initial_write_local_sysfile_ongoing = false;
  std::memset(&c_local_sysfile.m_data[0], 0, sizeof(c_local_sysfile.m_data));
}

void Ndbcntr::init_local_sysfile_vars() {
  c_local_sysfile.m_sender_ref = 0;
  c_local_sysfile.m_state = LocalSysfile::NOT_USED;
}

/**
 * This signal is sent from various blocks during early phases of
 * the restart. Only the first reader needs to actually read the
 * files, the others read the local variables.
 */
void Ndbcntr::execREAD_LOCAL_SYSFILE_REQ(Signal *signal) {
  jamEntry();
  ReadLocalSysfileReq *req = (ReadLocalSysfileReq *)signal->getDataPtr();

  if (c_local_sysfile.m_initial_read_done) {
    jam();
    sendReadLocalSysfileConf(signal, req->userReference, req->userPointer);
    return;
  }
  if (c_local_sysfile.m_sender_ref != 0) {
    /**
     * Someone is already busy reading the local sysfile
     * and haven't completed yet, we will simply wait until
     * this read is done and then we will get the data
     * immediately.
     *
     * This is normal since all LQH blocks asks for the
     * same thing at the same time.
     */
    jam();
    ndbabort();
    sendSignalWithDelay(reference(), GSN_READ_LOCAL_SYSFILE_REQ, signal, 10,
                        ReadLocalSysfileReq::SignalLength);
    return;
  }
  c_local_sysfile.m_sender_data = req->userPointer;
  c_local_sysfile.m_sender_ref = req->userReference;
  ndbrequire(c_local_sysfile.m_state == LocalSysfile::NOT_USED);
  c_local_sysfile.m_state = LocalSysfile::OPEN_READ_FILE_0;

  /**
   * Initialise data for response when no local sysfile is around.
   * In this case we report that the node is restorable. The rest
   * of the data is then of no specific value.
   *
   * This should never happen other than in an upgrade. Given that
   * upgrade from non-local sysfile version to versions containing
   * local sysfile requires initial restarts this should only happen
   * in non-supported cases and in this case we cannot remove the
   * file system just like that.
   */
  c_local_sysfile.m_restorable_flag =
      ReadLocalSysfileReq::NODE_RESTORABLE_ON_ITS_OWN;
  c_local_sysfile.m_max_restorable_gci = 1;

  open_local_sysfile(signal, 0, true);
}

/**
 * This is written at the following occasions:
 *
 * 1) Early phase writing that node requires initial restart
 *    This is used for all types of initial restarts. Cannot
 *    happen in parallel with other blocks. So no queueing
 *    can happen here.
 *    Sent by NDBCNTR
 *
 * 2) Late phase (phase = 50) indicating that node is now
 *    restorable on its own. This signal can come in
 *    parallel with a signal from 4).
 *    Sent by DBLQH
 *
 * 3) Before starting a local LCP we need to set state to
 *    not restorable on its own with a proper GCI.
 *    Can happen in parallel with 4) below.
 *    Sent by NDBCNTR
 *
 * 4) At each GCP_SAVEREQ after starting a local LCP we
 *    need to send a new write with a new GCP that we
 *    have seen completed.
 *    Sent by DBLQH
 *
 * 5) Before activating REDO logs we will write that we
 *    start an LCP since the first distributed LCP will
 *    make us unable to restart on our own. This can
 *    also happen in parallel with 4).
 */
void Ndbcntr::execWRITE_LOCAL_SYSFILE_REQ(Signal *signal) {
  jamEntry();
  WriteLocalSysfileReq req = *(WriteLocalSysfileReq *)signal->getDataPtr();

  ndbrequire(c_local_sysfile.m_initial_read_done);
  ndbrequire(!c_local_sysfile.m_last_write_done);
  if (c_local_sysfile.m_sender_ref != 0) {
    /**
     * 3) and 4) above happened concurrently, let one of them simply
     * wait, should be a very rare event.
     */
    sendSignalWithDelay(reference(), GSN_WRITE_LOCAL_SYSFILE_REQ, signal, 10,
                        WriteLocalSysfileReq::SignalLength);
    return;
  }
  ndbrequire(c_local_sysfile.m_state == LocalSysfile::NOT_USED);

  c_local_sysfile.m_sender_data = req.userPointer;
  c_local_sysfile.m_sender_ref = req.userReference;

  if ((c_local_sysfile.m_restorable_flag !=
       ReadLocalSysfileReq::NODE_REQUIRE_INITIAL_RESTART) ||
      (req.nodeRestorableOnItsOwn !=
       ReadLocalSysfileReq::NODE_NOT_RESTORABLE_ON_ITS_OWN)) {
    jam();
    c_local_sysfile.m_restorable_flag = req.nodeRestorableOnItsOwn;
  } else {
    jam();
    /**
     * When we want to say that node is not restorable on its own and
     * it was previously set to requiring initial restart we will keep
     * it set to requiring initial restart. This flag is only removed
     * by setting the flag to node restorable on its own.
     */
  }
  if (req.lastWrite == 1) {
    jam();
    c_local_sysfile.m_last_write_done = true;
  }
  c_local_sysfile.m_max_restorable_gci = req.maxGCIRestorable;

  c_local_sysfile.m_data[0] = NDBD_LOCAL_SYSFILE_VERSION;
  c_local_sysfile.m_data[1] = c_local_sysfile.m_restorable_flag;
  c_local_sysfile.m_data[2] = c_local_sysfile.m_max_restorable_gci;

  c_local_sysfile.m_state = LocalSysfile::OPEN_WRITE_FILE_0;
  open_local_sysfile(signal, 0, false);
}

void Ndbcntr::create_secrets_file(Signal *signal) {
  // Generate node master key
  require(0 == ndb_openssl_evp::generate_key(globalData.nodeMasterKey,
                                             c_nodeMasterKeyLength));
  globalData.nodeMasterKeyLength = c_nodeMasterKeyLength;
  // Open secrets file for writing
  open_secretsfile(signal, 0, false, false);
}

void Ndbcntr::open_secretsfile(Signal *signal, Uint32 secretsfile_num,
                               bool open_for_read, bool check_file_exists) {
  FsOpenReq *req = (FsOpenReq *)signal->getDataPtr();
  req->userReference = reference();
  req->userPointer = SecretsFileOperationRecord::FILE_ID;

  FsOpenReq::setVersion(req->fileNumber, 1);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_SYSFILE);
  FsOpenReq::v1_setDisk(req->fileNumber, 1);
  FsOpenReq::v1_setTable(req->fileNumber, -1);
  FsOpenReq::v1_setFragment(req->fileNumber, -1);
  FsOpenReq::v1_setS(req->fileNumber, 0);
  FsOpenReq::v1_setP(req->fileNumber, -1);

  jam();
  ndbrequire(c_secretsfile.m_state == SecretsFileOperationRecord::NOT_USED ||
             c_secretsfile.m_state == SecretsFileOperationRecord::WAITING);

  if (check_file_exists) {
    c_secretsfile.m_state = SecretsFileOperationRecord::CHECK_MISSING_0;
    jam();
    req->fileFlags = FsOpenReq::OM_READONLY;
    req->page_size = 0;
    req->file_size_hi = UINT32_MAX;
    req->file_size_lo = UINT32_MAX;
    req->auto_sync_size = 0;
    sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
    return;
  }

  if (open_for_read) {
    jam();
    req->file_size_hi = UINT32_MAX;
    req->file_size_lo = UINT32_MAX;
    req->fileFlags = FsOpenReq::OM_READONLY | FsOpenReq::OM_READ_FORWARD;
    c_secretsfile.m_state = SecretsFileOperationRecord::OPEN_READ_FILE_0;
  } else {
    jam();
    req->fileFlags = FsOpenReq::OM_WRITEONLY | FsOpenReq::OM_CREATE_IF_NONE |
                     FsOpenReq::OM_APPEND | FsOpenReq::OM_SIZE_ESTIMATED;
    req->file_size_hi = 0;
    req->file_size_lo = sizeof(c_secretsfile.m_data);
    c_secretsfile.m_state = SecretsFileOperationRecord::OPEN_WRITE_FILE_0;
  }

  req->fileFlags |= FsOpenReq::OM_ENCRYPT_XTS;
  req->fileFlags |= FsOpenReq::OM_ENCRYPT_PASSWORD;

  LinearSectionPtr lsptr[3];
  ndbrequire(FsOpenReq::getVersion(req->fileNumber) != 4);
  lsptr[FsOpenReq::FILENAME].p = nullptr;
  lsptr[FsOpenReq::FILENAME].sz = 0;

  EncryptionKeyMaterial ekm;
  ekm.length = globalData.filesystemPasswordLength;
  memcpy(&ekm.data, globalData.filesystemPassword,
         globalData.filesystemPasswordLength);
  ekm.data[globalData.filesystemPasswordLength] = 0;

  lsptr[FsOpenReq::ENCRYPT_KEY_MATERIAL].p = (const Uint32 *)&ekm;
  lsptr[FsOpenReq::ENCRYPT_KEY_MATERIAL].sz = ekm.get_needed_words();

  if (ERROR_INSERTED(1026)) {
    static constexpr EncryptionKeyMaterial WRONG_PWD = {8, "WRONGPWD"};
    lsptr[FsOpenReq::ENCRYPT_KEY_MATERIAL].p = (const Uint32 *)&WRONG_PWD;
    lsptr[FsOpenReq::ENCRYPT_KEY_MATERIAL].sz = WRONG_PWD.get_needed_words();
  }

  req->page_size = 0;
  req->auto_sync_size = 0;
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA,
             lsptr, 2);
}

void Ndbcntr::open_local_sysfile(Signal *signal, Uint32 sysfile_num,
                                 bool open_for_read) {
  Uint32 fileName[4];
  fileName[0] = Uint32(-1);                       /* T Directory not used */
  fileName[1] = Uint32(-1);                       /* F Directory not used */
  fileName[2] = Uint32(-1);                       /* S Directory not used */
  Uint32 fileType = 1;                            /* File name version 1 */
  fileType = (fileType << 8) + 6;                 /* .sysfile */
  fileType = (fileType << 8) + (sysfile_num + 1); /* D1 = 1, D2 = 2 */
  fileType = (fileType << 8) + 0;                 /* P0 file name */
  /* The name becomes D1/NDBCNTR/P0.sysfile or D2/NDBCNTR/P0.sysfile */
  fileName[3] = fileType;

  FsOpenReq *req = (FsOpenReq *)signal->getDataPtr();
  req->userReference = reference();
  req->userPointer = LocalSysfile::FILE_ID;
  req->fileNumber[0] = fileName[0];
  req->fileNumber[1] = fileName[1];
  req->fileNumber[2] = fileName[2];
  req->fileNumber[3] = fileName[3];
  if (open_for_read) {
    jam();
    req->fileFlags = FsOpenReq::OM_READONLY;
  } else {
    jam();
    req->fileFlags =
        FsOpenReq::OM_READWRITE | FsOpenReq::OM_CREATE | FsOpenReq::OM_TRUNCATE;
  }
  req->page_size = 0;
  req->file_size_hi = UINT32_MAX;
  req->file_size_lo = UINT32_MAX;
  req->auto_sync_size = 0;
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void Ndbcntr::execFSOPENREF(Signal *signal) {
  jamEntry();
  FsRef *ref = (FsRef *)signal->getDataPtr();
  jam();
  jamLine(ref->errorCode);
  jamLine(ref->osErrorCode);

  if (ref->userPointer == SecretsFileOperationRecord::FILE_ID) {
    if (c_secretsfile.m_state == SecretsFileOperationRecord::CHECK_MISSING_0) {
      jam();
      c_secretsfile.m_state = SecretsFileOperationRecord::NOT_USED;

      // Secrets file not found.
      // Expected case, no secrets file was expected, and none was found.
      if (ref->errorCode == FsRef::fsErrFileDoesNotExist) {
        // trigger read sysfile
        sendReadLocalSysfile(signal);
        return;
      }
      // Secrets file found
      g_eventLogger->error(
          "File system encrypted but EncryptedFileSystem "
          "option not set in data node configuration");
      CRASH_INSERTION(1025);
      progError(__LINE__, NDBD_EXIT_FS_ENCRYPTION_REQUIRED);
    } else if (c_secretsfile.m_state ==
               SecretsFileOperationRecord::OPEN_WRITE_FILE_0) {
      jam();
      g_eventLogger->error(
          "Failed to open secrets file for writing, "
          "error: %d",
          ref->errorCode);
      progError(__LINE__, NDBD_EXIT_INVALID_SECRETS_FILE);

    } else if (c_secretsfile.m_state ==
               SecretsFileOperationRecord::OPEN_READ_FILE_0) {
      jam();
      if (ref->errorCode == FsRef::fsErrFileDoesNotExist) {
        /**
         * Failed to open the secrets file for reading.
         * It is not an initial restart and encrypted filesystem is ON.
         * If filesystem is 'empty' we can also write the secrets file as in
         * an initial restart otherwise node will fail to start.
         *
         * Starts reading the local sysfile.
         * If it succeeded or failed with something else than
         * FsRef::fsErrFileDoesNotExist data node start will be aborted.
         */
        c_secretsfile.m_state = SecretsFileOperationRecord::WAITING;
        sendReadLocalSysfile(signal);
        return;
      }
      g_eventLogger->error(
          "Failed to open secrets file for reading, "
          "error: %d",
          ref->errorCode);
      progError(__LINE__, NDBD_EXIT_INVALID_SECRETS_FILE);
    }
    jamLine(c_secretsfile.m_state);
    ndbabort();
  }

  if (ref->userPointer == LocalSysfile::FILE_ID) {
    if (c_local_sysfile.m_state == LocalSysfile::OPEN_READ_FILE_0) {
      jam();

      if (c_secretsfile.m_state == SecretsFileOperationRecord::WAITING &&
          ref->errorCode != FsRef::fsErrFileDoesNotExist) {
        /**
         * We are trying to open the sysfile #0 just to check for "empty" FS
         * The secrets file is missing, so if the sysfile is found the
         * FS is not empty and the node start will be aborted.
         */
        g_eventLogger->error("Secrets file is missing from file system");
        progError(__LINE__, NDBD_EXIT_MISSING_SECRETS_FILE);
      }

      c_local_sysfile.m_state = LocalSysfile::OPEN_READ_FILE_1;
      open_local_sysfile(signal, 1, true);
      return;
    } else if (c_local_sysfile.m_state == LocalSysfile::OPEN_READ_FILE_1) {
      jam();

      if (c_secretsfile.m_state == SecretsFileOperationRecord::WAITING &&
          ref->errorCode != FsRef::fsErrFileDoesNotExist) {
        /**
         * We are trying to open the sysfile #1 just to check for "empty" FS
         * The secrets file is missing, so if the sysfile is found the
         * FS is not empty and the node start will be aborted.
         */
        g_eventLogger->error("Secrets file is missing from file system");
        progError(__LINE__, NDBD_EXIT_MISSING_SECRETS_FILE);
      }

      // Both sysfile failed with fsErrFileDoesNotExist, FS is empty
      // We can now create the secrets file.
      if (c_secretsfile.m_state == SecretsFileOperationRecord::WAITING &&
          ref->errorCode == FsRef::fsErrFileDoesNotExist) {
        create_secrets_file(signal);
        return;
      }
      sendReadLocalSysfileConf(signal, c_local_sysfile.m_sender_ref,
                               c_local_sysfile.m_sender_data);
      return;
    }
    jamLine(c_local_sysfile.m_state);
    ndbabort();
  }
  ndbabort();
}

void Ndbcntr::execFSOPENCONF(Signal *signal) {
  jamEntry();
  FsConf *conf = (FsConf *)signal->getDataPtr();

  if (conf->userPointer == SecretsFileOperationRecord::FILE_ID) {
    if (c_secretsfile.m_state == SecretsFileOperationRecord::CHECK_MISSING_0) {
      jam();
      g_eventLogger->error(
          "File system encrypted but EncryptedFileSystem "
          "option not set in data node configuration.");
      CRASH_INSERTION(1025);
      progError(__LINE__, NDBD_EXIT_FS_ENCRYPTION_REQUIRED);
    } else if (c_secretsfile.m_state ==
               SecretsFileOperationRecord::OPEN_WRITE_FILE_0) {
      jam();
      c_secretsfile.m_file_pointer = conf->filePointer;
      write_secretsfile(signal);
      return;
    } else if (c_secretsfile.m_state ==
               SecretsFileOperationRecord::OPEN_READ_FILE_0) {
      jam();
      c_secretsfile.m_file_pointer = conf->filePointer;
      read_secretsfile(signal);
      return;
    }
    jamLine(c_secretsfile.m_state);
    ndbabort();
  }

  if (conf->userPointer == LocalSysfile::FILE_ID) {
    if (c_local_sysfile.m_state == LocalSysfile::OPEN_READ_FILE_0 ||
        c_local_sysfile.m_state == LocalSysfile::OPEN_READ_FILE_1) {
      /**
       * We are trying to open the sysfile just to check for "empty" FS
       * The secrets file is missing, so since the sysfile open succeed the
       * FS is not empty and node start will be aborted.
       */
      if (c_secretsfile.m_state == SecretsFileOperationRecord::WAITING) {
        g_eventLogger->error("Secrets file is missing from file system");
        progError(__LINE__, NDBD_EXIT_MISSING_SECRETS_FILE);
      }
      if (c_local_sysfile.m_state == LocalSysfile::OPEN_READ_FILE_0) {
        jam();
        c_local_sysfile.m_state = LocalSysfile::READ_FILE_0;
      } else {
        jam();
        c_local_sysfile.m_state = LocalSysfile::READ_FILE_1;
      }
      c_local_sysfile.m_file_pointer = conf->filePointer;
      read_local_sysfile(signal);
      return;
    } else if (c_local_sysfile.m_state == LocalSysfile::OPEN_WRITE_FILE_0 ||
               c_local_sysfile.m_state == LocalSysfile::OPEN_WRITE_FILE_1) {
      if (c_local_sysfile.m_state == LocalSysfile::OPEN_WRITE_FILE_0) {
        jam();
        c_local_sysfile.m_state = LocalSysfile::WRITE_FILE_0;
      } else {
        jam();
        c_local_sysfile.m_state = LocalSysfile::WRITE_FILE_1;
      }
      c_local_sysfile.m_file_pointer = conf->filePointer;
      write_local_sysfile(signal);
      return;
    }
    jamLine(c_secretsfile.m_state);
    ndbabort();
  }
  ndbabort();
}

void Ndbcntr::read_secretsfile(Signal *signal) {
  jam();
  ndbrequire(c_secretsfile.m_state ==
             SecretsFileOperationRecord::OPEN_READ_FILE_0);
  c_secretsfile.m_state = SecretsFileOperationRecord::READ_FILE_0;

  FsReadWriteReq *req = (FsReadWriteReq *)signal->getDataPtr();
  req->filePointer = c_secretsfile.m_file_pointer;
  req->userReference = reference();
  req->userPointer = SecretsFileOperationRecord::FILE_ID;
  req->operationFlag = 0;
  req->setFormatFlag(req->operationFlag, FsReadWriteReq::fsFormatArrayOfPages);
  req->varIndex = ZVAR_SECRETSFILE_BAT_INDEX;
  req->numberOfPages = 1;
  req->data.arrayOfPages.varIndex = 0;
  req->data.arrayOfPages.fileOffset = 0;
  std::memset(&c_secretsfile.m_data[0], 0, sizeof(c_secretsfile.m_data));
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 8, JBA);
}

void Ndbcntr::write_secretsfile(Signal *signal) {
  jam();
  ndbrequire(c_secretsfile.m_state ==
             SecretsFileOperationRecord::OPEN_WRITE_FILE_0);
  c_secretsfile.m_state = SecretsFileOperationRecord::WRITE_FILE_0;

  Uint32 cnt = 0;
  memcpy(&c_secretsfile.m_data[cnt], "NDBSCRT1", 8);
  cnt += ndb_ceil_div<Uint32>(8, sizeof(Uint32));
  auto key_len_le = globalData.nodeMasterKeyLength;
#ifdef WORDS_BIGENDIAN
  // key length should be stored in little endian
  ndb_ndbxfrm1::toggle_endian32(&key_len_le);
#endif
  memcpy(&c_secretsfile.m_data[cnt], &key_len_le,
         sizeof(globalData.nodeMasterKeyLength));
  cnt += ndb_ceil_div<Uint32>(sizeof(globalData.nodeMasterKeyLength),
                              sizeof(Uint32));

  memcpy(&c_secretsfile.m_data[cnt], globalData.nodeMasterKey,
         globalData.nodeMasterKeyLength);

  FsAppendReq *req = (FsAppendReq *)signal->getDataPtr();
  req->filePointer = c_secretsfile.m_file_pointer;
  req->userReference = reference();
  req->userPointer = SecretsFileOperationRecord::FILE_ID;
  req->varIndex = ZVAR_SECRETSFILE_BAT_INDEX;

  req->offset = 0;
  req->size = sizeof(c_secretsfile.m_data) / 4;
  req->synch_flag = 0;
  sendSignal(NDBFS_REF, GSN_FSAPPENDREQ, signal, FsAppendReq::SignalLength,
             JBA);
}

void Ndbcntr::read_local_sysfile(Signal *signal) {
  FsReadWriteReq *req = (FsReadWriteReq *)signal->getDataPtr();
  req->filePointer = c_local_sysfile.m_file_pointer;
  req->userReference = reference();
  req->userPointer = LocalSysfile::FILE_ID;
  req->operationFlag = 0;
  req->setFormatFlag(req->operationFlag, FsReadWriteReq::fsFormatArrayOfPages);
  req->varIndex = ZVAR_LOCAL_SYSFILE_BAT_INDEX;
  req->numberOfPages = 1;
  req->data.arrayOfPages.varIndex = 0;
  req->data.arrayOfPages.fileOffset = 0;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 8, JBA);
}

void Ndbcntr::write_local_sysfile(Signal *signal) {
  FsReadWriteReq *req = (FsReadWriteReq *)signal->getDataPtr();
  req->filePointer = c_local_sysfile.m_file_pointer;
  req->userReference = reference();
  req->userPointer = LocalSysfile::FILE_ID;
  req->operationFlag = 0;
  req->setFormatFlag(req->operationFlag, FsReadWriteReq::fsFormatArrayOfPages);
  req->varIndex = ZVAR_LOCAL_SYSFILE_BAT_INDEX;
  req->numberOfPages = 1;
  req->data.arrayOfPages.varIndex = 0;
  req->data.arrayOfPages.fileOffset = 0;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 8, JBA);
}

void Ndbcntr::execFSREADREF(Signal *signal) {
  jamEntry();

  FsRef *ref = (FsRef *)signal->getDataPtr();
  if (ref->userPointer == SecretsFileOperationRecord::FILE_ID) {
    if (c_secretsfile.m_state == SecretsFileOperationRecord::READ_FILE_0) {
      jam();
      g_eventLogger->error("Failed to read secrets file, error: %d",
                           ref->errorCode);
      CRASH_INSERTION(1027);
      progError(__LINE__, NDBD_EXIT_INVALID_SECRETS_FILE);
    }
    jamLine(c_secretsfile.m_state);
    ndbabort();
  }
  if (ref->userPointer == LocalSysfile::FILE_ID) {
    if (c_local_sysfile.m_state == LocalSysfile::READ_FILE_0 ||
        c_local_sysfile.m_state == LocalSysfile::READ_FILE_1) {
      jam();
      handle_read_refuse(signal);
    }
    jamLine(c_local_sysfile.m_state);
    ndbabort();
  }
  ndbabort();
}

void Ndbcntr::read_secretsfile_data(Signal *signal) {
  char magic[8];
  memset(globalData.nodeMasterKey, 0, MAX_NODE_MASTER_KEY_LENGTH);
  unsigned char *ptr = (unsigned char *)c_secretsfile.m_data;
  Uint32 cnt = 0;
  memcpy(magic, ptr + cnt, sizeof(magic));

  if (memcmp(magic, "NDBSCRT1", 8) != 0) {
    jam();
    g_eventLogger->error(
        "Failed to read secrets file using the "
        "provided filesystem password (wrong password?)");
    CRASH_INSERTION(1026);
    progError(__LINE__, NDBD_EXIT_WRONG_FILESYSTEM_PASSWORD);
  }

  cnt += sizeof(magic);
  Uint32 key_len;
  memcpy(&key_len, ptr + cnt, sizeof(key_len));
#ifdef WORDS_BIGENDIAN
  // key length is always stored in little endian
  ndb_ndbxfrm1::toggle_endian32(&key_len);
#endif
  assert(key_len == c_nodeMasterKeyLength);
  cnt += sizeof(key_len);
  memset(globalData.nodeMasterKey, 0, MAX_NODE_MASTER_KEY_LENGTH);
  memcpy(globalData.nodeMasterKey, ptr + cnt, key_len);
  globalData.nodeMasterKeyLength = key_len;
}

const char *Ndbcntr::get_restorable_flag_string(Uint32 restorable_flag) {
  switch (restorable_flag) {
    case ReadLocalSysfileReq::NODE_RESTORABLE_ON_ITS_OWN:
      return "Node restorable on its own";
    case ReadLocalSysfileReq::NODE_NOT_RESTORABLE_ON_ITS_OWN:
      return "Node not restorable on its own";
    case ReadLocalSysfileReq::NODE_REQUIRE_INITIAL_RESTART:
      return "Node requires initial start";
    default:
      return "Invalid value of restorable flag in local sysfile";
  }
}

void Ndbcntr::read_local_sysfile_data(Signal *signal) {
  jam();
  const Uint32 version = c_local_sysfile.m_data[0];
  Uint32 node_restorable_flag = c_local_sysfile.m_data[1];
  Uint32 max_restorable_gci = c_local_sysfile.m_data[2];
  c_local_sysfile.m_restorable_flag = node_restorable_flag;
  c_local_sysfile.m_max_restorable_gci = max_restorable_gci;
  g_eventLogger->info(
      "Local sysfile: %s, gci: %u, version: %x",
      get_restorable_flag_string(c_local_sysfile.m_restorable_flag),
      c_local_sysfile.m_max_restorable_gci, version);
}

void Ndbcntr::execFSREADCONF(Signal *signal) {
  jamEntry();
  FsConf *conf = (FsConf *)signal->getDataPtr();

  if (conf->userPointer == SecretsFileOperationRecord::FILE_ID) {
    if (c_secretsfile.m_state == SecretsFileOperationRecord::READ_FILE_0) {
      jam();
      read_secretsfile_data(signal);
      close_secretsfile(signal);
      return;
    }
    jamLine(c_secretsfile.m_state);
    ndbabort();
  }

  if (conf->userPointer == LocalSysfile::FILE_ID) {
    if (c_local_sysfile.m_state == LocalSysfile::READ_FILE_0 ||
        c_local_sysfile.m_state == LocalSysfile::READ_FILE_1) {
      jam();
      read_local_sysfile_data(signal);
      c_local_sysfile.m_state = LocalSysfile::CLOSE_READ_FILE;
      close_local_sysfile(signal);
      return;
    }
    jamLine(c_local_sysfile.m_state);
    ndbabort();
  }
  ndbabort();
}

void Ndbcntr::execFSWRITEREF(Signal *signal) {
  jamEntry();
  jamLine(c_local_sysfile.m_state);
  ndbabort();
}

void Ndbcntr::execFSWRITECONF(Signal *signal) {
  jamEntry();
  FsConf *conf = (FsConf *)signal->getDataPtr();
  if (conf->userPointer == LocalSysfile::FILE_ID) {
    if (c_local_sysfile.m_state == LocalSysfile::WRITE_FILE_0 ||
        c_local_sysfile.m_state == LocalSysfile::WRITE_FILE_1) {
      if (c_local_sysfile.m_state == LocalSysfile::WRITE_FILE_0) {
        jam();
        c_local_sysfile.m_state = LocalSysfile::CLOSE_WRITE_FILE_0;
      } else {
        jam();
        c_local_sysfile.m_state = LocalSysfile::CLOSE_WRITE_FILE_1;
      }
      close_local_sysfile(signal);
      return;
    }
  }
  jamLine(c_local_sysfile.m_state);
  ndbabort();
}

void Ndbcntr::handle_read_refuse(Signal *signal) {
  if (c_local_sysfile.m_state == LocalSysfile::READ_FILE_0) {
    jam();
    c_local_sysfile.m_state = LocalSysfile::CLOSE_READ_REF_0;
  } else if (c_local_sysfile.m_state == LocalSysfile::READ_FILE_1) {
    jam();
    c_local_sysfile.m_state = LocalSysfile::CLOSE_READ_REF_1;
  } else {
    ndbabort();
  }
  close_local_sysfile(signal);
}

void Ndbcntr::execFSAPPENDREF(Signal *signal) {
  jam();
  FsRef *ref = (FsRef *)signal->getDataPtr();
  g_eventLogger->error(
      "Failed to write secrets file, "
      "error: %d",
      ref->errorCode);
  progError(__LINE__, NDBD_EXIT_INVALID_SECRETS_FILE);
}

void Ndbcntr::execFSAPPENDCONF(Signal *signal) {
  jamEntry();
  FsConf *conf = (FsConf *)signal->getDataPtr();
  if (conf->userPointer == SecretsFileOperationRecord::FILE_ID) {
    if (c_secretsfile.m_state == SecretsFileOperationRecord::WRITE_FILE_0) {
      close_secretsfile(signal);
      return;
    }
  }
  jamEntry();
  jamLine(c_secretsfile.m_state);
  ndbabort();
}

void Ndbcntr::close_secretsfile(Signal *signal) {
  jam();
  ndbrequire(c_secretsfile.m_state == SecretsFileOperationRecord::READ_FILE_0 ||
             c_secretsfile.m_state == SecretsFileOperationRecord::WRITE_FILE_0);

  c_secretsfile.m_state =
      (c_secretsfile.m_state == SecretsFileOperationRecord::READ_FILE_0)
          ? SecretsFileOperationRecord::CLOSE_READ_FILE_0
          : SecretsFileOperationRecord::CLOSE_WRITE_FILE_0;

  FsCloseReq *req = (FsCloseReq *)signal->getDataPtr();
  req->filePointer = c_secretsfile.m_file_pointer;
  req->userReference = reference();
  req->userPointer = SecretsFileOperationRecord::FILE_ID;
  req->fileFlag = 0;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 4, JBA);
}

void Ndbcntr::close_local_sysfile(Signal *signal) {
  jam();
  FsCloseReq *req = (FsCloseReq *)signal->getDataPtr();
  req->filePointer = c_local_sysfile.m_file_pointer;
  req->userReference = reference();
  req->userPointer = LocalSysfile::FILE_ID;
  req->fileFlag = 0;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 4, JBA);
}

void Ndbcntr::execFSCLOSEREF(Signal *signal) {
  jamEntry();
  FsRef *ref = (FsRef *)signal->getDataPtr();
  jam();
  jamLine(ref->errorCode);
  jamLine(ref->osErrorCode);

  if (ref->userPointer == SecretsFileOperationRecord::FILE_ID) {
    if (c_secretsfile.m_state ==
        SecretsFileOperationRecord::CLOSE_WRITE_FILE_0) {
      jam();
      g_eventLogger->error("Failed to write secrets file, error: %d",
                           ref->errorCode);
      progError(__LINE__, NDBD_EXIT_INVALID_SECRETS_FILE);
      return;
    } else if (c_secretsfile.m_state ==
               SecretsFileOperationRecord::CLOSE_READ_FILE_0) {
      jam();
      // TODO new error: checksum validation failed
      g_eventLogger->error(
          "Failed to read secrets file using the "
          "provided filesystem password (wrong password?)");
      progError(__LINE__, NDBD_EXIT_WRONG_FILESYSTEM_PASSWORD);
    }
    jamLine(c_secretsfile.m_state);
    ndbabort();
  }
  ndbabort();
}

void Ndbcntr::sendReadLocalSysfileConf(Signal *signal, BlockReference ref,
                                       Uint32 senderData) {
  ReadLocalSysfileConf *conf = (ReadLocalSysfileConf *)signal->getDataPtrSend();
  conf->userPointer = senderData;
  conf->nodeRestorableOnItsOwn = c_local_sysfile.m_restorable_flag;
  conf->maxGCIRestorable = c_local_sysfile.m_max_restorable_gci;

  sendSignal(ref, GSN_READ_LOCAL_SYSFILE_CONF, signal,
             ReadLocalSysfileConf::SignalLength, JBB);
  c_local_sysfile.m_initial_read_done = true;
  init_local_sysfile_vars();
}

void Ndbcntr::sendWriteLocalSysfileConf(Signal *signal) {
  DEB_LOCAL_SYSFILE((
      "LocalSysfile write: m_restorable_flag: %u,"
      " m_max_restorable_gci: %u",
      c_local_sysfile.m_restorable_flag, c_local_sysfile.m_max_restorable_gci));

  WriteLocalSysfileConf *conf =
      (WriteLocalSysfileConf *)signal->getDataPtrSend();
  conf->userPointer = c_local_sysfile.m_sender_data;
  sendSignal(c_local_sysfile.m_sender_ref, GSN_WRITE_LOCAL_SYSFILE_CONF, signal,
             WriteLocalSysfileConf::SignalLength, JBB);
  init_local_sysfile_vars();
  if ((c_local_sysfile.m_restorable_flag ==
       ReadLocalSysfileReq::NODE_NOT_RESTORABLE_ON_ITS_OWN) ||
      (c_local_sysfile.m_restorable_flag ==
       ReadLocalSysfileReq::NODE_REQUIRE_INITIAL_RESTART)) {
    jam();
    /**
     * We have successfully updated the local sysfile with a new
     * restorable GCI. Inform all backup blocks about this new
     * restorable GCI and inform ourselves about it.
     *
     * Only the first LQH instance is sending the update request
     * of the local sysfile request, the remainder will
     * immediately respond to the GCP_SAVEREQ signal. The first
     * instance will wait until the local sysfile have been
     * written. Thus no message will leave this node until the
     * local sysfile have been written informing the DIH master
     * about the outcome of the GCP_SAVEREQ signal.
     */
    Uint32 restorable_gci = c_local_sysfile.m_max_restorable_gci;
    send_restorable_gci_rep_to_backup(signal, restorable_gci);
    if (restorable_gci != 1) {
      jam();
      signal->theData[0] = restorable_gci;
      execRESTORABLE_GCI_REP(signal);
    }
  }

  if (ERROR_INSERTED(1028) && ctypeOfStart == NodeState::ST_SYSTEM_RESTART) {
    jam();
    CRASH_INSERTION(1028);
  }
}

void Ndbcntr::execFSCLOSECONF(Signal *signal) {
  jamEntry();
  FsConf *conf = (FsConf *)signal->getDataPtr();
  if (conf->userPointer == SecretsFileOperationRecord::FILE_ID) {
    if (c_secretsfile.m_state ==
        SecretsFileOperationRecord::CLOSE_WRITE_FILE_0) {
      jam();
      // Secrets File Operation Record no longer needed
      c_secretsfile.m_state = SecretsFileOperationRecord::NOT_USED;

      // If we are using the sysfile to check for empty FS and then create
      // secrets file, continue sysfile handling to its original state
      if (c_local_sysfile.m_state == LocalSysfile::OPEN_READ_FILE_1) {
        sendReadLocalSysfileConf(signal, c_local_sysfile.m_sender_ref,
                                 c_local_sysfile.m_sender_data);
        return;
      }
      c_local_sysfile.m_state = LocalSysfile::NOT_USED;
      sendSttorry(signal);
      return;
    } else if (c_secretsfile.m_state ==
               SecretsFileOperationRecord::CLOSE_READ_FILE_0) {
      jam();
      // Secrets File Operation Record no longer needed
      c_secretsfile.m_state = SecretsFileOperationRecord::NOT_USED;
      /**
       * State in Local sysfile is prioritized before the
       * state in DIH. So we first check this, if this is
       * set to initial start of some kind, then it means
       * that we need to clear the file system.
       */
      sendReadLocalSysfile(signal);
      return;
    }
    jamLine(c_secretsfile.m_state);
    ndbabort();
  }
  if (conf->userPointer == LocalSysfile::FILE_ID) {
    if (c_local_sysfile.m_state == LocalSysfile::CLOSE_READ_FILE) {
      jam();
      sendReadLocalSysfileConf(signal, c_local_sysfile.m_sender_ref,
                               c_local_sysfile.m_sender_data);
      return;
    } else if (c_local_sysfile.m_state == LocalSysfile::CLOSE_READ_REF_0) {
      jam();
      c_local_sysfile.m_state = LocalSysfile::OPEN_READ_FILE_1;
      open_local_sysfile(signal, 1, true);
      return;
    } else if (c_local_sysfile.m_state == LocalSysfile::CLOSE_READ_REF_1) {
      jam();
      sendReadLocalSysfileConf(signal, c_local_sysfile.m_sender_ref,
                               c_local_sysfile.m_sender_data);
      return;
    } else if (c_local_sysfile.m_state == LocalSysfile::CLOSE_WRITE_FILE_0) {
      jam();
      c_local_sysfile.m_state = LocalSysfile::OPEN_WRITE_FILE_1;
      open_local_sysfile(signal, 1, false);
      return;
    } else if (c_local_sysfile.m_state == LocalSysfile::CLOSE_WRITE_FILE_1) {
      jam();
      sendWriteLocalSysfileConf(signal);
      return;
    }
    jamLine(c_local_sysfile.m_state);
    ndbabort();
  }
  ndbabort();
}

void Ndbcntr::sendWriteLocalSysfile_startLcp(Signal *signal, Uint32 type) {
  Uint32 gci = m_max_completed_gci;
  if (m_max_completed_gci < c_local_sysfile.m_max_restorable_gci) {
    jam();
    gci = c_local_sysfile.m_max_restorable_gci;
    m_max_completed_gci = gci;
  }
  m_any_lcp_started = true;
  WriteLocalSysfileReq *req = (WriteLocalSysfileReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = type;
  req->nodeRestorableOnItsOwn =
      ReadLocalSysfileReq::NODE_NOT_RESTORABLE_ON_ITS_OWN;
  req->maxGCIRestorable = gci;
  req->lastWrite = 0;
  sendSignal(NDBCNTR_REF, GSN_WRITE_LOCAL_SYSFILE_REQ, signal,
             WriteLocalSysfileReq::SignalLength, JBB);
}

RedoStateRep::RedoAlertState Ndbcntr::get_node_redo_alert_state() {
  RedoStateRep::RedoAlertState redo_alert_state = RedoStateRep::NO_REDO_ALERT;
  for (Uint32 i = 0; i < MAX_NDBMT_LQH_THREADS; i++) {
    if (m_redo_alert_state[i] > redo_alert_state) {
      jamLine(i);
      redo_alert_state = m_redo_alert_state[i];
    }
  }
  return redo_alert_state;
}

void Ndbcntr::execREDO_STATE_REP(Signal *signal) {
  RedoStateRep *rep = (RedoStateRep *)signal->getDataPtr();
  BlockReference sender = signal->senderBlockRef();
  Uint32 instance = refToInstance(sender);
  Uint32 block = refToMain(sender);
  bool any_changes = false;
  if (block == DBDIH) {
    /**
     * The DIH master informs us of a new global redo alert state.
     * We will record this, DIH manages this information based on
     * knowledge of all state's in all nodes.
     */
    ndbrequire(rep->receiverInfo == RedoStateRep::ToNdbcntr);
    if (rep->redoState != (Uint32)m_global_redo_alert_state) {
      jam();
      any_changes = true;
      m_global_redo_alert_state = (RedoStateRep::RedoAlertState)rep->redoState;
    }
  } else {
    /**
     * The backup block in some LDM thread has changed the state of
     * REDO alert state. We will update our view on this state and
     * calculate the REDO alert state of this node.
     */
    RedoStateRep::RedoAlertState node_redo_alert_state;
    ndbrequire(block == BACKUP);
    ndbrequire(m_redo_alert_state[instance] !=
               (RedoStateRep::RedoAlertState)rep->redoState);
    m_redo_alert_state[instance] = (RedoStateRep::RedoAlertState)rep->redoState;
    node_redo_alert_state = get_node_redo_alert_state();
    if (node_redo_alert_state != m_node_redo_alert_state) {
      any_changes = true;
      m_node_redo_alert_state = node_redo_alert_state;
      if (m_first_distributed_lcp_started) {
        jam();
        rep->receiverInfo = RedoStateRep::ToLocalDih;
        rep->redoState = (RedoStateRep::RedoAlertState)m_node_redo_alert_state;
        sendSignal(DBDIH_REF, GSN_REDO_STATE_REP, signal, 2, JBB);
      }
    }
  }
  if (!any_changes) return;
  /**
   * We update the REDO alert state in all LDM threads if there was any
   * change of either global REDO alert state or the node REDO alert state.
   * If the node state is on a higher alert level we use this since we
   * could receive this information before the global state have been
   * updated.
   */
  RedoStateRep::RedoAlertState redo_alert_state;
  if (m_node_redo_alert_state > m_global_redo_alert_state) {
    jam();
    redo_alert_state = m_node_redo_alert_state;
  } else {
    jam();
    redo_alert_state = m_global_redo_alert_state;
  }
  DEB_REDO_CONTROL(("Set node REDO alert state to %u", redo_alert_state));
  rep->receiverInfo = RedoStateRep::ToBackup;
  rep->redoState = (RedoStateRep::RedoAlertState)redo_alert_state;
  send_to_all_backup(signal, GSN_REDO_STATE_REP, 2);
}

void Ndbcntr::execCOPY_FRAG_IN_PROGRESS_REP(Signal *signal) {
  jamEntry();
  m_copy_fragment_in_progress = true;
  DEB_LCP(("m_copy_fragment_in_progress: %u", m_copy_fragment_in_progress));
}

Uint32 Ndbcntr::send_to_all_backup(Signal *signal, Uint32 gsn, Uint32 sig_len) {
  Uint32 ldm_workers =
      globalData.ndbMtLqhWorkers == 0 ? 1 : globalData.ndbMtLqhWorkers;
  if (isNdbMtLqh()) {
    jam();
    for (Uint32 i = 1; i <= ldm_workers; i++) {
      jam();
      BlockReference ref = numberToRef(BACKUP, i, getOwnNodeId());
      sendSignal(ref, gsn, signal, sig_len, JBB);
    }
  } else {
    jam();
    sendSignal(BACKUP_REF, gsn, signal, sig_len, JBB);
  }
  return ldm_workers;
}

Uint32 Ndbcntr::send_to_all_lqh(Signal *signal, Uint32 gsn, Uint32 sig_len) {
  Uint32 ldm_workers =
      globalData.ndbMtLqhWorkers == 0 ? 1 : globalData.ndbMtLqhWorkers;
  if (isNdbMtLqh()) {
    jam();
    for (Uint32 i = 1; i <= ldm_workers; i++) {
      jam();
      BlockReference ref = numberToRef(DBLQH, i, getOwnNodeId());
      sendSignal(ref, gsn, signal, sig_len, JBB);
    }
  } else {
    jam();
    sendSignal(DBLQH_REF, gsn, signal, sig_len, JBB);
  }
  return ldm_workers;
}

void Ndbcntr::execCOPY_FRAG_NOT_IN_PROGRESS_REP(Signal *signal) {
  jamEntry();
  if (!m_copy_fragment_in_progress) {
    jam();
    /**
     * DIH master can start up to one COPY_ACTIVEREQ in parallel per
     * LDM thread, so this can arrive multiple times. If so we only
     * handle the first and ignore the rest.
     */
    return;
  }
  /**
   * We need to report back to the LDM threads such that we
   * are sure that no new local LCPs are started when we
   * are in the process of waiting for the last one to complete.
   */
  m_copy_fragment_in_progress = false;
  DEB_LCP(("m_copy_fragment_in_progress: %u", m_copy_fragment_in_progress));
  if (!m_any_lcp_started) {
    jam();
    sendWriteLocalSysfile_startLcp(signal, 1);
    return;
  }
  send_to_all_lqh(signal, GSN_COPY_FRAG_NOT_IN_PROGRESS_REP, 1);
}

#define START_LCP_LEVEL 50
void Ndbcntr::execUNDO_LOG_LEVEL_REP(Signal *signal) {
  jamEntry();
  UndoLogLevelRep *rep = (UndoLogLevelRep *)signal->getDataPtr();
  Uint32 levelUsed = rep->levelUsed;
  DEB_UNDO(("UNDO log level = %u", levelUsed));
  if (m_copy_fragment_in_progress &&
      !c_local_sysfile.m_initial_write_local_sysfile_ongoing &&
      (levelUsed >= START_LCP_LEVEL
#ifdef ERROR_INSERT
       || (ERROR_INSERTED(1011))
#endif
           )) {
    /**
     * If no local LCP is ongoing we need to start one.
     * We have reached a threshold where we consider it time to
     * start an LCP to ensure that we don't run out of log space.
     *
     * We will only start a local LCP during the time Copy Fragment
     * is ongoing. We will also not start a full local LCP while we
     * are right now spinning up the first local LCP. We will be back
     * here within 2 seconds to start a full local LCP later.
     */
#ifdef ERROR_INSERT
    CLEAR_ERROR_INSERT_VALUE;
#endif
    if (m_full_local_lcp_started) {
      jam();
      DEB_LCP(("A full local LCP already started"));
      return;
    }
    DEB_LCP(("Start a full local LCP"));
    if (m_local_lcp_started && m_local_lcp_completed) {
      jam();
      /**
       * We are running a local LCP, but not yet a full local LCP.
       * However the local LCP have already completed the LCP
       * phase and is now merely waiting for cutting the log tail,
       * so nothing extra to do.
       */
      DEB_LCP(("Start a full local LCP wasn't necessary"));
      return;
    }
    if (!m_local_lcp_started && !m_initial_local_lcp_started) {
      jam();
      m_initial_local_lcp_started = true;
      m_local_lcp_started = true;
      m_full_local_lcp_started = true;
      ndbrequire(!c_local_sysfile.m_initial_write_local_sysfile_ongoing);
      c_local_sysfile.m_initial_write_local_sysfile_ongoing = true;
      sendWriteLocalSysfile_startLcp(signal, 0);
      return;
    }
    if (!m_local_lcp_started) {
      jam();
      m_local_lcp_started = true;
      m_full_local_lcp_started = true;
      m_local_lcp_id++;
    }
    jam();
    sendSTART_FULL_LOCAL_LCP_ORD(signal);
  }
  return;
}

void Ndbcntr::sendSTART_FULL_LOCAL_LCP_ORD(Signal *signal) {
  DEB_LCP(("Send START_FULL_LOCAL_LCP_ORD(%u,%u)", m_lcp_id, m_local_lcp_id));
  signal->theData[0] = m_lcp_id;
  signal->theData[1] = m_local_lcp_id;
  send_to_all_lqh(signal, GSN_START_FULL_LOCAL_LCP_ORD, 2);
  m_local_lcp_started = true;
  m_full_local_lcp_started = true;
}

void Ndbcntr::execSTART_LOCAL_LCP_ORD(Signal *signal) {
  jamEntry();
  if (m_local_lcp_started) {
    jam();
    /**
     * First START_LOCAL_LCP_ORD have already arrived, no need
     * to start up again.
     */
    return;
  }
  m_local_lcp_started = true;
  /**
   * Haven't assigned first local LCP id yet. Time to do this
   * now.
   */
  ndbrequire(!m_initial_local_lcp_started);
  ndbrequire(!c_local_sysfile.m_initial_write_local_sysfile_ongoing);
  c_local_sysfile.m_initial_write_local_sysfile_ongoing = true;
  sendWriteLocalSysfile_startLcp(signal, 0);
}

void Ndbcntr::execSET_LOCAL_LCP_ID_REQ(Signal *signal) {
  Uint32 ldm_workers =
      globalData.ndbMtLqhWorkers == 0 ? 1 : globalData.ndbMtLqhWorkers;
  Uint32 max_lcp_id = signal->theData[0];
  Uint32 max_local_lcp_id = signal->theData[1];

  if (max_lcp_id > m_lcp_id) {
    jam();
    m_lcp_id = max_lcp_id;
    m_local_lcp_id = max_local_lcp_id;
  } else if (max_lcp_id == m_lcp_id && max_local_lcp_id > m_local_lcp_id) {
    jam();
    m_local_lcp_id = max_local_lcp_id;
  }
  m_set_local_lcp_id_reqs++;
  if (ldm_workers > m_set_local_lcp_id_reqs) {
    jam();
    return;
  }
  DEB_LCP(("Maximum LCP id restored was LCP(%u,%u)", m_lcp_id, m_local_lcp_id));
  if (ldm_workers == m_set_local_lcp_id_reqs) {
    jam();
    signal->theData[0] = m_lcp_id;
    signal->theData[1] = m_local_lcp_id;
    send_to_all_lqh(signal, GSN_SET_LOCAL_LCP_ID_CONF, 2);
    return;
  }
  ndbabort();
}

void Ndbcntr::write_local_sysfile_start_lcp_done(Signal *signal) {
  ndbrequire(c_local_sysfile.m_initial_write_local_sysfile_ongoing);
  c_local_sysfile.m_initial_write_local_sysfile_ongoing = false;
  if (!m_copy_fragment_in_progress) {
    jam();
    ndbrequire(m_full_local_lcp_started);
    /**
     * Copy fragment process completed before we had time to even start
     * a local LCP. So we avoid starting one now. The restart is already
     * progressing beyond this point so we simply don't do anything here.
     */
    m_full_local_lcp_started = false;
    m_local_lcp_started = 0;
    return;
  }
  m_local_lcp_id++;
  if (m_full_local_lcp_started) {
    jam();
    sendSTART_FULL_LOCAL_LCP_ORD(signal);
  } else {
    jam();
    sendSTART_LOCAL_LCP_ORD(signal);
  }
}

void Ndbcntr::sendSTART_LOCAL_LCP_ORD(Signal *signal) {
  DEB_LCP(("Send START_LOCAL_LCP_ORD(%u,%u)", m_lcp_id, m_local_lcp_id));
  signal->theData[0] = m_lcp_id;
  signal->theData[1] = m_local_lcp_id;
  send_to_all_lqh(signal, GSN_START_LOCAL_LCP_ORD, 2);
}

void Ndbcntr::execWAIT_ALL_COMPLETE_LCP_REQ(Signal *signal) {
  jamEntry();
  ndbrequire(m_local_lcp_started);
  if (signal->theData[1] == 0) {
    /**
     * The LDM had no fragments to restore and thus no fragments
     * checkpoint either. We will wait to start the complete of
     * the checkpoint until at least one LDM with fragments to
     * restore have sent the signal. The code in DBLQH is not
     * designed to handle that we start up new LCP_FRAG_ORD's
     * in the first phase of the Copy fragment and at the same
     * time complete an LCP. Thus we have to wait with completing
     * the first local LCP until at least the second phase of
     * copy fragment has started.
     */
    jam();
    DEB_LCP(("WAIT_ALL_COMPLETE_LCP_REQ received from empty LDM"));
    return;
  }
  if (m_received_wait_all) {
    /**
     * Ignore, already received it from one of the LDMs.
     * It is sufficient to receive it from one, then we
     * will ensure that all receive the rest of the
     * interaction. At least one fragment exists since someone
     * decided to start a local LCP. Thus at least one LDM should
     * send WAIT_ALL_COMPLETE_LCP_REQ with at least 1 fragment to
     * restore.
     */
    jam();
    return;
  }
  m_received_wait_all = true;
  m_outstanding_wait_lcp =
      send_to_all_lqh(signal, GSN_WAIT_COMPLETE_LCP_REQ, 1);
  DEB_LCP(("WAIT_ALL_COMPLETE_LCP_REQ received, sent WAIT_COMPLETE_LCP_REQ"));
}

void Ndbcntr::execWAIT_COMPLETE_LCP_CONF(Signal *signal) {
  WaitCompleteLcpConf *conf = (WaitCompleteLcpConf *)signal->getDataPtr();
  jamEntry();
  ndbrequire(m_received_wait_all);
  ndbrequire(m_outstanding_wait_lcp > 0);
  if (conf->maxGciInLcp > m_max_gci_in_lcp) {
    jam();
    m_max_gci_in_lcp = conf->maxGciInLcp;
  }
  if (m_max_keep_gci == 0 || conf->maxKeepGci < m_max_keep_gci) {
    jam();
    m_max_keep_gci = conf->maxKeepGci;
  }
  m_outstanding_wait_lcp--;
  if (m_outstanding_wait_lcp > 0) {
    jam();
    return;
  }
  DEB_LCP(("Received all WAIT_COMPLETE_LCP_CONF"));
  m_local_lcp_completed = true;
  m_ready_to_cut_log_tail = true;
  if (!is_ready_to_cut_log_tail()) {
    jam();
    return;
  }
  send_cut_log_tail(signal);
}

void Ndbcntr::execRESTORABLE_GCI_REP(Signal *signal) {
  jamEntry();
  Uint32 gci = signal->theData[0];
  m_max_completed_gci = gci;
  DEB_LCP(("(%u)max_completed_gci: %u", instance(), gci));
  if (is_ready_to_cut_log_tail()) {
    jam();
    send_cut_log_tail(signal);
  }
}

void Ndbcntr::execSTART_DISTRIBUTED_LCP_ORD(Signal *signal) {
  jamEntry();
  Uint32 ldm_workers =
      globalData.ndbMtLqhWorkers == 0 ? 1 : globalData.ndbMtLqhWorkers;
  Uint32 lcpId = signal->theData[0];
  if (!m_distributed_lcp_started) {
    jam();
    DEB_LCP(("Start distributed LCP: lcpId = %u", lcpId));
    ndbrequire(m_outstanding_wait_lcp == 0);
    m_first_distributed_lcp_started = true;
    m_distributed_lcp_started = true;
    m_outstanding_wait_lcp = ldm_workers;
    m_distributed_lcp_id = lcpId;
    m_max_gci_in_lcp = 0;
    m_max_keep_gci = 0;
    m_ready_to_cut_log_tail = false;
  }
}

void Ndbcntr::execLCP_ALL_COMPLETE_REQ(Signal *signal) {
  /**
   * LQH Proxy will ensure that all LDMs will receive an LCP_FRAG_ORD
   * with last fragment flag set. Thus we will expect to receive this
   * signal from all LDMs.
   */
  jamEntry();
  LcpAllCompleteReq *req = (LcpAllCompleteReq *)signal->getDataPtr();
  Uint32 lcpId = req->lcpId;

  ndbrequire(lcpId == m_distributed_lcp_id);
  if (req->maxGciInLcp > m_max_gci_in_lcp) {
    jam();
    m_max_gci_in_lcp = req->maxGciInLcp;
  }
  if (m_max_keep_gci == 0 || req->maxKeepGci < m_max_keep_gci) {
    jam();
    m_max_keep_gci = req->maxKeepGci;
  }
  ndbrequire(m_distributed_lcp_started);
  ndbrequire(m_outstanding_wait_lcp > 0);
  m_outstanding_wait_lcp--;
  if (m_outstanding_wait_lcp > 0) {
    /* Still waiting */
    jam();
    return;
  }
  DEB_LCP(
      ("Received all LCP_ALL_COMPLETE_REQ"
       " maxGciInLcp: %u, maxCompletedGci: %u",
       m_max_gci_in_lcp, m_max_completed_gci));

  m_ready_to_cut_log_tail = true;
  if (!is_ready_to_cut_log_tail()) {
    jam();
    return;
  }
  send_cut_log_tail(signal);
}

bool Ndbcntr::is_ready_to_cut_log_tail(void) {
  if (m_ready_to_cut_log_tail && m_max_completed_gci >= m_max_gci_in_lcp) {
    jam();
    return true;
  }
  jam();
  return false;
}

void Ndbcntr::send_cut_log_tail(Signal *signal) {
  DEB_LCP(("Sending CUT_LOG_REDO/UNDO_TAIL_REQ"));
  m_wait_cut_undo_log_tail = true;
  m_ready_to_cut_log_tail = false;
  signal->theData[0] = m_max_keep_gci;
  m_outstanding_wait_cut_redo_log_tail =
      send_to_all_lqh(signal, GSN_CUT_REDO_LOG_TAIL_REQ, 1);
  sendSignal(LGMAN_REF, GSN_CUT_UNDO_LOG_TAIL_REQ, signal, 1, JBB);
}

void Ndbcntr::execCUT_UNDO_LOG_TAIL_CONF(Signal *signal) {
  jamEntry();
  ndbrequire(m_wait_cut_undo_log_tail);
  m_wait_cut_undo_log_tail = false;
  check_cut_log_tail_completed(signal);
}

void Ndbcntr::execCUT_REDO_LOG_TAIL_CONF(Signal *signal) {
  jamEntry();
  ndbrequire(m_outstanding_wait_cut_redo_log_tail > 0);
  m_outstanding_wait_cut_redo_log_tail--;
  check_cut_log_tail_completed(signal);
}

void Ndbcntr::check_cut_log_tail_completed(Signal *signal) {
  if (m_wait_cut_undo_log_tail || m_outstanding_wait_cut_redo_log_tail > 0) {
    jam();
    return;
  }
  DEB_LCP(("Received all CUT_LOG_REDO/UNDO_TAIL_CONF"));
  if (m_local_lcp_started) {
    jam();
    ndbrequire(!m_distributed_lcp_started);
    sendWAIT_ALL_COMPLETE_LCP_CONF(signal);
  } else {
    ndbrequire(m_distributed_lcp_started);
    sendLCP_ALL_COMPLETE_CONF(signal);
  }
}

void Ndbcntr::sendLCP_ALL_COMPLETE_CONF(Signal *signal) {
  DEB_LCP(("Send LCP_ALL_COMPLETE_CONF"));
  signal->theData[0] = m_distributed_lcp_id;
  send_to_all_lqh(signal, GSN_LCP_ALL_COMPLETE_CONF, 1);
  g_eventLogger->debug(
      "Distributed LCP id %u complete in this node,"
      " max GCI in LCP = %u, also cut UNDO log tail"
      " and REDO log tail to keepGCI = %u",
      m_distributed_lcp_id, m_max_gci_in_lcp, m_max_keep_gci);
  m_distributed_lcp_started = false;
  m_distributed_lcp_id = 0;
  m_max_gci_in_lcp = 0;
  m_max_keep_gci = 0;
}

void Ndbcntr::sendWAIT_ALL_COMPLETE_LCP_CONF(Signal *signal) {
  DEB_LCP(("Send WAIT_ALL_COMPLETE_LCP_CONF"));
  signal->theData[0] = reference();
  send_to_all_lqh(signal, GSN_WAIT_ALL_COMPLETE_LCP_CONF, 1);
  m_received_wait_all = false;
  m_local_lcp_started = false;
  m_local_lcp_completed = false;
  m_full_local_lcp_started = false;
  DEB_LCP(("m_lcp_started false again"));
}

void Ndbcntr::get_node_group_mask(Signal *signal, NodeId node_id,
                                  NdbNodeBitmask &mask) {
  CheckNodeGroups *sd = (CheckNodeGroups *)signal->getDataPtrSend();
  sd->blockRef = reference();
  sd->requestType =
      CheckNodeGroups::Direct | CheckNodeGroups::GetNodeGroupMembers;
  sd->nodeId = node_id;
  EXECUTE_DIRECT_MT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal,
                    CheckNodeGroups::SignalLength, 0);
  jamEntry();
  mask.assign(sd->mask);
}

bool Ndbcntr::is_nodegroup_starting(Signal *signal, NodeId node_id) {
  NdbNodeBitmask mask;
  get_node_group_mask(signal, node_id, mask);
  for (Uint32 i = 1; i < MAX_NDB_NODES; i++) {
    if (mask.get(i) && i != getOwnNodeId()) {
      jam();
      jamLine(Uint16(i));
      /* Node i is in same node group */
      if (is_node_starting(i)) {
        jam();
        return true;
      }
    }
  }
  jam();
  return false;
}

bool Ndbcntr::is_node_starting(NodeId node_id) {
  if (c_start.m_starting.get(node_id) ||
      (c_cntr_startedNodeSet.get(node_id) && !c_startedNodeSet.get(node_id))) {
    jam();
    return true;
  } else {
    jam();
    return false;
  }
}

bool Ndbcntr::is_node_started(NodeId node_id) {
  if (c_startedNodeSet.get(node_id)) {
    jam();
    return true;
  } else {
    jam();
    return false;
  }
}
template class Vector<ddentry>;
