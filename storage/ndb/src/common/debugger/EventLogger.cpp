/* Copyright (C) 2003 MySQL AB

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

#include <EventLogger.hpp>
#include <TransporterCallback.hpp>

#include <NdbConfig.h>
#include <kernel/BlockNumbers.h>
#include <signaldata/ArbitSignalData.hpp>
#include <NodeState.hpp>
#include <version.h>

#include <ndbd_exit_codes.h>

//
// PUBLIC
//
EventLoggerBase::~EventLoggerBase()
{
  
}

#define QQQQ char *m_text, size_t m_text_len, const Uint32* theData

void getTextConnected(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Node %u Connected",
		       theData[1]);
}
void getTextConnectedApiVersion(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Node %u: API version %d.%d.%d",
		       theData[1],
		       getMajor(theData[2]),
		       getMinor(theData[2]),
		       getBuild(theData[2]));
}
void getTextDisconnected(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Node %u Disconnected", 
		       theData[1]);
}
void getTextCommunicationClosed(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT communication to node closed.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Communication to Node %u closed", 
		       theData[1]);
}
void getTextCommunicationOpened(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT communication to node opened.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Communication to Node %u opened", 
		       theData[1]);
}
void getTextNDBStartStarted(QQQQ) {
  //-----------------------------------------------------------------------
  // Start of NDB has been initiated.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Start initiated (version %d.%d.%d)", 
		       getMajor(theData[1]),
		       getMinor(theData[1]),
		       getBuild(theData[1]));
}
void getTextNDBStopStarted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len,
		       "%s shutdown initiated", 
		       (theData[1] == 1 ? "Cluster" : "Node"));
}
void getRestartAction(Uint32 action, BaseString &str)
{
  if (action == 0)
    return;
  str.appfmt(", restarting");
  if (action & 2)
    str.appfmt(", no start");
  if (action & 4)
    str.appfmt(", initial");
}
void getTextNDBStopCompleted(QQQQ) {
  BaseString action_str("");
  BaseString signum_str("");
  getRestartAction(theData[1], action_str);
  if (theData[2])
    signum_str.appfmt(" Initiated by signal %d.", theData[2]);
  BaseString::snprintf(m_text, m_text_len,
		       "Node shutdown completed%s.%s",
		       action_str.c_str(),
		       signum_str.c_str());
}
void getTextNDBStopForced(QQQQ) {
  BaseString action_str("");
  BaseString reason_str("");
  BaseString sphase_str("");
  int signum        = theData[2];
  int error         = theData[3];
  int sphase        = theData[4];
  int extra         = theData[5];
  getRestartAction(theData[1],action_str);
  if (signum)
    reason_str.appfmt(" Initiated by signal %d.", signum);
  if (error)
  {
    ndbd_exit_classification cl;
    ndbd_exit_status st;
    const char *msg = ndbd_exit_message(error, &cl);
    const char *cl_msg = ndbd_exit_classification_message(cl, &st);
    const char *st_msg = ndbd_exit_status_message(st);
    reason_str.appfmt(" Caused by error %d: \'%s(%s). %s\'.",
		      error, msg, cl_msg, st_msg);
    if (extra != 0)
      reason_str.appfmt(" (extra info %d)", extra);
  }
  if (sphase < 255)
    sphase_str.appfmt(" Occured during startphase %u.", sphase);
  BaseString::snprintf(m_text, m_text_len,
		       "Forced node shutdown completed%s.%s%s",
		       action_str.c_str(), sphase_str.c_str(),
		       reason_str.c_str());
}
void getTextNDBStopAborted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len,
		       "Node shutdown aborted");
}
void getTextNDBStartCompleted(QQQQ) {
  //-----------------------------------------------------------------------
  // Start of NDB has been completed.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Started (version %d.%d.%d)", 
		       getMajor(theData[1]),
		       getMinor(theData[1]),
		       getBuild(theData[1]));
}
void getTextSTTORRYRecieved(QQQQ) {
  //-----------------------------------------------------------------------
  // STTORRY recevied after restart finished.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "STTORRY received after restart finished");
}
void getTextStartPhaseCompleted(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT Start phase completed.
  //-----------------------------------------------------------------------
  const char *type = "<Unknown>";
  switch((NodeState::StartType)theData[2]){
  case NodeState::ST_INITIAL_START:
    type = "(initial start)";
    break;
  case NodeState::ST_SYSTEM_RESTART:
    type = "(system restart)";
    break;
  case NodeState::ST_NODE_RESTART:
    type = "(node restart)";
    break;
  case NodeState::ST_INITIAL_NODE_RESTART:
    type = "(initial node restart)";
    break;
  case NodeState::ST_ILLEGAL_TYPE:
    type = "";
    break;
  default:
    BaseString::snprintf(m_text, m_text_len, 
			 "Start phase %u completed (unknown = %d)", 
			 theData[1],
			 theData[2]);
    return;
  }
  BaseString::snprintf(m_text, m_text_len, 
		       "Start phase %u completed %s", 
		       theData[1],
		       type);
}
void getTextCM_REGCONF(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "CM_REGCONF president = %u, own Node = %u, our dynamic id = %u",
		       theData[2], 
		       theData[1],
		       theData[3]);
}
void getTextCM_REGREF(QQQQ) {
  const char* line = "";
  switch (theData[3]) {
  case 0:
    line = "Busy";
    break;
  case 1:
    line = "Election with wait = false";
    break;
  case 2:
    line = "Election with wait = false";
    break;
  case 3:
    line = "Not president";
    break;
  case 4:
    line = "Election without selecting new candidate";
    break;
  default:
    line = "No such cause";
    break;
  }//switch

  BaseString::snprintf(m_text, m_text_len, 
		       "CM_REGREF from Node %u to our Node %u. Cause = %s", 
		       theData[2], 
		       theData[1], 
		       line);
}
void getTextFIND_NEIGHBOURS(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT Node Restart copied a fragment.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "We are Node %u with dynamic ID %u, our left neighbour "
		       "is Node %u, our right is Node %u", 
		       theData[1], 
		       theData[4], 
		       theData[2], 
		       theData[3]);
}
void getTextNodeFailCompleted(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT Node failure phase completed.
  //-----------------------------------------------------------------------
  if (theData[1] == 0)
  {
    if (theData[3] != 0) {
      BaseString::snprintf(m_text, m_text_len, 
			   "Node %u completed failure of Node %u", 
			   theData[3], 
			   theData[2]);
    } else {
      BaseString::snprintf(m_text, m_text_len, 
			   "All nodes completed failure of Node %u", 
			   theData[2]);
    }//if      
  } else {
    const char* line = "";
    if (theData[1] == DBTC){
      line = "DBTC";
    }else if (theData[1] == DBDICT){
      line = "DBDICT";
    }else if (theData[1] == DBDIH){
      line = "DBDIH";
    }else if (theData[1] == DBLQH){
      line = "DBLQH";
    }
    BaseString::snprintf(m_text, m_text_len, 
			 "Node failure of %u %s completed", 
			 theData[2], 
			 line);
  }
}
void getTextNODE_FAILREP(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Node %u has failed. The Node state at failure "
		       "was %u", 
		       theData[1], 
		       theData[2]); 
}
void getTextArbitState(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT arbitrator found or lost.
  //-----------------------------------------------------------------------
  {
    const ArbitSignalData* sd = (ArbitSignalData*)theData;
    char ticketText[ArbitTicket::TextLength + 1];
    char errText[ArbitCode::ErrTextLength + 1];
    const unsigned code = sd->code & 0xFFFF;
    const unsigned state = sd->code >> 16;
    switch (code) {
    case ArbitCode::ThreadStart:
      BaseString::snprintf(m_text, m_text_len,
			   "President restarts arbitration thread [state=%u]",
			   state);
      break;
    case ArbitCode::PrepPart2:
      sd->ticket.getText(ticketText, sizeof(ticketText));
      BaseString::snprintf(m_text, m_text_len,
			   "Prepare arbitrator node %u [ticket=%s]",
			   sd->node, ticketText);
      break;
    case ArbitCode::PrepAtrun:
      sd->ticket.getText(ticketText, sizeof(ticketText));
      BaseString::snprintf(m_text, m_text_len,
			   "Receive arbitrator node %u [ticket=%s]",
			   sd->node, ticketText);
      break;
    case ArbitCode::ApiStart:
      sd->ticket.getText(ticketText, sizeof(ticketText));
      BaseString::snprintf(m_text, m_text_len,
			   "Started arbitrator node %u [ticket=%s]",
			   sd->node, ticketText);
      break;
    case ArbitCode::ApiFail:
      BaseString::snprintf(m_text, m_text_len,
			   "Lost arbitrator node %u - process failure [state=%u]",
			   sd->node, state);
      break;
    case ArbitCode::ApiExit:
      BaseString::snprintf(m_text, m_text_len,
			   "Lost arbitrator node %u - process exit [state=%u]",
			   sd->node, state);
      break;
    default:
      ArbitCode::getErrText(code, errText, sizeof(errText));
      BaseString::snprintf(m_text, m_text_len,
			   "Lost arbitrator node %u - %s [state=%u]",
			   sd->node, errText, state);
      break;
    }
  }
}

void getTextArbitResult(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT arbitration result (the failures may not reach us).
  //-----------------------------------------------------------------------
  {
    const ArbitSignalData* sd = (ArbitSignalData*)theData;
    char errText[ArbitCode::ErrTextLength + 1];
    const unsigned code = sd->code & 0xFFFF;
    const unsigned state = sd->code >> 16;
    switch (code) {
    case ArbitCode::LoseNodes:
      BaseString::snprintf(m_text, m_text_len,
			   "Arbitration check lost - less than 1/2 nodes left");
      break;
    case ArbitCode::WinNodes:
      BaseString::snprintf(m_text, m_text_len,
			   "Arbitration check won - all node groups and more than 1/2 nodes left");
      break;
    case ArbitCode::WinGroups:
      BaseString::snprintf(m_text, m_text_len,
			   "Arbitration check won - node group majority");
      break;
    case ArbitCode::LoseGroups:
      BaseString::snprintf(m_text, m_text_len,
			   "Arbitration check lost - missing node group");
      break;
    case ArbitCode::Partitioning:
      BaseString::snprintf(m_text, m_text_len,
			   "Network partitioning - arbitration required");
      break;
    case ArbitCode::WinChoose:
      BaseString::snprintf(m_text, m_text_len,
			   "Arbitration won - positive reply from node %u",
			   sd->node);
      break;
    case ArbitCode::LoseChoose:
      BaseString::snprintf(m_text, m_text_len,
			   "Arbitration lost - negative reply from node %u",
			   sd->node);
      break;
    case ArbitCode::LoseNorun:
      BaseString::snprintf(m_text, m_text_len,
			   "Network partitioning - no arbitrator available");
      break;
    case ArbitCode::LoseNocfg:
      BaseString::snprintf(m_text, m_text_len,
			   "Network partitioning - no arbitrator configured");
      break;
    default:
      ArbitCode::getErrText(code, errText, sizeof(errText));
      BaseString::snprintf(m_text, m_text_len,
			   "Arbitration failure - %s [state=%u]",
			   errText, state);
      break;
    }
  }
}
void getTextGlobalCheckpointStarted(QQQQ) {
  //-----------------------------------------------------------------------
  // This event reports that a global checkpoint has been started and this
  // node is the master of this global checkpoint.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Global checkpoint %u started", 
		       theData[1]);
}
void getTextGlobalCheckpointCompleted(QQQQ) {
  //-----------------------------------------------------------------------
  // This event reports that a global checkpoint has been completed on this
  // node and the node is the master of this global checkpoint.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Global checkpoint %u completed", 
		       theData[1]);
}
void getTextLocalCheckpointStarted(QQQQ) {
  //-----------------------------------------------------------------------
  // This event reports that a local checkpoint has been started and this
  // node is the master of this local checkpoint.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Local checkpoint %u started. "
		       "Keep GCI = %u oldest restorable GCI = %u", 
		       theData[1], 
		       theData[2], 
		       theData[3]);
}
void getTextLocalCheckpointCompleted(QQQQ) {
  //-----------------------------------------------------------------------
  // This event reports that a local checkpoint has been completed on this
  // node and the node is the master of this local checkpoint.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Local checkpoint %u completed", 
		       theData[1]);
}
void getTextTableCreated(QQQQ) {
  //-----------------------------------------------------------------------
  // This event reports that a table has been created.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Table with ID =  %u created", 
		       theData[1]);
}
/* STRANGE */
void getTextLCPStoppedInCalcKeepGci(QQQQ) {
  if (theData[1] == 0)
    BaseString::snprintf(m_text, m_text_len, 
			 "Local Checkpoint stopped in CALCULATED_KEEP_GCI");
}
void getTextNR_CopyDict(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT Node Restart completed copy of dictionary information.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Node restart completed copy of dictionary information");
}
void getTextNR_CopyDistr(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT Node Restart completed copy of distribution information.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Node restart completed copy of distribution information");
}
void getTextNR_CopyFragsStarted(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT Node Restart is starting to copy the fragments.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Node restart starting to copy the fragments "
		       "to Node %u", 
		       theData[1]);
}
void getTextNR_CopyFragDone(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT Node Restart copied a fragment.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Table ID = %u, fragment ID = %u have been copied "
		       "to Node %u", 
		       theData[2], 
		       theData[3], 
		       theData[1]);
}
void getTextNR_CopyFragsCompleted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Node restart completed copying the fragments "
		       "to Node %u", 
		       theData[1]);
}
void getTextLCPFragmentCompleted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Table ID = %u, fragment ID = %u has completed LCP "
		       "on Node %u maxGciStarted: %d maxGciCompleted: %d", 
		       theData[2], 
		       theData[3], 
		       theData[1],
		       theData[4],
		       theData[5]);
}
void getTextTransReportCounters(QQQQ) {
  // -------------------------------------------------------------------  
  // Report information about transaction activity once per 10 seconds.
  // ------------------------------------------------------------------- 
  BaseString::snprintf(m_text, m_text_len, 
		       "Trans. Count = %u, Commit Count = %u, "
		       "Read Count = %u, Simple Read Count = %u, "
		       "Write Count = %u, AttrInfo Count = %u, "
		       "Concurrent Operations = %u, Abort Count = %u"
		       " Scans = %u Range scans = %u", 
		       theData[1], 
		       theData[2], 
		       theData[3], 
		       theData[4],
		       theData[5], 
		       theData[6], 
		       theData[7], 
		       theData[8],
		       theData[9],
		       theData[10]);
}
void getTextOperationReportCounters(QQQQ) {
  BaseString::snprintf(m_text, m_text_len,
		       "Operations=%u",
		       theData[1]);
}
void getTextUndoLogBlocked(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT Undo Logging blocked due to buffer near to overflow.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "ACC Blocked %u and TUP Blocked %u times last second",
		       theData[1],
		       theData[2]);
}

