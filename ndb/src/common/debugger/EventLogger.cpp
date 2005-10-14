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

#include "EventLogger.hpp"

#include <NdbConfig.h>
#include <kernel/BlockNumbers.h>
#include <signaldata/ArbitSignalData.hpp>
#include <GrepEvent.hpp>
#include <NodeState.hpp>
#include <version.h>

//
// PUBLIC
//
EventLoggerBase::~EventLoggerBase()
{
  
}

/**
 * This matrix defines which event should be printed when
 *
 * threshold - is in range [0-15]
 * severity  - DEBUG to ALERT (Type of log message)
 */
const EventLoggerBase::EventRepLogLevelMatrix EventLoggerBase::matrix[] = {
  // CONNECTION
  { EventReport::Connected,           LogLevel::llConnection, 8, Logger::LL_INFO },
  { EventReport::Disconnected,        LogLevel::llConnection, 8, Logger::LL_ALERT },
  { EventReport::CommunicationClosed, LogLevel::llConnection, 8, Logger::LL_INFO },
  { EventReport::CommunicationOpened, LogLevel::llConnection, 8, Logger::LL_INFO },
  { EventReport::ConnectedApiVersion, LogLevel::llConnection, 8, Logger::LL_INFO },
  // CHECKPOINT
  { EventReport::GlobalCheckpointStarted, LogLevel::llCheckpoint,  9, Logger::LL_INFO },
  { EventReport::GlobalCheckpointCompleted,LogLevel::llCheckpoint,10, Logger::LL_INFO },
  { EventReport::LocalCheckpointStarted,  LogLevel::llCheckpoint,  7, Logger::LL_INFO },
  { EventReport::LocalCheckpointCompleted,LogLevel::llCheckpoint,  8, Logger::LL_INFO },
  { EventReport::LCPStoppedInCalcKeepGci, LogLevel::llCheckpoint,  0, Logger::LL_ALERT },
  { EventReport::LCPFragmentCompleted,    LogLevel::llCheckpoint, 11, Logger::LL_INFO },
  { EventReport::UndoLogBlocked,          LogLevel::llCheckpoint,  7, Logger::LL_INFO },

  // STARTUP
  { EventReport::NDBStartStarted,          LogLevel::llStartUp, 1, Logger::LL_INFO },
  { EventReport::NDBStartCompleted,        LogLevel::llStartUp, 1, Logger::LL_INFO },
  { EventReport::STTORRYRecieved,          LogLevel::llStartUp,15, Logger::LL_INFO },
  { EventReport::StartPhaseCompleted,      LogLevel::llStartUp, 4, Logger::LL_INFO },
  { EventReport::CM_REGCONF,               LogLevel::llStartUp, 3, Logger::LL_INFO },
  { EventReport::CM_REGREF,                LogLevel::llStartUp, 8, Logger::LL_INFO },
  { EventReport::FIND_NEIGHBOURS,          LogLevel::llStartUp, 8, Logger::LL_INFO },
  { EventReport::NDBStopStarted,           LogLevel::llStartUp, 1, Logger::LL_INFO },
  { EventReport::NDBStopAborted,           LogLevel::llStartUp, 1, Logger::LL_INFO },
  { EventReport::StartREDOLog,             LogLevel::llStartUp, 10, Logger::LL_INFO },
  { EventReport::StartLog,                 LogLevel::llStartUp, 10, Logger::LL_INFO },
  { EventReport::UNDORecordsExecuted,      LogLevel::llStartUp, 15, Logger::LL_INFO },
  
  // NODERESTART
  { EventReport::NR_CopyDict,            LogLevel::llNodeRestart,  8, Logger::LL_INFO },
  { EventReport::NR_CopyDistr,           LogLevel::llNodeRestart,  8, Logger::LL_INFO },
  { EventReport::NR_CopyFragsStarted,    LogLevel::llNodeRestart,  8, Logger::LL_INFO },
  { EventReport::NR_CopyFragDone,        LogLevel::llNodeRestart, 10, Logger::LL_INFO },
  { EventReport::NR_CopyFragsCompleted,  LogLevel::llNodeRestart,  8, Logger::LL_INFO },

  { EventReport::NodeFailCompleted,      LogLevel::llNodeRestart,  8, Logger::LL_ALERT},
  { EventReport::NODE_FAILREP,           LogLevel::llNodeRestart,  8, Logger::LL_ALERT},
  { EventReport::ArbitState,		 LogLevel::llNodeRestart,  6, Logger::LL_INFO },
  { EventReport::ArbitResult,		 LogLevel::llNodeRestart,  2, Logger::LL_ALERT},
  { EventReport::GCP_TakeoverStarted,    LogLevel::llNodeRestart,  7, Logger::LL_INFO },
  { EventReport::GCP_TakeoverCompleted,  LogLevel::llNodeRestart,  7, Logger::LL_INFO },
  { EventReport::LCP_TakeoverStarted,    LogLevel::llNodeRestart,  7, Logger::LL_INFO },
  { EventReport::LCP_TakeoverCompleted,  LogLevel::llNodeRestart,  7, Logger::LL_INFO },

  // STATISTIC
  { EventReport::TransReportCounters,     LogLevel::llStatistic, 8, Logger::LL_INFO },
  { EventReport::OperationReportCounters, LogLevel::llStatistic, 8, Logger::LL_INFO }, 
  { EventReport::TableCreated,            LogLevel::llStatistic, 7, Logger::LL_INFO },
  { EventReport::JobStatistic,            LogLevel::llStatistic, 9, Logger::LL_INFO },
  { EventReport::SendBytesStatistic,      LogLevel::llStatistic, 9, Logger::LL_INFO },
  { EventReport::ReceiveBytesStatistic,   LogLevel::llStatistic, 9, Logger::LL_INFO },
  { EventReport::MemoryUsage,             LogLevel::llStatistic, 5, Logger::LL_INFO },

  // ERROR
  { EventReport::TransporterError,   LogLevel::llError, 2, Logger::LL_ERROR   },
  { EventReport::TransporterWarning, LogLevel::llError, 8, Logger::LL_WARNING },
  { EventReport::MissedHeartbeat,    LogLevel::llError, 8, Logger::LL_WARNING },
  { EventReport::DeadDueToHeartbeat, LogLevel::llError, 8, Logger::LL_ALERT   },
  { EventReport::WarningEvent,       LogLevel::llError, 2, Logger::LL_WARNING },
  // INFO
  { EventReport::SentHeartbeat,     LogLevel::llInfo, 12, Logger::LL_INFO },
  { EventReport::CreateLogBytes,    LogLevel::llInfo, 11, Logger::LL_INFO },
  { EventReport::InfoEvent,         LogLevel::llInfo,  2, Logger::LL_INFO },

  //Single User
  { EventReport::SingleUser,  LogLevel::llInfo, 7, Logger::LL_INFO},

  // Backup
  { EventReport::BackupStarted, LogLevel::llBackup, 7, Logger::LL_INFO },
  { EventReport::BackupCompleted, LogLevel::llBackup, 7, Logger::LL_INFO },
  { EventReport::BackupFailedToStart, LogLevel::llBackup, 7, Logger::LL_ALERT},
  { EventReport::BackupAborted, LogLevel::llBackup, 7, Logger::LL_ALERT }
};

