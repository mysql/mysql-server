/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <mgmapi.h>
#include <mgmapi_internal.h>

#include <NdbOut.hpp>
#include <Properties.hpp>
#include <InputStream.hpp>
#include <NdbTick.h>

#include <debugger/EventLogger.hpp>
#include <kernel/NodeBitmask.hpp>

#include "ndb_logevent.hpp"

extern
int ndb_mgm_listen_event_internal(NdbMgmHandle, const int filter[],
                                  int, NDB_SOCKET_TYPE*);

struct ndb_logevent_error_msg {
  enum ndb_logevent_handle_error code;
  const char *msg;
};

struct ndb_logevent_error_msg ndb_logevent_error_messages[]= {
  { NDB_LEH_READ_ERROR,              "Read error" },
  { NDB_LEH_MISSING_EVENT_SPECIFIER, "Missing event specifier" },
  { NDB_LEH_UNKNOWN_EVENT_VARIABLE,  "Unknown event variable" },
  { NDB_LEH_UNKNOWN_EVENT_TYPE,      "Unknown event type" },
  { NDB_LEH_INTERNAL_ERROR,          "Unknown internal error" },
  { NDB_LEH_NO_ERROR,0}
};

struct ndb_logevent_handle {
  NDB_SOCKET_TYPE socket;
  enum ndb_logevent_handle_error m_error;
};

/*
  Create a new NdbLogEventHandle for reading events from
  the same socket as the NdbMgmHandle
*/

NdbLogEventHandle
ndb_mgm_create_logevent_handle_same_socket(NdbMgmHandle mh)
{
  NdbLogEventHandle h=
    (NdbLogEventHandle)malloc(sizeof(ndb_logevent_handle));
  if (!h)
    return NULL;

  h->socket= _ndb_mgm_get_socket(mh);

  return h;
}

extern "C"
NdbLogEventHandle
ndb_mgm_create_logevent_handle(NdbMgmHandle mh,
			       const int filter[])
{
  NdbLogEventHandle h=
    (NdbLogEventHandle)malloc(sizeof(ndb_logevent_handle));
  if (!h)
    return NULL;

  NDB_SOCKET_TYPE sock;
  if(ndb_mgm_listen_event_internal(mh, filter, 1, &sock) < 0)
  {
    free(h);
    return 0;
  }

  h->socket= sock;

  return h;
}

extern "C"
#ifdef NDB_WIN
SOCKET
ndb_logevent_get_fd(const NdbLogEventHandle h)
{
  return h->socket.s;
}
#else
int
ndb_logevent_get_fd(const NdbLogEventHandle h)
{
  return h->socket.fd;
}
#endif

extern "C"
void ndb_mgm_destroy_logevent_handle(NdbLogEventHandle * h)
{
  if( !h )
    return;

  if ( *h )
    my_socket_close((*h)->socket);

  free(*h);
  * h = 0;
}

#define ROW(a,b,c,d) \
{ NDB_LE_ ## a,  b, c, 0, offsetof(struct ndb_logevent, a.d), \
  sizeof(((struct ndb_logevent *)0)->a.d) }

#define ROW_FN(a,b,c,d,e) \
{ NDB_LE_ ## a,  b, c, e, offsetof(struct ndb_logevent, a.d), \
  sizeof(((struct ndb_logevent *)0)->a.d) }

static int ref_to_node(int ref){
  return ref & 0xFFFF;
}

struct Ndb_logevent_body_row ndb_logevent_body[]= {

  // Connection
  ROW( Connected,           "node",          1, node),

  ROW( Disconnected,        "node",          1, node),

  ROW( CommunicationClosed, "node",          1, node),

  ROW( CommunicationOpened, "node",          1, node),

  ROW( ConnectedApiVersion, "node",          1, node),
  ROW( ConnectedApiVersion, "version",       2, version),

      /* CHECKPOINT */

  ROW( GlobalCheckpointStarted, "gci", 1, gci),

  ROW( GlobalCheckpointCompleted, "gci", 1, gci),

  ROW( LocalCheckpointStarted, "lci",         1, lci),
  ROW( LocalCheckpointStarted, "keep_gci",    2, keep_gci),
  ROW( LocalCheckpointStarted, "restore_gci", 3, restore_gci),

  ROW( LocalCheckpointCompleted, "lci", 1, lci),