void getTextTransporterError(QQQQ) {
  struct myTransporterError{
    Uint32 errorNum;
    char   errorString[256];
  };
  int i = 0;
  int lenth = 0;
  static const struct myTransporterError TransporterErrorString[]=
  {
    //TE_NO_ERROR = 0
    {TE_NO_ERROR,"No error"},
    //TE_ERROR_CLOSING_SOCKET = 0x1
    {TE_ERROR_CLOSING_SOCKET,"Error found during closing of socket"},
    //TE_ERROR_IN_SELECT_BEFORE_ACCEPT = 0x2
    {TE_ERROR_IN_SELECT_BEFORE_ACCEPT,"Error found before accept. The transporter will retry"},
    //TE_INVALID_MESSAGE_LENGTH = 0x3 | TE_DO_DISCONNECT
    {TE_INVALID_MESSAGE_LENGTH,"Error found in message (invalid message length)"},
    //TE_INVALID_CHECKSUM = 0x4 | TE_DO_DISCONNECT
    {TE_INVALID_CHECKSUM,"Error found in message (checksum)"},
    //TE_COULD_NOT_CREATE_SOCKET = 0x5
    {TE_COULD_NOT_CREATE_SOCKET,"Error found while creating socket(can't create socket)"},
    //TE_COULD_NOT_BIND_SOCKET = 0x6
    {TE_COULD_NOT_BIND_SOCKET,"Error found while binding server socket"},
    //TE_LISTEN_FAILED = 0x7
    {TE_LISTEN_FAILED,"Error found while listening to server socket"},
    //TE_ACCEPT_RETURN_ERROR = 0x8
    {TE_ACCEPT_RETURN_ERROR,"Error found during accept(accept return error)"},
    //TE_SHM_DISCONNECT = 0xb | TE_DO_DISCONNECT
    {TE_SHM_DISCONNECT,"The remote node has disconnected"},
    //TE_SHM_IPC_STAT = 0xc | TE_DO_DISCONNECT
    {TE_SHM_IPC_STAT,"Unable to check shm segment"},
    //TE_SHM_UNABLE_TO_CREATE_SEGMENT = 0xd
    {TE_SHM_UNABLE_TO_CREATE_SEGMENT,"Unable to create shm segment"},
    //TE_SHM_UNABLE_TO_ATTACH_SEGMENT = 0xe
    {TE_SHM_UNABLE_TO_ATTACH_SEGMENT,"Unable to attach shm segment"},
    //TE_SHM_UNABLE_TO_REMOVE_SEGMENT = 0xf
    {TE_SHM_UNABLE_TO_REMOVE_SEGMENT,"Unable to remove shm segment"},
    //TE_TOO_SMALL_SIGID = 0x10
    {TE_TOO_SMALL_SIGID,"Sig ID too small"},
    //TE_TOO_LARGE_SIGID = 0x11
    {TE_TOO_LARGE_SIGID,"Sig ID too large"},
    //TE_WAIT_STACK_FULL = 0x12 | TE_DO_DISCONNECT
    {TE_WAIT_STACK_FULL,"Wait stack was full"},
    //TE_RECEIVE_BUFFER_FULL = 0x13 | TE_DO_DISCONNECT
    {TE_RECEIVE_BUFFER_FULL,"Receive buffer was full"},
    //TE_SIGNAL_LOST_SEND_BUFFER_FULL = 0x14 | TE_DO_DISCONNECT
    {TE_SIGNAL_LOST_SEND_BUFFER_FULL,"Send buffer was full,and trying to force send fails"},
    //TE_SIGNAL_LOST = 0x15
    {TE_SIGNAL_LOST,"Send failed for unknown reason(signal lost)"},
    //TE_SEND_BUFFER_FULL = 0x16
    {TE_SEND_BUFFER_FULL,"The send buffer was full, but sleeping for a while solved"},
    //TE_SCI_LINK_ERROR = 0x0017
    {TE_SCI_LINK_ERROR,"There is no link from this node to the switch"},
    //TE_SCI_UNABLE_TO_START_SEQUENCE = 0x18 | TE_DO_DISCONNECT
    {TE_SCI_UNABLE_TO_START_SEQUENCE,"Could not start a sequence, because system resources are exumed or no sequence has been created"},
    //TE_SCI_UNABLE_TO_REMOVE_SEQUENCE = 0x19 | TE_DO_DISCONNECT
    {TE_SCI_UNABLE_TO_REMOVE_SEQUENCE,"Could not remove a sequence"},
    //TE_SCI_UNABLE_TO_CREATE_SEQUENCE = 0x1a | TE_DO_DISCONNECT
    {TE_SCI_UNABLE_TO_CREATE_SEQUENCE,"Could not create a sequence, because system resources are exempted. Must reboot"},
    //TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR = 0x1b | TE_DO_DISCONNECT
    {TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR,"Tried to send data on redundant link but failed"},
    //TE_SCI_CANNOT_INIT_LOCALSEGMENT = 0x1c | TE_DO_DISCONNECT
    {TE_SCI_CANNOT_INIT_LOCALSEGMENT,"Cannot initialize local segment"},
    //TE_SCI_CANNOT_MAP_REMOTESEGMENT = 0x1d | TE_DO_DISCONNEC
    {TE_SCI_CANNOT_MAP_REMOTESEGMENT,"Cannot map remote segment"},
    //TE_SCI_UNABLE_TO_UNMAP_SEGMENT = 0x1e | TE_DO_DISCONNECT
    {TE_SCI_UNABLE_TO_UNMAP_SEGMENT,"Cannot free the resources used by this segment (step 1)"},
    //TE_SCI_UNABLE_TO_REMOVE_SEGMENT = 0x1f  | TE_DO_DISCONNEC
    {TE_SCI_UNABLE_TO_REMOVE_SEGMENT,"Cannot free the resources used by this segment (step 2)"},
    //TE_SCI_UNABLE_TO_DISCONNECT_SEGMENT = 0x20 | TE_DO_DISCONNECT
    {TE_SCI_UNABLE_TO_DISCONNECT_SEGMENT,"Cannot disconnect from a remote segment"},
    //TE_SHM_IPC_PERMANENT = 0x21
    {TE_SHM_IPC_PERMANENT,"Shm ipc Permanent error"},
    //TE_SCI_UNABLE_TO_CLOSE_CHANNEL = 0x22
    {TE_SCI_UNABLE_TO_CLOSE_CHANNEL,"Unable to close the sci channel and the resources allocated"}
  };

  lenth = sizeof(TransporterErrorString)/sizeof(struct myTransporterError);
  for(i=0; i<lenth; i++)
  {
    if(theData[2] == (Uint32) TransporterErrorString[i].errorNum)
    {
      BaseString::snprintf(m_text, m_text_len,
                           "Transporter to node %d reported error 0x%x: %s",
                           theData[1],
                           theData[2],
                           TransporterErrorString[i].errorString);
      break;
    }
  }
  if(i == lenth)
    BaseString::snprintf(m_text, m_text_len,   
                         "Transporter to node %d reported error 0x%x: unknown error",
                         theData[1],
                         theData[2]);
}
void getTextTransporterWarning(QQQQ) {
  getTextTransporterError(m_text, m_text_len, theData);
}
void getTextMissedHeartbeat(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT Undo Logging blocked due to buffer near to overflow.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Node %d missed heartbeat %d",
		       theData[1],
		       theData[2]);
}
void getTextDeadDueToHeartbeat(QQQQ) {
  //-----------------------------------------------------------------------
  // REPORT Undo Logging blocked due to buffer near to overflow.
  //-----------------------------------------------------------------------
  BaseString::snprintf(m_text, m_text_len, 
		       "Node %d declared dead due to missed heartbeat",
		       theData[1]);
}
void getTextJobStatistic(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Mean loop Counter in doJob last 8192 times = %u",
		       theData[1]);
}
void getTextSendBytesStatistic(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Mean send size to Node = %d last 4096 sends = %u bytes",
		       theData[1],
		       theData[2]);
}
void getTextReceiveBytesStatistic(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Mean receive size to Node = %d last 4096 sends = %u bytes",
		       theData[1],
		       theData[2]);
}
void getTextSentHeartbeat(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Node Sent Heartbeat to node = %d",
		       theData[1]);
}
void getTextCreateLogBytes(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Log part %u, log file %u, MB %u",
		       theData[1],
		       theData[2],
		       theData[3]);
}
void getTextStartLog(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Log part %u, start MB %u, stop MB %u, last GCI, log exec %u",
		       theData[1],
		       theData[2],
		       theData[3],
		       theData[4]);
}
void getTextStartREDOLog(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Node: %d StartLog: [GCI Keep: %d LastCompleted: %d NewestRestorable: %d]",
		       theData[1],
		       theData[2],
		       theData[3],
		       theData[4]);
}
void getTextUNDORecordsExecuted(QQQQ) {
  const char* line = "";
  if (theData[1] == DBTUP){
    line = "DBTUP";
  }else if (theData[1] == DBACC){
    line = "DBACC";
  }
    
  BaseString::snprintf(m_text, m_text_len, 
		       " UNDO %s %d [%d %d %d %d %d %d %d %d %d]",
		       line,
		       theData[2],
		       theData[3],
		       theData[4],
		       theData[5],
		       theData[6],
		       theData[7],
		       theData[8],
		       theData[9],
		       theData[10],
		       theData[11]);
}
void getTextInfoEvent(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, (char *)&theData[1]);
}
const char bytes_unit[]= "B";
const char kbytes_unit[]= "KB";
const char mbytes_unit[]= "MB";
static void convert_unit(unsigned &data, const char *&unit)
{
  if (data < 16*1024)
  {
    unit= bytes_unit;
    return;
  }
  if (data < 16*1024*1024)
  {
    data= (data+1023)/1024;
    unit= kbytes_unit;
    return;
  }
  data= (data+1024*1024-1)/(1024*1024);
  unit= mbytes_unit;  
}

