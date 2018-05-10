/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <ndbd_exit_codes.h>

typedef struct ErrStruct {
   int faultId;
   ndbd_exit_classification classification;
   const char* text;
} ErrStruct;

/**
 * Shorter names in table below
 */

#define XST_S ndbd_exit_st_success
#define XST_U ndbd_exit_st_unknown
#define XST_P ndbd_exit_st_permanent
#define XST_R ndbd_exit_st_temporary
#define XST_I ndbd_exit_st_filesystem_error

#define XNE ndbd_exit_cl_none
#define XUE ndbd_exit_cl_unknown
#define XIE ndbd_exit_cl_internal_error
#define XCE ndbd_exit_cl_configuration_error
#define XAE ndbd_exit_cl_arbitration_error
#define XRE ndbd_exit_cl_restart_error
#define XCR ndbd_exit_cl_resource_configuration_error
#define XFF ndbd_exit_cl_filesystem_full_error
#define XFI ndbd_exit_cl_filesystem_inconsistency_error
#define XFL ndbd_exit_cl_filesystem_limit

static const ErrStruct errArray[] =
{
   {NDBD_EXIT_GENERIC, XRE, "Generic error"},
   {NDBD_EXIT_PRGERR, XIE, "Assertion"},
   {NDBD_EXIT_NODE_NOT_IN_CONFIG, XCE,
    "node id in the configuration has the wrong type, (i.e. not an NDB node)"},
   {NDBD_EXIT_SYSTEM_ERROR, XIE,
    "System error, node killed during node restart by other node"},
   {NDBD_EXIT_INDEX_NOTINRANGE, XIE, "Array index out of range"},
   {NDBD_EXIT_ARBIT_SHUTDOWN, XAE, "Node lost connection to other nodes and "
    "can not form a unpartitioned cluster, please investigate if there are "
    "error(s) on other node(s)"},
   {NDBD_EXIT_PARTITIONED_SHUTDOWN, XAE, "Partitioned cluster detected. "
    "Please check if cluster is already running"},
   {NDBD_EXIT_NODE_DECLARED_DEAD, XAE,
    "Node declared dead. See error log for details"},
   {NDBD_EXIT_POINTER_NOTINRANGE, XIE, "Pointer too large"},
   {NDBD_EXIT_SR_OTHERNODEFAILED, XRE, "Another node failed during system "
    "restart, please investigate error(s) on other node(s)"},
   {NDBD_EXIT_NODE_NOT_DEAD, XRE, "Internal node state conflict, "
    "most probably resolved by restarting node again"},
   {NDBD_EXIT_SR_REDOLOG, XFI, "Error while reading the REDO log"},
   {NDBD_EXIT_SR_SCHEMAFILE, XFI, "Error while reading the schema file"},
   /* Currently unused? */
   {2311, XIE, "Conflict when selecting restart type"},
   {NDBD_EXIT_NO_MORE_UNDOLOG, XCR,
    "No more free UNDO log, increase UndoIndexBuffer"},
   {NDBD_EXIT_SR_UNDOLOG, XFI,
    "Error while reading the datapages and UNDO log"},
   {NDBD_EXIT_SINGLE_USER_MODE, XRE, "Data node is not allowed to get added "
    "to the cluster while it is in single user mode"},
   {NDBD_EXIT_MEMALLOC, XCE, "Memory allocation failure, "
    "please decrease some configuration parameters"},
   {NDBD_EXIT_BLOCK_JBUFCONGESTION, XIE, "Job buffer congestion"},
   {NDBD_EXIT_TIME_QUEUE_ZERO, XIE, "Error in zero time queue"},
   {NDBD_EXIT_TIME_QUEUE_SHORT, XIE, "Error in short time queue"},
   {NDBD_EXIT_TIME_QUEUE_LONG, XIE, "Error in long time queue"},
   {NDBD_EXIT_TIME_QUEUE_DELAY, XIE, "Error in time queue, too long delay"},
   {NDBD_EXIT_TIME_QUEUE_INDEX, XIE, "Time queue index out of range"},
   {NDBD_EXIT_BLOCK_BNR_ZERO, XIE, "Send signal error"},
   {NDBD_EXIT_WRONG_PRIO_LEVEL, XIE, "Wrong priority level when sending signal"},
   {NDBD_EXIT_NDBREQUIRE, XIE, "Internal program error (failed ndbrequire)"},
   {NDBD_EXIT_NDBASSERT, XIE, "Internal program error (failed ndbassert)"},
   {NDBD_EXIT_ERROR_INSERT, XNE, "Error insert executed" },
   /* this error message is complemented by additional info when generated */
   {NDBD_EXIT_INVALID_CONFIG, XCE,
    "Invalid configuration received from Management Server"},

   {NDBD_EXIT_RESOURCE_ALLOC_ERROR, XCE,
    "Resource allocation error, please review the configuration"},

   {NDBD_EXIT_NO_MORE_REDOLOG, XCR,
    "Fatal error due to end of REDO log. Increase NoOfFragmentLogFiles or FragmentLogFileSize" },

   /* this error message is complemented by additional info when
      generated, such as signal, and text
   */
   {NDBD_EXIT_OS_SIGNAL_RECEIVED, XIE, "Error OS signal received"},

   {NDBD_EXIT_SR_RESTARTCONFLICT, XRE,
    "Partial system restart causing conflicting file systems"},
   
   /* VM */
   {NDBD_EXIT_OUT_OF_LONG_SIGNAL_MEMORY,    XCR,
    "Signal lost, out of long signal memory, please increase LongMessageBuffer"},
   {NDBD_EXIT_WATCHDOG_TERMINATE, XIE, "WatchDog terminate, internal error "
    "or massive overload on the machine running this node"},
   {NDBD_EXIT_SIGNAL_LOST_SEND_BUFFER_FULL, XCR,
    "Signal lost, out of send buffer memory, please increase SendBufferMemory or lower the load"},
   {NDBD_EXIT_SIGNAL_LOST,    XIE, "Signal lost (unknown reason)"},
   {NDBD_EXIT_ILLEGAL_SIGNAL, XIE,
    "Illegal signal (version mismatch a possibility)"},
   {NDBD_EXIT_CONNECTION_SETUP_FAILED, XCE, "Connection setup failed"},

   /* Ndbcntr */
   {NDBD_EXIT_RESTART_TIMEOUT, XCE,
    "Total restart time too long, consider increasing StartFailureTimeout "
    "or investigate error(s) on other node(s)"},
   {NDBD_EXIT_RESTART_DURING_SHUTDOWN, XRE,
    "Node started while node shutdown in progress. "
    "Please wait until shutdown complete before starting node"},
   {NDBD_EXIT_UPGRADE_INITIAL_REQUIRED, XRE,
    "Node upgrade requires initial restart to rebuild filesystem. "
    "Please retry with --initial or reconsider."},

   /* DIH */
   {NDBD_EXIT_MAX_CRASHED_REPLICAS, XFL,
    "Too many crashed replicas (8 consecutive node restart failures)"},
   {NDBD_EXIT_MASTER_FAILURE_DURING_NR, XRE,
    "Unhandled master failure during node restart"},
   {NDBD_EXIT_LOST_NODE_GROUP, XAE,
    "All nodes in a node group are unavailable"},
   {NDBD_EXIT_NO_RESTORABLE_REPLICA, XFI,
    "Unable to find a restorable replica"},

   /* ACC */
   {NDBD_EXIT_SR_OUT_OF_INDEXMEMORY, XCR,
    "Out of index memory during system restart, please increase DataMemory"},

   /* TUP */
   {NDBD_EXIT_SR_OUT_OF_DATAMEMORY, XCR,
    "Out of data memory during system restart, please increase DataMemory"},

   /* LQH */
   {NDBD_EXIT_LCP_SCAN_WATCHDOG_FAIL, XIE,
    "LCP fragment scan watchdog detected a problem.  Please report a bug."},

   /* Ndbfs error messages */
   /* Most codes will have additional info, such as OS error code */
   {NDBD_EXIT_AFS_NOPATH,       XIE, "No file system path"},
   {2802,                       XIE, "Channel is full"},
   {2803,                       XIE, "No more threads"},
   {NDBD_EXIT_AFS_PARAMETER,    XIE, "Bad parameter"},
   {NDBD_EXIT_AFS_INVALIDPATH,  XCE, "Illegal file system path"},
   {NDBD_EXIT_AFS_MAXOPEN,      XCR,
    "Max number of open files exceeded, please increase MaxNoOfOpenFiles"},
   {NDBD_EXIT_AFS_ALREADY_OPEN, XIE, "File has already been opened"},

   {NDBD_EXIT_AFS_ENVIRONMENT           , XIE, "Environment error using file"},
   {NDBD_EXIT_AFS_TEMP_NO_ACCESS        , XIE, "Temporary on access to file"},
   {NDBD_EXIT_AFS_DISK_FULL             , XFF, "The file system is full"},
   {NDBD_EXIT_AFS_PERMISSION_DENIED     , XCE, "Received permission denied for file"},
   {NDBD_EXIT_AFS_INVALID_PARAM         , XCE, "Invalid parameter for file"},
   {NDBD_EXIT_AFS_UNKNOWN               , XIE, "Unknown file system error"},
   {NDBD_EXIT_AFS_NO_MORE_RESOURCES     , XIE,
    "System reports no more file system resources"},
   {NDBD_EXIT_AFS_NO_SUCH_FILE          , XFI, "File not found"},
   {NDBD_EXIT_AFS_READ_UNDERFLOW        , XFI, "Read underflow"},
   
   {NDBD_EXIT_INVALID_LCP_FILE, XFI, "Invalid LCP" },
   {NDBD_EXIT_INSUFFICENT_NODES, XRE, "Insufficent nodes for system restart" },
   
   {NDBD_EXIT_UNSUPPORTED_VERSION, XRE, "Unsupported version" },
   
   {NDBD_EXIT_RESTORE_SCHEMA, XCR, "Failure to restore schema" },
   
   {NDBD_EXIT_GRACEFUL_SHUTDOWN_ERROR, XNE,
    "Graceful shutdown not 100% possible due to mixed ndbd versions" },

   /* Sentinel */
   {0, XUE,
    "No message slogan found (please report a bug if you get this error code)"}
};