  ROW( LCPStoppedInCalcKeepGci, "data", 1, data),

  ROW( LCPFragmentCompleted, "node",        1, node),
  ROW( LCPFragmentCompleted, "table_id",    2, table_id),
  ROW( LCPFragmentCompleted, "fragment_id", 3, fragment_id),

  ROW( UndoLogBlocked, "acc_count", 1, acc_count),
  ROW( UndoLogBlocked, "tup_count", 2, tup_count),

      /* STARTUP */
  ROW( NDBStartStarted, "version", 1, version),

  ROW( NDBStartCompleted, "version", 1, version),

//  ROW( STTORRYRecieved),

  ROW( StartPhaseCompleted, "phase",     1, phase),
  ROW( StartPhaseCompleted, "starttype", 2, starttype),

  ROW( CM_REGCONF, "own_id",       1, own_id),
  ROW( CM_REGCONF, "president_id", 2, president_id),
  ROW( CM_REGCONF, "dynamic_id",   3, dynamic_id),

  ROW( CM_REGREF, "own_id",   1, own_id),
  ROW( CM_REGREF, "other_id", 2, other_id),
  ROW( CM_REGREF, "cause",    3, cause),

  ROW( FIND_NEIGHBOURS, "own_id",     1, own_id),
  ROW( FIND_NEIGHBOURS, "left_id",    3, left_id),
  ROW( FIND_NEIGHBOURS, "right_id",   3, right_id),
  ROW( FIND_NEIGHBOURS, "dynamic_id", 4, dynamic_id),

  ROW( NDBStopStarted, "stoptype", 1, stoptype),

  ROW( NDBStopCompleted, "action",         1, action),
  ROW( NDBStopCompleted, "signum",         2, signum),

  ROW( NDBStopForced, "action",         1, action),
  ROW( NDBStopForced, "signum",         2, signum),
  ROW( NDBStopForced, "error",          3, error),
  ROW( NDBStopForced, "sphase",         4, sphase),
  ROW( NDBStopForced, "extra",          5, extra),

//  ROW( NDBStopAborted),

  ROW( StartREDOLog, "node",           1, node),
  ROW( StartREDOLog, "keep_gci",       2, keep_gci),
  ROW( StartREDOLog, "completed_gci",  3, completed_gci),
  ROW( StartREDOLog, "restorable_gci", 4, restorable_gci),

  ROW( StartLog, "log_part", 1, log_part),
  ROW( StartLog, "start_mb", 2, start_mb),
  ROW( StartLog, "stop_mb",  3, stop_mb),
  ROW( StartLog, "gci",      4, gci),

  ROW( UNDORecordsExecuted, "block", 1, block),
  ROW( UNDORecordsExecuted, "data1", 2, data1),
  ROW( UNDORecordsExecuted, "data2", 3, data2),
  ROW( UNDORecordsExecuted, "data3", 4, data3),
  ROW( UNDORecordsExecuted, "data4", 5, data4),
  ROW( UNDORecordsExecuted, "data5", 6, data5),
  ROW( UNDORecordsExecuted, "data6", 7, data6),
  ROW( UNDORecordsExecuted, "data7", 8, data7),
  ROW( UNDORecordsExecuted, "data8", 9, data8),
  ROW( UNDORecordsExecuted, "data9", 10, data9),
  ROW( UNDORecordsExecuted, "data10", 11, data10),
  
      /* NODERESTART */
//  ROW( NR_CopyDict),

//  ROW( NR_CopyDistr),

  ROW( NR_CopyFragsStarted, "dest_node", 1, dest_node),

  ROW( NR_CopyFragDone, "dest_node",   1, dest_node),
  ROW( NR_CopyFragDone, "table_id",    2, table_id),
  ROW( NR_CopyFragDone, "fragment_id", 3, fragment_id),

  ROW( NR_CopyFragsCompleted, "dest_node", 1, dest_node),

  ROW( NodeFailCompleted, "block",           1, block), /* 0 = all */
  ROW( NodeFailCompleted, "failed_node",     2, failed_node),
  ROW( NodeFailCompleted, "completing_node", 3, completing_node), /* 0 = all */

  ROW( NODE_FAILREP, "failed_node",   1, failed_node),
  ROW( NODE_FAILREP, "failure_state", 2, failure_state),