void getTextEventBufferStatus(QQQQ) {
  unsigned used= theData[1], alloc= theData[2], max_= theData[3];
  const char *used_unit, *alloc_unit, *max_unit;
  convert_unit(used, used_unit);
  convert_unit(alloc, alloc_unit);
  convert_unit(max_, max_unit);
  BaseString::snprintf(m_text, m_text_len,
		       "Event buffer status: used=%d%s(%d%) alloc=%d%s(%d%) "
		       "max=%d%s apply_gci=%lld latest_gci=%lld",
		       used, used_unit,
		       theData[2] ? (Uint32)((((Uint64)theData[1])*100)/theData[2]) : 0,
		       alloc, alloc_unit,
		       theData[3] ? (Uint32)((((Uint64)theData[2])*100)/theData[3]) : 0,
		       max_, max_unit,
		       theData[4]+(((Uint64)theData[5])<<32),
		       theData[6]+(((Uint64)theData[7])<<32));
}
void getTextWarningEvent(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, (char *)&theData[1]);
}
void getTextGCP_TakeoverStarted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, "GCP Take over started");
}
void getTextGCP_TakeoverCompleted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, "GCP Take over completed");
}
void getTextLCP_TakeoverStarted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, "LCP Take over started");
}
void getTextLCP_TakeoverCompleted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len,
		       "LCP Take over completed (state = %d)", 
		       theData[1]);
}
void getTextMemoryUsage(QQQQ) {
  const int gth = theData[1];
  const int size = theData[2];
  const int used = theData[3];
  const int total = theData[4];
  const int block = theData[5];
  const int percent = total ? (used*100)/total : 0;
  
  BaseString::snprintf(m_text, m_text_len,
		       "%s usage %s %d%s(%d %dK pages of total %d)",
		       (block==DBACC ? "Index" : (block == DBTUP ?"Data":"<unknown>")),
		       (gth == 0 ? "is" : (gth > 0 ? "increased to" : "decreased to")),
		       percent, "%",
		       used, size/1024, total
		       );
}