typedef struct StatusExitMessage {
  ndbd_exit_status status;
  const char * message;
} StatusExitMessage;

typedef struct StatusExitClassification {
  ndbd_exit_status status;
  ndbd_exit_classification classification;
  const char * message;
} StatusExitClassification;

/**
 * Mapping between classification and status
 */
static
const
StatusExitMessage StatusExitMessageMapping[] = {
  { XST_S, "Success"},
  { XST_U ,"Unknown"},
  { XST_P, "Permanent error, external action needed"},
  { XST_R, "Temporary error, restart node"},
  { XST_I, "Ndbd file system error, restart node initial"}
};

static
const
int NbExitStatus = sizeof(StatusExitMessageMapping)/sizeof(StatusExitMessage);

static
const
int errArraysize = sizeof(errArray) / sizeof(ErrStruct);

static
const
StatusExitClassification StatusExitClassificationMapping[] = {
  { XST_S, XNE, "No error"},
  { XST_U, XUE, "Unknown"},
  { XST_R, XIE, "Internal error, programming error or missing error message, "
                "please report a bug"},
  { XST_P, XCE, "Configuration error"},
  { XST_R, XAE, "Arbitration error"},
  { XST_R, XRE, "Restart error"},
  { XST_P, XCR, "Resource configuration error"},
  { XST_P, XFF, "File system full"},
  { XST_I, XFI, "Ndbd file system inconsistency error, please report a bug"},
  { XST_I, XFL, "Ndbd file system limit exceeded"}
};