	/* TODO */
  ROW( ArbitState,   "code",          1, code),
  ROW( ArbitState,   "arbit_node",    2, arbit_node),
  ROW( ArbitState,   "ticket_0",      3, ticket_0),
  ROW( ArbitState,   "ticket_1",      4, ticket_1),

	/* TODO */
  ROW( ArbitResult,  "code",          1, code),
  ROW( ArbitResult,  "arbit_node",    2, arbit_node),
  ROW( ArbitResult,  "ticket_0",      3, ticket_0),
  ROW( ArbitResult,  "ticket_1",      4, ticket_1),

//  ROW( GCP_TakeoverStarted),

//  ROW( GCP_TakeoverCompleted),

//  ROW( LCP_TakeoverStarted),

  ROW( LCP_TakeoverCompleted, "state", 1, state),

      /* STATISTIC */
  ROW( TransReportCounters, "trans_count",       1, trans_count),
  ROW( TransReportCounters, "commit_count",      2, commit_count),
  ROW( TransReportCounters, "read_count",        3, read_count),
  ROW( TransReportCounters, "simple_read_count", 4, simple_read_count),
  ROW( TransReportCounters, "write_count",       5, write_count),
  ROW( TransReportCounters, "attrinfo_count",    6, attrinfo_count),
  ROW( TransReportCounters, "conc_op_count",     7, conc_op_count),
  ROW( TransReportCounters, "abort_count",       8, abort_count),
  ROW( TransReportCounters, "scan_count",        9, scan_count),
  ROW( TransReportCounters, "range_scan_count", 10, range_scan_count),

  ROW( OperationReportCounters, "ops", 1, ops),

  ROW( TableCreated, "table_id", 1, table_id),

  ROW( JobStatistic, "mean_loop_count", 1, mean_loop_count),

  ROW( SendBytesStatistic, "to_node",         1, to_node),
  ROW( SendBytesStatistic, "mean_sent_bytes", 2, mean_sent_bytes),

  ROW( ReceiveBytesStatistic, "from_node",           1, from_node),
  ROW( ReceiveBytesStatistic, "mean_received_bytes", 2, mean_received_bytes),

  ROW( MemoryUsage, "gth",          1, gth),
  ROW( MemoryUsage, "page_size_bytes", 2, page_size_bytes),
  ROW( MemoryUsage, "pages_used",   3, pages_used),
  ROW( MemoryUsage, "pages_total",  4, pages_total),
  ROW( MemoryUsage, "block",        5, block),

  ROW( MTSignalStatistics, "mt_deliver_thread", 1, thr_no),
  ROW( MTSignalStatistics, "mt_prioa_count", 2, prioa_count),
  ROW( MTSignalStatistics, "mt_prioa_size", 3, prioa_size),
  ROW( MTSignalStatistics, "mt_priob_count", 4, priob_count),
  ROW( MTSignalStatistics, "mt_priob_size", 5, priob_size),

      /* ERROR */
  ROW( TransporterError, "to_node", 1, to_node),
  ROW( TransporterError, "code",    2, code),

  ROW( TransporterWarning, "to_node", 1, to_node),
  ROW( TransporterWarning, "code",    2, code),

  ROW( MissedHeartbeat, "node",  1, node),
  ROW( MissedHeartbeat, "count", 2, count),

  ROW( DeadDueToHeartbeat, "node", 1, node),

	/* TODO */
//  ROW( WarningEvent),

      /* INFO */
  ROW( SentHeartbeat, "node", 1, node),

  ROW( CreateLogBytes, "node", 1, node),

	/* TODO */
//  ROW( InfoEvent),

  ROW( EventBufferStatus, "usage",        1, usage),
  ROW( EventBufferStatus, "alloc",        2, alloc),
  ROW( EventBufferStatus, "max",          3, max),
  ROW( EventBufferStatus, "apply_gci_l",  4, apply_gci_l),
  ROW( EventBufferStatus, "apply_gci_h",  5, apply_gci_h),
  ROW( EventBufferStatus, "latest_gci_l", 6, latest_gci_l),
  ROW( EventBufferStatus, "latest_gci_h", 7, latest_gci_h),

  // Backup
  ROW_FN( BackupStarted,    "starting_node", 1, starting_node, ref_to_node),
  ROW( BackupStarted,       "backup_id",     2, backup_id),

  ROW_FN(BackupFailedToStart,"starting_node",1, starting_node, ref_to_node),
  ROW( BackupFailedToStart, "error",         2, error),