const Uint32 EventLoggerBase::matrixSize = sizeof(EventLoggerBase::matrix)/
                                       sizeof(EventRepLogLevelMatrix);

const char*
EventLogger::getText(char * m_text, size_t m_text_len, 
		     int type,
		     const Uint32* theData, NodeId nodeId)
{
  // TODO: Change the switch implementation...
  char theNodeId[32];
  if (nodeId != 0){
    BaseString::snprintf(theNodeId, 32, "Node %u: ", nodeId);
  } else {
    theNodeId[0] = 0;
  }

  EventReport::EventType eventType = (EventReport::EventType)type;
  switch (eventType){
  case EventReport::Connected:
    BaseString::snprintf(m_text, m_text_len, 
	       "%sNode %u Connected",
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::ConnectedApiVersion:
    BaseString::snprintf(m_text, m_text_len, 
	       "%sNode %u: API version %d.%d.%d",
	       theNodeId,
	       theData[1],
	       getMajor(theData[2]),
	       getMinor(theData[2]),
	       getBuild(theData[2]));
  break;
  case EventReport::Disconnected:
    BaseString::snprintf(m_text, m_text_len, 
	       "%sNode %u Disconnected", 
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::CommunicationClosed:
    //-----------------------------------------------------------------------
    // REPORT communication to node closed.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, m_text_len, 
	       "%sCommunication to Node %u closed", 
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::CommunicationOpened:
    //-----------------------------------------------------------------------
    // REPORT communication to node opened.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, m_text_len, 
	       "%sCommunication to Node %u opened", 
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::NDBStartStarted:
    //-----------------------------------------------------------------------
    // Start of NDB has been initiated.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, m_text_len, 
	       "%sStart initiated (version %d.%d.%d)", 
	       theNodeId ,
	       getMajor(theData[1]),
	       getMinor(theData[1]),
	       getBuild(theData[1]));
  break;
  case EventReport::NDBStopStarted:
    BaseString::snprintf(m_text, m_text_len,
	       "%s%s shutdown initiated", 
	       theNodeId, 
	       (theData[1] == 1 ? "Cluster" : "Node"));
  break;
  case EventReport::NDBStopAborted:
    BaseString::snprintf(m_text, m_text_len,
	       "%sNode shutdown aborted",
	       theNodeId);
  break;
  case EventReport::NDBStartCompleted:
    //-----------------------------------------------------------------------
    // Start of NDB has been completed.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, m_text_len, 
	       "%sStarted (version %d.%d.%d)", 
	       theNodeId ,
	       getMajor(theData[1]),
	       getMinor(theData[1]),
	       getBuild(theData[1]));

  break;
  case EventReport::STTORRYRecieved:
    //-----------------------------------------------------------------------
    // STTORRY recevied after restart finished.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, m_text_len, 
	       "%sSTTORRY received after restart finished",
	       theNodeId);
  break;
  case EventReport::StartPhaseCompleted:{
    //-----------------------------------------------------------------------
    // REPORT Start phase completed.
    //-----------------------------------------------------------------------
    const char * type = "<Unknown>";
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
    default:{
      BaseString::snprintf(m_text, m_text_len, 
		 "%sStart phase %u completed (unknown = %d)", 
		 theNodeId,
		 theData[1],
		 theData[2]);
      return m_text;
    }
    }
    BaseString::snprintf(m_text, m_text_len, 
	       "%sStart phase %u completed %s", 
	       theNodeId,
	       theData[1],
	       type);
    return m_text;
    break;
  }
  case EventReport::CM_REGCONF:
    BaseString::snprintf(m_text, m_text_len, 
	       "%sCM_REGCONF president = %u, own Node = %u, our dynamic id = %u"
	       , 
	       theNodeId,
	       theData[2], 
	       theData[1],
	       theData[3]);
  break;
  case EventReport::CM_REGREF:
  {
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
	       "%sCM_REGREF from Node %u to our Node %u. Cause = %s", 
	       theNodeId,
	       theData[2], 
	       theData[1], 
	       line);
  }
  break;
  case EventReport::FIND_NEIGHBOURS:
    //-----------------------------------------------------------------------
    // REPORT Node Restart copied a fragment.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sWe are Node %u with dynamic ID %u, our left neighbour "
	       "is Node %u, our right is Node %u", 
	       theNodeId,
	       theData[1], 
	       theData[4], 
	       theData[2], 
	       theData[3]);
  break;
  case EventReport::NodeFailCompleted:
    //-----------------------------------------------------------------------
    // REPORT Node failure phase completed.
    //-----------------------------------------------------------------------
    if (theData[1] == 0)
    {
      if (theData[3] != 0) {
        BaseString::snprintf(m_text, m_text_len, 
		 "%sNode %u completed failure of Node %u", 
		 theNodeId,
		 theData[3], 
		 theData[2]);
      } else {
        BaseString::snprintf(m_text, m_text_len, 
		 "%sAll nodes completed failure of Node %u", 
		 theNodeId,
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
		 "%sNode failure of %u %s completed", 
		 theNodeId,
		 theData[2], 
		 line);
    }
    break;
  case EventReport::NODE_FAILREP:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sNode %u has failed. The Node state at failure "
	       "was %u", 
	       theNodeId,
	       theData[1], 
	       theData[2]); 

  break;
  case EventReport::ArbitState:
    //-----------------------------------------------------------------------
    // REPORT arbitrator found or lost.
    //-----------------------------------------------------------------------
    { const ArbitSignalData* sd = (ArbitSignalData*)theData;
      char ticketText[ArbitTicket::TextLength + 1];
      char errText[ArbitCode::ErrTextLength + 1];
      const unsigned code = sd->code & 0xFFFF;
      const unsigned state = sd->code >> 16;
      switch (code) {
      case ArbitCode::ThreadStart:
        BaseString::snprintf(m_text, m_text_len,
          "%sPresident restarts arbitration thread [state=%u]",
          theNodeId, state);
        break;
      case ArbitCode::PrepPart2:
	sd->ticket.getText(ticketText, sizeof(ticketText));
	BaseString::snprintf(m_text, m_text_len,
	  "%sPrepare arbitrator node %u [ticket=%s]",
	  theNodeId, sd->node, ticketText);
	break;
      case ArbitCode::PrepAtrun:
	sd->ticket.getText(ticketText, sizeof(ticketText));
	BaseString::snprintf(m_text, m_text_len,
	  "%sReceive arbitrator node %u [ticket=%s]",
	  theNodeId, sd->node, ticketText);
	break;
      case ArbitCode::ApiStart:
	sd->ticket.getText(ticketText, sizeof(ticketText));
	BaseString::snprintf(m_text, m_text_len,
	  "%sStarted arbitrator node %u [ticket=%s]",
	  theNodeId, sd->node, ticketText);
	break;
      case ArbitCode::ApiFail:
	BaseString::snprintf(m_text, m_text_len,
	  "%sLost arbitrator node %u - process failure [state=%u]",
	  theNodeId, sd->node, state);
	break;
      case ArbitCode::ApiExit:
	BaseString::snprintf(m_text, m_text_len,
	  "%sLost arbitrator node %u - process exit [state=%u]",
	  theNodeId, sd->node, state);
	break;
      default:
	ArbitCode::getErrText(code, errText, sizeof(errText));
	BaseString::snprintf(m_text, m_text_len,
	  "%sLost arbitrator node %u - %s [state=%u]",
	  theNodeId, sd->node, errText, state);
	break;
      }
    }
    break;
  case EventReport::ArbitResult:
    //-----------------------------------------------------------------------
    // REPORT arbitration result (the failures may not reach us).
    //-----------------------------------------------------------------------
    { const ArbitSignalData* sd = (ArbitSignalData*)theData;
      char errText[ArbitCode::ErrTextLength + 1];
      const unsigned code = sd->code & 0xFFFF;
      const unsigned state = sd->code >> 16;
      switch (code) {
      case ArbitCode::LoseNodes:
	BaseString::snprintf(m_text, m_text_len,
	  "%sArbitration check lost - less than 1/2 nodes left",
	  theNodeId);
	break;
      case ArbitCode::WinNodes:
	BaseString::snprintf(m_text, m_text_len,
	  "%sArbitration check won - all node groups and more than 1/2 nodes left",
	  theNodeId);
	break;
      case ArbitCode::WinGroups:
	BaseString::snprintf(m_text, m_text_len,
	  "%sArbitration check won - node group majority",
	  theNodeId);
	break;
      case ArbitCode::LoseGroups:
	BaseString::snprintf(m_text, m_text_len,
	  "%sArbitration check lost - missing node group",
	  theNodeId);
	break;
      case ArbitCode::Partitioning:
	BaseString::snprintf(m_text, m_text_len,
	  "%sNetwork partitioning - arbitration required",
	  theNodeId);
	break;
      case ArbitCode::WinChoose:
	BaseString::snprintf(m_text, m_text_len,
	  "%sArbitration won - positive reply from node %u",
	  theNodeId, sd->node);
	break;
      case ArbitCode::LoseChoose:
	BaseString::snprintf(m_text, m_text_len,
	  "%sArbitration lost - negative reply from node %u",
	  theNodeId, sd->node);
	break;
      case ArbitCode::LoseNorun:
	BaseString::snprintf(m_text, m_text_len,
	  "%sNetwork partitioning - no arbitrator available",
	  theNodeId);
	break;
      case ArbitCode::LoseNocfg:
	BaseString::snprintf(m_text, m_text_len,
	  "%sNetwork partitioning - no arbitrator configured",
	  theNodeId);
	break;
      default:
	ArbitCode::getErrText(code, errText, sizeof(errText));
	BaseString::snprintf(m_text, m_text_len,
	  "%sArbitration failure - %s [state=%u]",
	  theNodeId, errText, state);
	break;
      }
    }
    break;
  case EventReport::GlobalCheckpointStarted:
    //-----------------------------------------------------------------------
    // This event reports that a global checkpoint has been started and this
    // node is the master of this global checkpoint.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sGlobal checkpoint %u started", 
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::GlobalCheckpointCompleted:
    //-----------------------------------------------------------------------
    // This event reports that a global checkpoint has been completed on this
    // node and the node is the master of this global checkpoint.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, m_text_len, 
	       "%sGlobal checkpoint %u completed", 
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::LocalCheckpointStarted:
    //-----------------------------------------------------------------------
    // This event reports that a local checkpoint has been started and this
    // node is the master of this local checkpoint.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sLocal checkpoint %u started. "
	       "Keep GCI = %u oldest restorable GCI = %u", 
	       theNodeId,
	       theData[1], 
	       theData[2], 
	       theData[3]);
    break;
  case EventReport::LocalCheckpointCompleted:
    //-----------------------------------------------------------------------
    // This event reports that a local checkpoint has been completed on this
    // node and the node is the master of this local checkpoint.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sLocal checkpoint %u completed", 
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::TableCreated:
    //-----------------------------------------------------------------------
    // This event reports that a table has been created.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, m_text_len, 
	       "%sTable with ID =  %u created", 
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::LCPStoppedInCalcKeepGci:
    if (theData[1] == 0)
      BaseString::snprintf(m_text, m_text_len, 
		 "%sLocal Checkpoint stopped in CALCULATED_KEEP_GCI",
		 theNodeId);
    break;
  case EventReport::NR_CopyDict:
    //-----------------------------------------------------------------------
    // REPORT Node Restart completed copy of dictionary information.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sNode restart completed copy of dictionary information",
	       theNodeId);
    break;
  case EventReport::NR_CopyDistr:
    //-----------------------------------------------------------------------
    // REPORT Node Restart completed copy of distribution information.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sNode restart completed copy of distribution information",
	       theNodeId);
    break;
  case EventReport::NR_CopyFragsStarted:
    //-----------------------------------------------------------------------
    // REPORT Node Restart is starting to copy the fragments.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sNode restart starting to copy the fragments "
	       "to Node %u", 
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::NR_CopyFragDone:
    //-----------------------------------------------------------------------
    // REPORT Node Restart copied a fragment.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sTable ID = %u, fragment ID = %u have been copied "
	       "to Node %u", 
	       theNodeId,
	       theData[2], 
	       theData[3], 
	       theData[1]);
  break;
  case EventReport::NR_CopyFragsCompleted:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sNode restart completed copying the fragments "
	       "to Node %u", 
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::LCPFragmentCompleted:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sTable ID = %u, fragment ID = %u has completed LCP "
	       "on Node %u maxGciStarted: %d maxGciCompleted: %d", 
	       theNodeId,
	       theData[2], 
	       theData[3], 
	       theData[1],
	       theData[4],
	       theData[5]);
    break;
  case EventReport::TransReportCounters:
    // -------------------------------------------------------------------  
    // Report information about transaction activity once per 10 seconds.
    // ------------------------------------------------------------------- 
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sTrans. Count = %u, Commit Count = %u, "
	       "Read Count = %u, Simple Read Count = %u,\n"
	       "Write Count = %u, AttrInfo Count = %u, "
	       "Concurrent Operations = %u, Abort Count = %u\n"
	       " Scans: %u Range scans: %u", 
	       theNodeId,
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
    break;
  case EventReport::OperationReportCounters:
    BaseString::snprintf(m_text, m_text_len,
	       "%sOperations=%u",
	       theNodeId, 
	       theData[1]);
    break;
  case EventReport::UndoLogBlocked:
    //-----------------------------------------------------------------------
    // REPORT Undo Logging blocked due to buffer near to overflow.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sACC Blocked %u and TUP Blocked %u times last second",
	       theNodeId,
	       theData[1],
	       theData[2]);
  break;
  case EventReport::TransporterError:
  case EventReport::TransporterWarning:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sTransporter to node %d reported error 0x%x",
	       theNodeId,
	       theData[1],
	       theData[2]);
  break;
  case EventReport::MissedHeartbeat:
    //-----------------------------------------------------------------------
    // REPORT Undo Logging blocked due to buffer near to overflow.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sNode %d missed heartbeat %d",
	       theNodeId,
	       theData[1],
	       theData[2]);
  break;
  case EventReport::DeadDueToHeartbeat:
    //-----------------------------------------------------------------------
    // REPORT Undo Logging blocked due to buffer near to overflow.
    //-----------------------------------------------------------------------
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sNode %d declared dead due to missed heartbeat",
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::JobStatistic:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sMean loop Counter in doJob last 8192 times = %u",
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::SendBytesStatistic:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sMean send size to Node = %d last 4096 sends = %u bytes",
	       theNodeId,
	       theData[1],
	       theData[2]);
    break;
  case EventReport::ReceiveBytesStatistic:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sMean receive size to Node = %d last 4096 sends = %u bytes",
	       theNodeId,
	       theData[1],
	       theData[2]);
  break;
  case EventReport::SentHeartbeat:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sNode Sent Heartbeat to node = %d",
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::CreateLogBytes:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sLog part %u, log file %u, MB %u",
	       theNodeId,
	       theData[1],
	       theData[2],
	       theData[3]);
  break;
  case EventReport::StartLog:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sLog part %u, start MB %u, stop MB %u, last GCI, log exec %u",
	       theNodeId,
	       theData[1],
	       theData[2],
	       theData[3],
	       theData[4]);
  break;
  case EventReport::StartREDOLog:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sNode: %d StartLog: [GCI Keep: %d LastCompleted: %d NewestRestorable: %d]",
	       theNodeId,
	       theData[1],
	       theData[2],
	       theData[3],
	       theData[4]);
  break;
  case EventReport::UNDORecordsExecuted:{
    const char* line = "";
    if (theData[1] == DBTUP){
      line = "DBTUP";
    }else if (theData[1] == DBACC){
      line = "DBACC";
    }
    
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%s UNDO %s %d [%d %d %d %d %d %d %d %d %d]",
	       theNodeId,
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
    break;
  case EventReport::InfoEvent:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%s%s",
	       theNodeId,
	       (char *)&theData[1]);
  break;
  case EventReport::WarningEvent:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%s%s",
	       theNodeId,
	       (char *)&theData[1]);
  break;
  case EventReport::GCP_TakeoverStarted:
    BaseString::snprintf(m_text,
	       m_text_len,
	       "%sGCP Take over started", theNodeId);
  break;
  case EventReport::GCP_TakeoverCompleted:
    BaseString::snprintf(m_text,
	       m_text_len,
	       "%sGCP Take over completed", theNodeId);
  break;
  case EventReport::LCP_TakeoverStarted:
    BaseString::snprintf(m_text,
	       m_text_len,
	       "%sLCP Take over started", theNodeId);
  break;
  case EventReport::LCP_TakeoverCompleted:
    BaseString::snprintf(m_text,
	       m_text_len,
	       "%sLCP Take over completed (state = %d)", 
	       theNodeId, theData[1]);
  break;
  case EventReport::MemoryUsage:{
    const int gth = theData[1];
    const int size = theData[2];
    const int used = theData[3];
    const int total = theData[4];
    const int block = theData[5];
    const int percent = (used*100)/total;
    
    BaseString::snprintf(m_text, m_text_len,
	       "%s%s usage %s %d%s(%d %dK pages of total %d)",
	       theNodeId,
	       (block==DBACC ? "Index" : (block == DBTUP ?"Data":"<unknown>")),
	       (gth == 0 ? "is" : (gth > 0 ? "increased to" : "decreased to")),
	       percent, "%",
	       used, size/1024, total
	       );
    break;
  }
  case EventReport::SingleUser : 
  {   
    switch (theData[1])
    {
    case 0:
      BaseString::snprintf(m_text, m_text_len, 
			   "%sEntering single user mode", theNodeId);
      break;
    case 1:
      BaseString::snprintf(m_text, m_text_len, 
			   "%sEntered single user mode %d", theNodeId, theData[2]);
      break;
    case 2:
      BaseString::snprintf(m_text, m_text_len, 
			   "%sExiting single user mode", theNodeId);
      break;
    default:
      BaseString::snprintf(m_text, m_text_len, 
			   "%sUnknown single user report %d", theNodeId, theData[1]);
      break;
    }
    break;
  }      
  case EventReport::BackupStarted:
    BaseString::snprintf(m_text,
	       m_text_len,
	       "%sBackup %d started from node %d", 
	       theNodeId, theData[2], refToNode(theData[1]));
  break;
  case EventReport::BackupFailedToStart:
    BaseString::snprintf(m_text,
	       m_text_len,
	       "%sBackup request from %d failed to start. Error: %d", 
	       theNodeId, refToNode(theData[1]), theData[2]);
  break;
  case EventReport::BackupCompleted:
    BaseString::snprintf(m_text,
	       m_text_len,
	       "%sBackup %u started from node %u completed\n" 
	       " StartGCP: %u StopGCP: %u\n"
	       " #Records: %u #LogRecords: %u\n"
	       " Data: %u bytes Log: %u bytes",
	       theNodeId, theData[2], refToNode(theData[1]),
	       theData[3], theData[4], theData[6], theData[8],
	       theData[5], theData[7]);
  break;
  case EventReport::BackupAborted:
    BaseString::snprintf(m_text,
	       m_text_len,
	       "%sBackup %d started from %d has been aborted. Error: %d",
	       theNodeId, 
	       theData[2], 
	       refToNode(theData[1]), 
	       theData[3]);
  break;
  default:
    BaseString::snprintf(m_text, 
	       m_text_len, 
	       "%sUnknown event: %d",
	       theNodeId,
	       theData[0]);
  
  }
  return m_text;
}

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