static const int NbExitClassification =
sizeof(StatusExitClassificationMapping)/sizeof(StatusExitClassification);

int ndbd_exit_code_get_next(int index, int* exit_code,
                            const char** status_msg,
                            const char** class_msg,
                            const char** error_msg)
{
  ndbd_exit_classification cl;
  ndbd_exit_status st;

  if (index >= errArraysize)
  {
    // No more ndbd exit codes
    return -1;
  }

  *exit_code = errArray[index].faultId;
  *error_msg = ndbd_exit_message(errArray[index].faultId, &cl);
  *class_msg = ndbd_exit_classification_message(cl, &st);
  *status_msg = ndbd_exit_status_message(st);

  return index + 1;
}

const char *ndbd_exit_message(int faultId, ndbd_exit_classification *cl)
{
  int i = 0;
  while (errArray[i].faultId != faultId && errArray[i].faultId != 0)
    i++;
  *cl = errArray[i].classification;
  return errArray[i].text;
}

static const char* empty_xstring = "";

const
char *ndbd_exit_classification_message(ndbd_exit_classification classification,
                                     ndbd_exit_status *status)
{
  int i;
  for (i= 0; i < NbExitClassification; i++)
  {
    if (StatusExitClassificationMapping[i].classification == classification)
    {
      *status = StatusExitClassificationMapping[i].status;
      return StatusExitClassificationMapping[i].message;
    }
  }
  *status = XST_U;
  return empty_xstring;
}

const char *ndbd_exit_status_message(ndbd_exit_status status)
{
  int i;
  for (i= 0; i < NbExitStatus; i++)
    if (StatusExitMessageMapping[i].status == status)
      return StatusExitMessageMapping[i].message;
  return empty_xstring;
}

int ndbd_exit_string(int err_no, char *str, unsigned int size)
{
  ndbd_exit_classification cl;
  ndbd_exit_status st;
  const char *msg = ndbd_exit_message(err_no, &cl);
  if (msg[0] != '\0' && cl != XUE)
  {
    const char *cl_msg = ndbd_exit_classification_message(cl, &st);
    const char *st_msg = ndbd_exit_status_message(st);

    int len = snprintf(str, size-1, "%s: %s: %s", msg, st_msg, cl_msg);
    if (len >= 0 && (unsigned)(len) > size-2)
    {
      len = size-2;
    }

    return len;
  }
  return -1;
}