  ROW_FN( BackupCompleted,  "starting_node", 1, starting_node, ref_to_node),
  ROW( BackupCompleted,     "backup_id",     2, backup_id), 
  ROW( BackupCompleted,     "start_gci",     3, start_gci),
  ROW( BackupCompleted,     "stop_gci",      4, stop_gci),
  ROW( BackupCompleted,     "n_bytes",       5, n_bytes),
  ROW( BackupCompleted,     "n_records",     6, n_records), 
  ROW( BackupCompleted,     "n_log_bytes",   7, n_log_bytes),
  ROW( BackupCompleted,     "n_log_records", 8, n_log_records),
  ROW( BackupCompleted,     "n_bytes_hi",    9+NdbNodeBitmask::Size, n_bytes_hi),
  ROW( BackupCompleted,     "n_records_hi", 10+NdbNodeBitmask::Size, n_records_hi), 
  ROW( BackupCompleted,     "n_log_bytes_hi",   11+NdbNodeBitmask::Size, n_log_bytes_hi),
  ROW( BackupCompleted,     "n_log_records_hi", 12+NdbNodeBitmask::Size, n_log_records_hi), 

  ROW_FN( BackupStatus,     "starting_node",    1, starting_node, ref_to_node),
  ROW( BackupStatus,        "backup_id",        2, backup_id), 
  ROW( BackupStatus,        "n_bytes_lo",       3, n_bytes_lo),
  ROW( BackupStatus,        "n_bytes_hi",       4, n_bytes_hi),
  ROW( BackupStatus,        "n_records_lo",     5, n_records_lo), 
  ROW( BackupStatus,        "n_records_hi",     6, n_records_hi), 
  ROW( BackupStatus,        "n_log_bytes_lo",   7, n_log_bytes_lo),
  ROW( BackupStatus,        "n_log_bytes_hi",   8, n_log_bytes_hi),
  ROW( BackupStatus,        "n_log_records_lo", 9, n_log_records_lo),
  ROW( BackupStatus,        "n_log_records_hi",10, n_log_records_hi),

  ROW_FN( BackupAborted,    "starting_node", 1, starting_node, ref_to_node),
  ROW( BackupAborted,       "backup_id",     2, backup_id),
  ROW( BackupAborted,       "error",         3, error),

  ROW( RestoreStarted,      "backup_id",     1, backup_id),
  ROW( RestoreStarted,      "node_id",       2, node_id),

  ROW( RestoreMetaData,     "backup_id",     1, backup_id),
  ROW( RestoreMetaData,     "node_id",       2, node_id),
  ROW( RestoreMetaData,     "n_tables",      3, n_tables),
  ROW( RestoreMetaData,     "n_tablespaces", 4, n_tablespaces),
  ROW( RestoreMetaData,     "n_logfilegroups", 5, n_logfilegroups),
  ROW( RestoreMetaData,     "n_datafiles",   6, n_datafiles),
  ROW( RestoreMetaData,     "n_undofiles",   7, n_undofiles),

  ROW( RestoreData,         "backup_id",     1, backup_id),
  ROW( RestoreData,         "node_id",       2, node_id),
  ROW( RestoreData,         "n_records_lo",  3, n_records_lo),
  ROW( RestoreData,         "n_records_hi",  4, n_records_hi),
  ROW( RestoreData,         "n_bytes_lo",    5, n_bytes_lo),
  ROW( RestoreData,         "n_bytes_hi",    6, n_bytes_hi),

  ROW( RestoreLog,          "backup_id",     1, backup_id),
  ROW( RestoreLog,          "node_id",       2, node_id),
  ROW( RestoreLog,          "n_records_lo",  3, n_records_lo),
  ROW( RestoreLog,          "n_records_hi",  4, n_records_hi),
  ROW( RestoreLog,          "n_bytes_lo",    5, n_bytes_lo),
  ROW( RestoreLog,          "n_bytes_hi",    6, n_bytes_hi),

  ROW( RestoreCompleted,    "backup_id",     1, backup_id),
  ROW( RestoreCompleted,    "node_id",       2, node_id),

  ROW( SingleUser,          "type",	     1, type),
  ROW( SingleUser,          "node_id",	     2, node_id),