void getTextBackupStarted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Backup %d started from node %d", 
		       theData[2], refToNode(theData[1]));
}
void getTextBackupFailedToStart(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Backup request from %d failed to start. Error: %d", 
		       refToNode(theData[1]), theData[2]);
}
void getTextBackupCompleted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Backup %u started from node %u completed." 
		       " StartGCP: %u StopGCP: %u"
		       " #Records: %u #LogRecords: %u"
		       " Data: %u bytes Log: %u bytes",
		       theData[2], refToNode(theData[1]),
		       theData[3], theData[4], theData[6], theData[8],
		       theData[5], theData[7]);
}
void getTextBackupAborted(QQQQ) {
  BaseString::snprintf(m_text, m_text_len, 
		       "Backup %d started from %d has been aborted. Error: %d",
		       theData[2], 
		       refToNode(theData[1]), 
		       theData[3]);
}

void getTextSingleUser(QQQQ) {
  switch (theData[1])
  {
  case 0:
    BaseString::snprintf(m_text, m_text_len, "Entering single user mode");
    break;
  case 1:
    BaseString::snprintf(m_text, m_text_len,
			 "Entered single user mode "
			 "Node %d has exclusive access", theData[2]);
    break;
  case 2:
    BaseString::snprintf(m_text, m_text_len,"Exiting single user mode");
    break;
  default:
    BaseString::snprintf(m_text, m_text_len,
			 "Unknown single user report %d", theData[1]);
    break;
  }
}