static NdbOut&
operator<<(NdbOut& out, const LogLevel & ll)
{
  out << "[LogLevel: ";
  for(size_t i = 0; i<LogLevel::LOGLEVEL_CATEGORIES; i++)
    out << ll.getLogLevel((LogLevel::EventCategory)i) << " ";
  out << "]";
  return out;
}

int
EventLoggerBase::event_lookup(int eventType,
			      LogLevel::EventCategory &cat,
			      Uint32 &threshold, 
			      Logger::LoggerLevel &severity)
{
  for(unsigned i = 0; i<EventLoggerBase::matrixSize; i++){
    if(EventLoggerBase::matrix[i].eventType == eventType){
      cat = EventLoggerBase::matrix[i].eventCategory;
      threshold = EventLoggerBase::matrix[i].threshold;
      severity = EventLoggerBase::matrix[i].severity;
      return 0;
    }
  }
  return 1;
}

void 
EventLogger::log(int eventType, const Uint32* theData, NodeId nodeId,
		 const LogLevel* ll)
{
  Uint32 threshold = 0;
  Logger::LoggerLevel severity = Logger::LL_WARNING;
  LogLevel::EventCategory cat= LogLevel::llInvalid;

  DBUG_ENTER("EventLogger::log");
  DBUG_PRINT("enter",("eventType=%d, nodeid=%d", eventType, nodeId));

  if (EventLoggerBase::event_lookup(eventType,cat,threshold,severity))
    DBUG_VOID_RETURN;
  
  Uint32 set = ll?ll->getLogLevel(cat) : m_logLevel.getLogLevel(cat);
  DBUG_PRINT("info",("threshold=%d, set=%d", threshold, set));
  if (ll)
    DBUG_PRINT("info",("m_logLevel.getLogLevel=%d", m_logLevel.getLogLevel(cat)));
  if (threshold <= set){
    switch (severity){
    case Logger::LL_ALERT:
      alert(EventLogger::getText(m_text, sizeof(m_text), 
				 eventType, theData, nodeId));
      break;
      
    case Logger::LL_CRITICAL:
      critical(EventLogger::getText(m_text, sizeof(m_text), 
				    eventType, theData, nodeId));
      break;
      
    case Logger::LL_WARNING:
      warning(EventLogger::getText(m_text, sizeof(m_text), 
				   eventType, theData, nodeId));
      break;
      
    case Logger::LL_ERROR:
      error(EventLogger::getText(m_text, sizeof(m_text), 
				 eventType, theData, nodeId));
      break;
      
    case Logger::LL_INFO:
      info(EventLogger::getText(m_text, sizeof(m_text), 
				eventType, theData, nodeId));
      break;
      
    case Logger::LL_DEBUG:
      debug(EventLogger::getText(m_text, sizeof(m_text), 
				 eventType, theData, nodeId));
      break;
      
    default:
      info(EventLogger::getText(m_text, sizeof(m_text), 
				eventType, theData, nodeId));
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

//
// PRIVATE
//