  ROW( LogFileInitStatus,   "node_id",       1, node_id ),
  ROW( LogFileInitStatus,   "total_files",   2, total_files),
  ROW( LogFileInitStatus,   "file_done",     3, file_done),
  ROW( LogFileInitStatus,   "total_mbytes",  4, total_mbytes),
  ROW( LogFileInitStatus,   "mbytes_done",   5, mbytes_done),

 
  ROW( RedoStatus,          "log_part",      1, log_part), 
  ROW( RedoStatus,          "head_file_no",  2, head_file_no), 
  ROW( RedoStatus,          "head_mbyte",    3, head_mbyte), 
  ROW( RedoStatus,          "tail_file_no",  4, tail_file_no), 
  ROW( RedoStatus,          "tail_mbyte",    5, tail_mbyte), 
  ROW( RedoStatus,          "total_hi",      6, total_hi), 
  ROW( RedoStatus,          "total_lo",      7, total_lo), 
  ROW( RedoStatus,          "free_hi",       8, free_hi), 
  ROW( RedoStatus,          "free_lo",       9, free_lo), 
  ROW( RedoStatus,          "no_logfiles",  10, no_logfiles),
  ROW( RedoStatus,          "logfilesize",  11, logfilesize),
 
  { NDB_LE_ILLEGAL_TYPE, 0, 0, 0, 0, 0}
};

struct Ndb_logevent_header_row {
  const char            *token;    // token to use for text transfer
  int                    offset;   // offset into struct ndb_logevent
  int                    size;
};

#define ROW2(a,b) \
{ a, offsetof(struct ndb_logevent, b), \
  sizeof(((struct ndb_logevent *)0)->b) }

struct Ndb_logevent_header_row ndb_logevent_header[]= {
  ROW2( "type",          type),
  ROW2( "time",          time),
  ROW2( "source_nodeid", source_nodeid),
  { 0, 0, 0 }
};

static int
insert_row(const char * pair, Properties & p){
  BaseString tmp(pair);
  
  tmp.trim(" \t\n\r");
  Vector<BaseString> split;
  tmp.split(split, ":=", 2);
  if(split.size() != 2)
    return -1;
  p.put(split[0].trim().c_str(), split[1].trim().c_str());

  return 0;
}

static
int memcpy_atoi(void *dst, const char *str, int sz)
{
  switch (sz)
  {
  case 1:
  {
    Int8 val= atoi(str);
    memcpy(dst,&val,sz);
    return 0;
  }
  case 2:
  {
    Int16 val= atoi(str);
    memcpy(dst,&val,sz);
    return 0;
  }
  case 4:
  {
    Int32 val= atoi(str);
    memcpy(dst,&val,sz);
    return 0;
  }
  case 8:
  {
    Int64 val= atoi(str);
    memcpy(dst,&val,sz);
    return 0;
  }
  default:
  {
    return -1;
  }
  }
}

extern "C"
int ndb_logevent_get_next(const NdbLogEventHandle h,
                           struct ndb_logevent *dst,
                           unsigned timeout_in_milliseconds)
{
  int rc = ndb_logevent_get_next2(h,
                                  dst,
                                  timeout_in_milliseconds);
  if (rc == 1)
  {
    /**
     * Undo effect of bug#16723708 fix to maintain
     * backwards compatibility
     */
    Uint32 category = (Uint32) dst->category;
    switch(category)
    {
    case NDB_MGM_ILLEGAL_EVENT_CATEGORY:
      category = (Uint32) LogLevel::llInvalid;
      break;
    default:
      category -= CFG_MIN_LOGLEVEL;
    }
    dst->category = (enum ndb_mgm_event_category) category;
  }

  return rc;
}