void getTextStartReport(QQQQ) {
  Uint32 time = theData[2];
  Uint32 sz = theData[3];
  char mask1[100];
  char mask2[100];
  char mask3[100];
  char mask4[100];
  BitmaskImpl::getText(sz, theData + 4 + (0 * sz), mask1);
  BitmaskImpl::getText(sz, theData + 4 + (1 * sz), mask2);
  BitmaskImpl::getText(sz, theData + 4 + (2 * sz), mask3);
  BitmaskImpl::getText(sz, theData + 4 + (3 * sz), mask4);
  switch(theData[1]){
  case 1: // Wait initial
    BaseString::snprintf
      (m_text, m_text_len,
       "Initial start, waiting for %s to connect, "
       " nodes [ all: %s connected: %s no-wait: %s ]",
       mask4, mask1, mask2, mask3);
    break;
  case 2: // Wait partial
    BaseString::snprintf
      (m_text, m_text_len,
       "Waiting until nodes: %s connects, "
       "nodes [ all: %s connected: %s no-wait: %s ]",
       mask4, mask1, mask2, mask3);
    break;
  case 3: // Wait partial timeout
    BaseString::snprintf
      (m_text, m_text_len,
       "Waiting %u sec for nodes %s to connect, "
       "nodes [ all: %s connected: %s no-wait: %s ]",
       
       time, mask4, mask1, mask2, mask3);
    break;
  case 4: // Wait partioned
    BaseString::snprintf
      (m_text, m_text_len,
       "Waiting for non partitioned start, "
       "nodes [ all: %s connected: %s missing: %s no-wait: %s ]",
       
       mask1, mask2, mask4, mask3);
    break;
  case 5:
    BaseString::snprintf
      (m_text, m_text_len,
       "Waiting %u sec for non partitioned start, "
       "nodes [ all: %s connected: %s missing: %s no-wait: %s ]",
       
       time, mask1, mask2, mask4, mask3);
    break;
  case 0x8000: // Do initial
    BaseString::snprintf
      (m_text, m_text_len,
       "Initial start with nodes %s [ missing: %s no-wait: %s ]",
       mask2, mask4, mask3);
    break;
  case 0x8001: // Do start
    BaseString::snprintf
      (m_text, m_text_len,
       "Start with all nodes %s",
       mask2);
    break;
  case 0x8002: // Do partial
    BaseString::snprintf
      (m_text, m_text_len,
       "Start with nodes %s [ missing: %s no-wait: %s ]",
       mask2, mask4, mask3);
    break;
  case 0x8003: // Do partioned
    BaseString::snprintf
      (m_text, m_text_len,
       "Start potentially partitioned with nodes %s "
       " [ missing: %s no-wait: %s ]",
       mask2, mask4, mask3);
    break;
  default:
    BaseString::snprintf
      (m_text, m_text_len,
       "Unknown startreport: 0x%x [ %s %s %s %s ]", 
       theData[1],
       mask1, mask2, mask3, mask4);
  }
}