extern "C"
int ndb_logevent_get_next2(const NdbLogEventHandle h,
                           struct ndb_logevent *dst,
                           unsigned timeout_in_milliseconds)
{
  if (timeout_in_milliseconds == 0)
  {
    int res;
    while ((res = ndb_logevent_get_next2(h, dst, 60000))==0);
    return res;
  }

  SocketInputStream in(h->socket, timeout_in_milliseconds);

  /*
    Read log event header until header received
    or timeout expired. The MGM server will continusly
    send <PING>'s that should be ignored.
  */
  char buf[1024];
  const NDB_TICKS start = NdbTick_getCurrentTicks();
  while(1)
  {
    if (in.gets(buf,sizeof(buf)) == 0)
    {
      h->m_error= NDB_LEH_READ_ERROR;
      return -1;
    }
    if ( buf[0] == 0 )
    {
      // timed out
      return 0;
    }

    if ( strcmp("log event reply\n", buf) == 0 )
      break;

    if ( strcmp("<PING>\n", buf) )
      ndbout_c("skipped: %s", buf);

    if(in.timedout())
      return 0;

    const NDB_TICKS now = NdbTick_getCurrentTicks();
    if ((NdbTick_Elapsed(start,now)).milliSec() > timeout_in_milliseconds)
      return 0;

  };

  /* Read name-value pairs until empty new line */
  Properties p;
  while (1)
  {
    if (in.gets(buf,sizeof(buf)) == 0)
    {
      h->m_error= NDB_LEH_READ_ERROR;
      return -1;
    }
    if (in.timedout())
      return 0;

    if ( buf[0] == '\n' )
    {
      break;
    }
    if (insert_row(buf,p))
    {
      h->m_error= NDB_LEH_READ_ERROR;
      return -1;
    }
  }

  int i;
  const char *val;

  dst->type= (enum Ndb_logevent_type)-1;
  /* fill in header info from p*/
  for (i= 0; ndb_logevent_header[i].token; i++)
  {
    if ( p.get(ndb_logevent_header[i].token, &val) == 0 )
    {
      ndbout_c("missing: %s\n", ndb_logevent_header[i].token);
      h->m_error= NDB_LEH_MISSING_EVENT_SPECIFIER;
      return -1;
    }
    if ( memcpy_atoi((char *)dst+ndb_logevent_header[i].offset, val,
		     ndb_logevent_header[i].size) )
    {
      h->m_error= NDB_LEH_INTERNAL_ERROR;
      return -1;
    }
  }

  Uint32                             level;
  LogLevel::EventCategory            category;
  Logger::LoggerLevel                severity;
  EventLoggerBase::EventTextFunction text_fn;

  /* fill in rest of header info event_lookup */
  if (EventLoggerBase::event_lookup(dst->type,category,level,severity,text_fn))
  {
    ndbout_c("unknown type: %d\n", dst->type);
    h->m_error= NDB_LEH_UNKNOWN_EVENT_TYPE;
    return -1;
  }

  /* convert LogLevel::EventCategory enum values to ndb_mgm_event_category 
   * enum values and store in dst->category
   */
  enum ndb_mgm_event_category mgm_category;
  if(category == LogLevel::llInvalid) 
      mgm_category= NDB_MGM_ILLEGAL_EVENT_CATEGORY;
  else
      mgm_category= (enum ndb_mgm_event_category)(category + CFG_MIN_LOGLEVEL);

  dst->category= mgm_category;
  dst->severity= (enum ndb_mgm_event_severity)severity;
  dst->level=    level;

  /* fill in header info from p */
  for (i= 0; ndb_logevent_body[i].token; i++)
  {
    if ( ndb_logevent_body[i].type == dst->type )
      break;
  }

  if (ndb_logevent_body[i].token)
  {
    do {

      if ( p.get(ndb_logevent_body[i].token, &val) == 0 )
      {
        h->m_error= NDB_LEH_UNKNOWN_EVENT_VARIABLE;
        return -1;
      }
      if ( memcpy_atoi((char *)dst+ndb_logevent_body[i].offset, val,
                       ndb_logevent_body[i].size) )
      {
        h->m_error= NDB_LEH_INTERNAL_ERROR;
        return -1;
      }
    } while (ndb_logevent_body[++i].type == dst->type);
  }
  else
  {
    if (!p.get("data", &val))
    {
      h->m_error= NDB_LEH_UNKNOWN_EVENT_VARIABLE;
      return -1;
    }
    BaseString tmp(val);
    Vector<BaseString> list;
    tmp.split(list);
    for (unsigned j = 0; j<list.size(); j++)
    {
      dst->Data[j] = atoi(list[j].c_str());
    }
  }
  return 1;
}

extern "C"
int ndb_logevent_get_latest_error(const NdbLogEventHandle h)
{
  return h->m_error;
}

extern "C"
const char *ndb_logevent_get_latest_error_msg(const NdbLogEventHandle h)
{
  for (int i= 0; ndb_logevent_error_messages[i].msg; i++)
    if (ndb_logevent_error_messages[i].code == h->m_error)
      return ndb_logevent_error_messages[i].msg;
  return "<unknown error msg>";
}