#if 0
BaseString::snprintf(m_text, 
		     m_text_len, 
		     "Unknown event: %d",
		     theData[0]);
#endif

/**
 * This matrix defines which event should be printed when
 *
 * threshold - is in range [0-15]
 * severity  - DEBUG to ALERT (Type of log message)
 */

#define ROW(a,b,c,d) \
{ NDB_LE_ ## a, b, c, d, getText ## a}

const EventLoggerBase::EventRepLogLevelMatrix EventLoggerBase::matrix[] = {
  // CONNECTION
  ROW(Connected,               LogLevel::llConnection,  8, Logger::LL_INFO ),
  ROW(Disconnected,            LogLevel::llConnection,  8, Logger::LL_ALERT ),
  ROW(CommunicationClosed,     LogLevel::llConnection,  8, Logger::LL_INFO ),
  ROW(CommunicationOpened,     LogLevel::llConnection,  8, Logger::LL_INFO ),
  ROW(ConnectedApiVersion,     LogLevel::llConnection,  8, Logger::LL_INFO ),
  // CHECKPOINT
  ROW(GlobalCheckpointStarted, LogLevel::llCheckpoint,  9, Logger::LL_INFO ),
  ROW(GlobalCheckpointCompleted,LogLevel::llCheckpoint,10, Logger::LL_INFO ),
  ROW(LocalCheckpointStarted,  LogLevel::llCheckpoint,  7, Logger::LL_INFO ),
  ROW(LocalCheckpointCompleted,LogLevel::llCheckpoint,  8, Logger::LL_INFO ),
  ROW(LCPStoppedInCalcKeepGci, LogLevel::llCheckpoint,  0, Logger::LL_ALERT ),
  ROW(LCPFragmentCompleted,    LogLevel::llCheckpoint, 11, Logger::LL_INFO ),
  ROW(UndoLogBlocked,          LogLevel::llCheckpoint,  7, Logger::LL_INFO ),

  // STARTUP
  ROW(NDBStartStarted,         LogLevel::llStartUp,     1, Logger::LL_INFO ),
  ROW(NDBStartCompleted,       LogLevel::llStartUp,     1, Logger::LL_INFO ),
  ROW(STTORRYRecieved,         LogLevel::llStartUp,    15, Logger::LL_INFO ),
  ROW(StartPhaseCompleted,     LogLevel::llStartUp,     4, Logger::LL_INFO ),
  ROW(CM_REGCONF,              LogLevel::llStartUp,     3, Logger::LL_INFO ),
  ROW(CM_REGREF,               LogLevel::llStartUp,     8, Logger::LL_INFO ),
  ROW(FIND_NEIGHBOURS,         LogLevel::llStartUp,     8, Logger::LL_INFO ),
  ROW(NDBStopStarted,          LogLevel::llStartUp,     1, Logger::LL_INFO ),
  ROW(NDBStopCompleted,        LogLevel::llStartUp,     1, Logger::LL_INFO ),
  ROW(NDBStopForced,           LogLevel::llStartUp,     1, Logger::LL_ALERT ),
  ROW(NDBStopAborted,          LogLevel::llStartUp,     1, Logger::LL_INFO ),
  ROW(StartREDOLog,            LogLevel::llStartUp,     4, Logger::LL_INFO ),
  ROW(StartLog,                LogLevel::llStartUp,    10, Logger::LL_INFO ),
  ROW(UNDORecordsExecuted,     LogLevel::llStartUp,    15, Logger::LL_INFO ),
  ROW(StartReport,             LogLevel::llStartUp,     4, Logger::LL_INFO ),
  
  // NODERESTART
  ROW(NR_CopyDict,             LogLevel::llNodeRestart, 8, Logger::LL_INFO ),
  ROW(NR_CopyDistr,            LogLevel::llNodeRestart, 8, Logger::LL_INFO ),
  ROW(NR_CopyFragsStarted,     LogLevel::llNodeRestart, 8, Logger::LL_INFO ),
  ROW(NR_CopyFragDone,         LogLevel::llNodeRestart,10, Logger::LL_INFO ),
  ROW(NR_CopyFragsCompleted,   LogLevel::llNodeRestart, 8, Logger::LL_INFO ),

  ROW(NodeFailCompleted,       LogLevel::llNodeRestart, 8, Logger::LL_ALERT),
  ROW(NODE_FAILREP,            LogLevel::llNodeRestart, 8, Logger::LL_ALERT),
  ROW(ArbitState,		LogLevel::llNodeRestart, 6, Logger::LL_INFO ),
  ROW(ArbitResult,	        LogLevel::llNodeRestart, 2, Logger::LL_ALERT),
  ROW(GCP_TakeoverStarted,     LogLevel::llNodeRestart, 7, Logger::LL_INFO ),
  ROW(GCP_TakeoverCompleted,   LogLevel::llNodeRestart, 7, Logger::LL_INFO ),
  ROW(LCP_TakeoverStarted,     LogLevel::llNodeRestart, 7, Logger::LL_INFO ),
  ROW(LCP_TakeoverCompleted,   LogLevel::llNodeRestart, 7, Logger::LL_INFO ),

  // STATISTIC
  ROW(TransReportCounters,     LogLevel::llStatistic,   8, Logger::LL_INFO ),
  ROW(OperationReportCounters, LogLevel::llStatistic,   8, Logger::LL_INFO ), 
  ROW(TableCreated,            LogLevel::llStatistic,   7, Logger::LL_INFO ),
  ROW(JobStatistic,            LogLevel::llStatistic,   9, Logger::LL_INFO ),
  ROW(SendBytesStatistic,      LogLevel::llStatistic,   9, Logger::LL_INFO ),
  ROW(ReceiveBytesStatistic,   LogLevel::llStatistic,   9, Logger::LL_INFO ),
  ROW(MemoryUsage,             LogLevel::llStatistic,   5, Logger::LL_INFO ),

  // ERROR
  ROW(TransporterError,        LogLevel::llError,  2, Logger::LL_ERROR   ),
  ROW(TransporterWarning,      LogLevel::llError,  8, Logger::LL_WARNING ),
  ROW(MissedHeartbeat,         LogLevel::llError,  8, Logger::LL_WARNING ),
  ROW(DeadDueToHeartbeat,      LogLevel::llError,  8, Logger::LL_ALERT   ),
  ROW(WarningEvent,            LogLevel::llError,  2, Logger::LL_WARNING ),
  // INFO
  ROW(SentHeartbeat,           LogLevel::llInfo,  12, Logger::LL_INFO ),
  ROW(CreateLogBytes,          LogLevel::llInfo,  11, Logger::LL_INFO ),
  ROW(InfoEvent,               LogLevel::llInfo,   2, Logger::LL_INFO ),
  ROW(EventBufferStatus,       LogLevel::llInfo,   7, Logger::LL_INFO ),

  //Single User
  ROW(SingleUser,              LogLevel::llInfo,   7, Logger::LL_INFO ),

  // Backup
  ROW(BackupStarted,           LogLevel::llBackup, 7, Logger::LL_INFO ),
  ROW(BackupCompleted,         LogLevel::llBackup, 7, Logger::LL_INFO ),
  ROW(BackupFailedToStart,     LogLevel::llBackup, 7, Logger::LL_ALERT),
  ROW(BackupAborted,           LogLevel::llBackup, 7, Logger::LL_ALERT )
};

const Uint32 EventLoggerBase::matrixSize=
sizeof(EventLoggerBase::matrix)/sizeof(EventRepLogLevelMatrix);

EventLogger::EventLogger() : m_filterLevel(15)
{
  setCategory("EventLogger");
  enable(Logger::LL_INFO, Logger::LL_ALERT); 
}

EventLogger::~EventLogger()
{
}

bool
EventLogger::open(const char* logFileName, int maxNoFiles, long maxFileSize, 
		  unsigned int maxLogEntries)
{
  return addHandler(new FileLogHandler(logFileName, maxNoFiles, maxFileSize, 
				       maxLogEntries));
}

void
EventLogger::close()
{
  removeAllHandlers();
}

#ifdef NOT_USED

static NdbOut&
operator<<(NdbOut& out, const LogLevel & ll)
{
  out << "[LogLevel: ";
  for(size_t i = 0; i<LogLevel::LOGLEVEL_CATEGORIES; i++)
    out << ll.getLogLevel((LogLevel::EventCategory)i) << " ";
  out << "]";
  return out;
}
#endif

int
EventLoggerBase::event_lookup(int eventType,
			      LogLevel::EventCategory &cat,
			      Uint32 &threshold, 
			      Logger::LoggerLevel &severity,
			      EventTextFunction &textF)
{
  for(unsigned i = 0; i<EventLoggerBase::matrixSize; i++){
    if(EventLoggerBase::matrix[i].eventType == eventType){
      cat = EventLoggerBase::matrix[i].eventCategory;
      threshold = EventLoggerBase::matrix[i].threshold;
      severity = EventLoggerBase::matrix[i].severity;
      textF= EventLoggerBase::matrix[i].textF;
      return 0;
    }
  }
  return 1;
}

const char*
EventLogger::getText(char * dst, size_t dst_len,
		     EventTextFunction textF,
		     const Uint32* theData, NodeId nodeId )
{
  int pos= 0;
  if (nodeId != 0)
  {
    BaseString::snprintf(dst, dst_len, "Node %u: ", nodeId);
    pos= strlen(dst);
  }
  if (dst_len-pos > 0)
    textF(dst+pos,dst_len-pos,theData);
  return dst;
}

void 
EventLogger::log(int eventType, const Uint32* theData, NodeId nodeId,
		 const LogLevel* ll)
{
  Uint32 threshold = 0;
  Logger::LoggerLevel severity = Logger::LL_WARNING;
  LogLevel::EventCategory cat= LogLevel::llInvalid;
  EventTextFunction textF;
  char log_text[MAX_TEXT_LENGTH];

  DBUG_ENTER("EventLogger::log");
  DBUG_PRINT("enter",("eventType=%d, nodeid=%d", eventType, nodeId));

  if (EventLoggerBase::event_lookup(eventType,cat,threshold,severity,textF))
    DBUG_VOID_RETURN;
  
  Uint32 set = ll?ll->getLogLevel(cat) : m_logLevel.getLogLevel(cat);
  DBUG_PRINT("info",("threshold=%d, set=%d", threshold, set));
  if (ll)
    DBUG_PRINT("info",("m_logLevel.getLogLevel=%d", m_logLevel.getLogLevel(cat)));

  if (threshold <= set){
    getText(log_text,sizeof(log_text),textF,theData,nodeId);

    switch (severity){
    case Logger::LL_ALERT:
      alert(log_text);
      break;
    case Logger::LL_CRITICAL:
      critical(log_text); 
      break;
    case Logger::LL_WARNING:
      warning(log_text); 
      break;
    case Logger::LL_ERROR:
      error(log_text); 
      break;
    case Logger::LL_INFO:
      info(log_text); 
      break;
    case Logger::LL_DEBUG:
      debug(log_text); 
      break;
    default:
      info(log_text); 
      break;
    }
  } // if (..
  DBUG_VOID_RETURN;
}

int
EventLogger::getFilterLevel() const
{
  return m_filterLevel;
}

void 
EventLogger::setFilterLevel(int filterLevel)
{
  m_filterLevel = filterLevel;
}
