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

  //Global replication
  { EventReport::GrepSubscriptionInfo,  LogLevel::llGrep, 7, Logger::LL_INFO},
  { EventReport::GrepSubscriptionAlert, LogLevel::llGrep, 7, Logger::LL_ALERT}
};

const Uint32 EventLoggerBase::matrixSize = sizeof(EventLoggerBase::matrix)/
                                       sizeof(EventRepLogLevelMatrix);

/**
 * Specifies allowed event categories/log levels that can be set from
 * the Management API/interactive shell.
 */
const EventLoggerBase::EventCategoryName 
EventLoggerBase::eventCategoryNames[] = {
  { LogLevel::llStartUp,     "STARTUP"     },
  { LogLevel::llStatistic,   "STATISTICS"  },
  { LogLevel::llCheckpoint,  "CHECKPOINT"  },
  { LogLevel::llNodeRestart, "NODERESTART" },
  { LogLevel::llConnection,  "CONNECTION"  },
  { LogLevel::llInfo,        "INFO"        },
  { LogLevel::llGrep,        "GREP"        }
};

const Uint32 
EventLoggerBase::noOfEventCategoryNames = 
  sizeof(EventLoggerBase::eventCategoryNames)/
  sizeof(EventLoggerBase::EventCategoryName);

const char*
EventLogger::getText(char * m_text, size_t m_text_len, 
		     int type,
		     const Uint32* theData, NodeId nodeId)
{
  // TODO: Change the switch implementation...
  char theNodeId[32];
  if (nodeId != 0){
    ::snprintf(theNodeId, 32, "Node %u: ", nodeId);
  } else {
    theNodeId[0] = 0;
  }

  EventReport::EventType eventType = (EventReport::EventType)type;
  switch (eventType){
  case EventReport::Connected:
    ::snprintf(m_text, m_text_len, 
	       "%sNode %u Connected",
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::ConnectedApiVersion:
    ::snprintf(m_text, m_text_len, 
	       "%sNode %u: API version %d.%d.%d",
	       theNodeId,
	       theData[1],
	       getMajor(theData[2]),
	       getMinor(theData[2]),
	       getBuild(theData[2]));
  break;
  case EventReport::Disconnected:
    ::snprintf(m_text, m_text_len, 
	       "%sNode %u Disconnected", 
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::CommunicationClosed:
    //-----------------------------------------------------------------------
    // REPORT communication to node closed.
    //-----------------------------------------------------------------------
    ::snprintf(m_text, m_text_len, 
	       "%sCommunication to Node %u closed", 
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::CommunicationOpened:
    //-----------------------------------------------------------------------
    // REPORT communication to node opened.
    //-----------------------------------------------------------------------
    ::snprintf(m_text, m_text_len, 
	       "%sCommunication to Node %u opened", 
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::NDBStartStarted:
    //-----------------------------------------------------------------------
    // Start of NDB has been initiated.
    //-----------------------------------------------------------------------
    ::snprintf(m_text, m_text_len, 
	       "%sStart initiated (version %d.%d.%d)", 
	       theNodeId ,
	       getMajor(theData[1]),
	       getMinor(theData[1]),
	       getBuild(theData[1]));
  break;
  case EventReport::NDBStopStarted:
    ::snprintf(m_text, m_text_len,
	       "%s%s shutdown initiated", 
	       theNodeId, 
	       (theData[1] == 1 ? "Cluster" : "Node"));
  break;
  case EventReport::NDBStopAborted:
    ::snprintf(m_text, m_text_len,
	       "%sNode shutdown aborted",
	       theNodeId);
  break;
  case EventReport::NDBStartCompleted:
    //-----------------------------------------------------------------------
    // Start of NDB has been completed.
    //-----------------------------------------------------------------------
    ::snprintf(m_text, m_text_len, 
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
    ::snprintf(m_text, m_text_len, 
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
      ::snprintf(m_text, m_text_len, 
		 "%sStart phase %u completed (unknown = %d)", 
		 theNodeId,
		 theData[1],
		 theData[2]);
      return m_text;
    }
    }
    ::snprintf(m_text, m_text_len, 
	       "%sStart phase %u completed %s", 
	       theNodeId,
	       theData[1],
	       type);
    return m_text;
    break;
  }
  case EventReport::CM_REGCONF:
    ::snprintf(m_text, m_text_len, 
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

    ::snprintf(m_text, m_text_len, 
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
    ::snprintf(m_text, 
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
        ::snprintf(m_text, m_text_len, 
		 "%sNode %u completed failure of Node %u", 
		 theNodeId,
		 theData[3], 
		 theData[2]);
      } else {
        ::snprintf(m_text, m_text_len, 
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
      
      ::snprintf(m_text, m_text_len, 
		 "%sNode failure of %u %s completed", 
		 theNodeId,
		 theData[2], 
		 line);
    }
    break;
  case EventReport::NODE_FAILREP:
    ::snprintf(m_text, 
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
        ::snprintf(m_text, m_text_len,
          "%sPresident restarts arbitration thread [state=%u]",
          theNodeId, state);
        break;
      case ArbitCode::PrepPart2:
	sd->ticket.getText(ticketText, sizeof(ticketText));
	::snprintf(m_text, m_text_len,
	  "%sPrepare arbitrator node %u [ticket=%s]",
	  theNodeId, sd->node, ticketText);
	break;
      case ArbitCode::PrepAtrun:
	sd->ticket.getText(ticketText, sizeof(ticketText));
	::snprintf(m_text, m_text_len,
	  "%sReceive arbitrator node %u [ticket=%s]",
	  theNodeId, sd->node, ticketText);
	break;
      case ArbitCode::ApiStart:
	sd->ticket.getText(ticketText, sizeof(ticketText));
	::snprintf(m_text, m_text_len,
	  "%sStarted arbitrator node %u [ticket=%s]",
	  theNodeId, sd->node, ticketText);
	break;
      case ArbitCode::ApiFail:
	::snprintf(m_text, m_text_len,
	  "%sLost arbitrator node %u - process failure [state=%u]",
	  theNodeId, sd->node, state);
	break;
      case ArbitCode::ApiExit:
	::snprintf(m_text, m_text_len,
	  "%sLost arbitrator node %u - process exit [state=%u]",
	  theNodeId, sd->node, state);
	break;
      default:
	ArbitCode::getErrText(code, errText, sizeof(errText));
	::snprintf(m_text, m_text_len,
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
	::snprintf(m_text, m_text_len,
	  "%sArbitration check lost - less than 1/2 nodes left",
	  theNodeId);
	break;
      case ArbitCode::WinGroups:
	::snprintf(m_text, m_text_len,
	  "%sArbitration check won - node group majority",
	  theNodeId);
	break;
      case ArbitCode::LoseGroups:
	::snprintf(m_text, m_text_len,
	  "%sArbitration check lost - missing node group",
	  theNodeId);
	break;
      case ArbitCode::Partitioning:
	::snprintf(m_text, m_text_len,
	  "%sNetwork partitioning - arbitration required",
	  theNodeId);
	break;
      case ArbitCode::WinChoose:
	::snprintf(m_text, m_text_len,
	  "%sArbitration won - positive reply from node %u",
	  theNodeId, sd->node);
	break;
      case ArbitCode::LoseChoose:
	::snprintf(m_text, m_text_len,
	  "%sArbitration lost - negative reply from node %u",
	  theNodeId, sd->node);
	break;
      case ArbitCode::LoseNorun:
	::snprintf(m_text, m_text_len,
	  "%sNetwork partitioning - no arbitrator available",
	  theNodeId);
	break;
      case ArbitCode::LoseNocfg:
	::snprintf(m_text, m_text_len,
	  "%sNetwork partitioning - no arbitrator configured",
	  theNodeId);
	break;
      default:
	ArbitCode::getErrText(code, errText, sizeof(errText));
	::snprintf(m_text, m_text_len,
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
    ::snprintf(m_text, 
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
    ::snprintf(m_text, m_text_len, 
	       "%sGlobal checkpoint %u completed", 
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::LocalCheckpointStarted:
    //-----------------------------------------------------------------------
    // This event reports that a local checkpoint has been started and this
    // node is the master of this local checkpoint.
    //-----------------------------------------------------------------------
    ::snprintf(m_text, 
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
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sLocal checkpoint %u completed", 
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::TableCreated:
    //-----------------------------------------------------------------------
    // This event reports that a table has been created.
    //-----------------------------------------------------------------------
    ::snprintf(m_text, m_text_len, 
	       "%sTable with ID =  %u created", 
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::LCPStoppedInCalcKeepGci:
    if (theData[1] == 0)
      ::snprintf(m_text, m_text_len, 
		 "%sLocal Checkpoint stopped in CALCULATED_KEEP_GCI",
		 theNodeId);
    break;
  case EventReport::NR_CopyDict:
    //-----------------------------------------------------------------------
    // REPORT Node Restart completed copy of dictionary information.
    //-----------------------------------------------------------------------
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sNode restart completed copy of dictionary information",
	       theNodeId);
    break;
  case EventReport::NR_CopyDistr:
    //-----------------------------------------------------------------------
    // REPORT Node Restart completed copy of distribution information.
    //-----------------------------------------------------------------------
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sNode restart completed copy of distribution information",
	       theNodeId);
    break;
  case EventReport::NR_CopyFragsStarted:
    //-----------------------------------------------------------------------
    // REPORT Node Restart is starting to copy the fragments.
    //-----------------------------------------------------------------------
    ::snprintf(m_text, 
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
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sTable ID = %u, fragment ID = %u have been copied "
	       "to Node %u", 
	       theNodeId,
	       theData[2], 
	       theData[3], 
	       theData[1]);
  break;
  case EventReport::NR_CopyFragsCompleted:
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sNode restart completed copying the fragments "
	       "to Node %u", 
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::LCPFragmentCompleted:
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sTable ID = %u, fragment ID = %u has completed LCP "
	       "on Node %u", 
	       theNodeId,
	       theData[2], 
	       theData[3], 
	       theData[1]);
    break;
  case EventReport::TransReportCounters:
    // -------------------------------------------------------------------  
    // Report information about transaction activity once per 10 seconds.
    // ------------------------------------------------------------------- 
    ::snprintf(m_text, 
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
    ::snprintf(m_text, m_text_len,
	       "%sOperations=%u",
	       theNodeId, 
	       theData[1]);
    break;
  case EventReport::UndoLogBlocked:
    //-----------------------------------------------------------------------
    // REPORT Undo Logging blocked due to buffer near to overflow.
    //-----------------------------------------------------------------------
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sACC Blocked %u and TUP Blocked %u times last second",
	       theNodeId,
	       theData[1],
	       theData[2]);
  break;
  case EventReport::TransporterError:
  case EventReport::TransporterWarning:
    ::snprintf(m_text, 
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
    ::snprintf(m_text, 
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
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sNode %d declared dead due to missed heartbeat",
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::JobStatistic:
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sMean loop Counter in doJob last 8192 times = %u",
	       theNodeId,
	       theData[1]);
    break;
  case EventReport::SendBytesStatistic:
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sMean send size to Node = %d last 4096 sends = %u bytes",
	       theNodeId,
	       theData[1],
	       theData[2]);
    break;
  case EventReport::ReceiveBytesStatistic:
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sMean receive size to Node = %d last 4096 sends = %u bytes",
	       theNodeId,
	       theData[1],
	       theData[2]);
  break;
  case EventReport::SentHeartbeat:
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sNode Sent Heartbeat to node = %d",
	       theNodeId,
	       theData[1]);
  break;
  case EventReport::CreateLogBytes:
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sLog part %u, log file %u, MB %u",
	       theNodeId,
	       theData[1],
	       theData[2],
	       theData[3]);
  break;
  case EventReport::StartLog:
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sLog part %u, start MB %u, stop MB %u, last GCI, log exec %u",
	       theNodeId,
	       theData[1],
	       theData[2],
	       theData[3],
	       theData[4]);
  break;
  case EventReport::StartREDOLog:
    ::snprintf(m_text, 
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
    
    ::snprintf(m_text, 
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
    ::snprintf(m_text, 
	       m_text_len, 
	       "%s%s",
	       theNodeId,
	       (char *)&theData[1]);
  break;
  case EventReport::WarningEvent:
    ::snprintf(m_text, 
	       m_text_len, 
	       "%s%s",
	       theNodeId,
	       (char *)&theData[1]);
  break;
  case EventReport::GCP_TakeoverStarted:
    ::snprintf(m_text,
	       m_text_len,
	       "%sGCP Take over started", theNodeId);
  break;
  case EventReport::GCP_TakeoverCompleted:
    ::snprintf(m_text,
	       m_text_len,
	       "%sGCP Take over completed", theNodeId);
  break;
  case EventReport::LCP_TakeoverStarted:
    ::snprintf(m_text,
	       m_text_len,
	       "%sLCP Take over started", theNodeId);
  break;
  case EventReport::LCP_TakeoverCompleted:
    ::snprintf(m_text,
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
    
    ::snprintf(m_text, m_text_len,
	       "%s%s usage %s %d%s(%d %dK pages of total %d)",
	       theNodeId,
	       (block==DBACC ? "Index" : (block == DBTUP ?"Data":"<unknown>")),
	       (gth == 0 ? "is" : (gth > 0 ? "increased to" : "decreased to")),
	       percent, "%",
	       used, size/1024, total
	       );
    break;
  }


  case EventReport::GrepSubscriptionInfo : 
    {   
      GrepEvent::Subscription event  = (GrepEvent::Subscription)theData[1];
      switch(event) {
      case GrepEvent::GrepSS_CreateSubIdConf: 
	{
	  const int subId  = theData[2];
	  const int subKey = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::SSCoord: Created subscription id"
		     " (subId=%d,SubKey=%d)"
		     " Return code: %d.",
		     subId,
		     subKey,
		     err);
	  break;
	}      
      case GrepEvent::GrepPS_CreateSubIdConf: 
	{
	  const int subId  = theData[2];
	  const int subKey = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::PSCoord: Created subscription id" 
		     " (subId=%d,SubKey=%d)"
		     " Return code: %d.",
		     subId,
		     subKey,
		     err);
	  break;
	}
      case GrepEvent::GrepSS_SubCreateConf: 
	{
	  const int subId   = theData[2];
	  const int subKey  = theData[3];
	  const int err     = theData[4];
	  const int nodegrp = theData[5];
	  ::snprintf(m_text, m_text_len, 
		   "Grep::SSCoord: Created subscription using"
		     " (subId=%d,SubKey=%d)" 
		     " in primary system. Primary system has %d nodegroup(s)."
		     " Return code: %d",
		     subId,
		     subKey,
		     nodegrp,
		     err);
	  break;
	}
      case GrepEvent::GrepPS_SubCreateConf: 
	{
	  const int subId   = theData[2];
	  const int subKey  = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::PSCoord: All participants have created "
		     "subscriptions"
		     " using (subId=%d,SubKey=%d)."
		     " Return code: %d",
		     subId,
		     subKey,
		     err);
	  break;
	}      
      case GrepEvent::GrepSS_SubStartMetaConf: 
	{
	  const int subId  = theData[2];
	  const int subKey = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::SSCoord: Logging started on meta data changes." 
		     " using (subId=%d,SubKey=%d)"
		     " Return code: %d",
		     subId,
		     subKey,
		     err);
	  break;
	}
      case GrepEvent::GrepPS_SubStartMetaConf: 
	{
	  const int subId  = theData[2];
	  const int subKey = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::PSCoord: All participants have started " 
		     "logging meta data" 
		     " changes on the subscription subId=%d,SubKey=%d) "
		     "(N.I yet)."
		     " Return code: %d",
		     subId,
		     subKey,
		     err);
	  break;
	}
      case GrepEvent::GrepSS_SubStartDataConf: {
	const int subId  = theData[2];
	const int subKey = theData[3];
	const int err    = theData[4];
	::snprintf(m_text, m_text_len, 
		   "Grep::SSCoord: Logging started on table data changes " 
		   " using (subId=%d,SubKey=%d)"
		   " Return code: %d",
		   subId,
		   subKey,
		   err);
	break;
      }
      case GrepEvent::GrepPS_SubStartDataConf: 
	{
	  const int subId  = theData[2];
	  const int subKey = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::PSCoord: All participants have started logging "
		     "table data changes on the subscription " 
		     "subId=%d,SubKey=%d)."
		     " Return code: %d",
		     subId,
		     subKey,
		     err);
	  break;
	}
      case GrepEvent::GrepPS_SubSyncMetaConf: 
	{
	  const int subId  = theData[2];
	  const int subKey = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::PSCoord: All participants have started "
		     " synchronization  on meta data (META SCAN) using "
		     "(subId=%d,SubKey=%d)."
		     " Return code: %d",
		     subId,
		     subKey,
		     err);
	  break;
	}
      case GrepEvent::GrepSS_SubSyncMetaConf: 
	{
	  const int subId  = theData[2];
	  const int subKey = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::SSCoord: Synchronization started (META SCAN) on "
		     " meta data using (subId=%d,SubKey=%d)"
		     " Return code: %d",
		     subId,
		     subKey,
		     err);
	  break;
	}
      case GrepEvent::GrepPS_SubSyncDataConf: 
	{
	  const int subId  = theData[2];
	  const int subKey = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::PSCoord: All participants have started " 
		     "synchronization "
		     " on table data (DATA SCAN) using (subId=%d,SubKey=%d)."
		     " Return code: %d",
		     subId,
		     subKey,
		     err);
	  break;
	}
      case GrepEvent::GrepSS_SubSyncDataConf: 
	{
	  const int subId   =  theData[2];
	  const int subKey  =  theData[3];
	  const int err     =  theData[4];
	  const int gci     =  theData[5];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::SSCoord: Synchronization started (DATA SCAN) on "
		     "table data using (subId=%d,SubKey=%d). GCI = %d"
		     " Return code: %d",
		     subId,
		     subKey,
		     gci,
		     err);
	  break;
	}      
      case GrepEvent::GrepPS_SubRemoveConf: 
	{
	  const int subId  = theData[2];
	  const int subKey = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::PSCoord: All participants have removed "
		     "subscription (subId=%d,SubKey=%d). I have cleaned "
		     "up resources I've used."
		     " Return code: %d",
		     subId,
		     subKey,
		     err);
	  break;
	}	
      case GrepEvent::GrepSS_SubRemoveConf: 
	{
	  const int subId  = theData[2];
	  const int subKey = theData[3];
	  const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::SSCoord: Removed subscription "
		     "(subId=%d,SubKey=%d)"
		     " Return code: %d",
		     subId,
		     subKey,
		     err);
	  break;
	}
    default:
      ::snprintf(m_text, 
		 m_text_len, 
		 "%sUnknown GrepSubscriptonInfo event: %d",
		 theNodeId,
		 theData[1]);
      }
      break;
    }
      
  case EventReport::GrepSubscriptionAlert : 
    {
      GrepEvent::Subscription event  = (GrepEvent::Subscription)theData[1];
      switch(event) 
	{ 
	case GrepEvent::GrepSS_CreateSubIdRef: 
	  {
	    const int subId    = theData[2];
	    const int subKey   = theData[3];
	    const int err      = theData[4];
	    ::snprintf(m_text, m_text_len, 
		       "Grep::SSCoord:Error code: %d Error message: %s"
		       " (subId=%d,SubKey=%d)",
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err),
		       subId,
		       subKey);
	    break;
	  }
	case GrepEvent::GrepSS_SubCreateRef: 
	  {
	    const int subId   = theData[2];
	    const int subKey  = theData[3];
	    const int err     = theData[4];
	    ::snprintf(m_text, m_text_len, 
		       "Grep::SSCoord: FAILED to Created subscription using"
		       " (subId=%d,SubKey=%d)in primary system."
		       " Error code: %d Error Message: %s",
		       subId,
		       subKey,
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err));
	    break;
	  }
	case GrepEvent::GrepSS_SubStartMetaRef: 
	  {
	    const int subId  = theData[2];
	    const int subKey = theData[3];
	    const int err    = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::SSCoord: Logging failed to start on meta "
		     "data changes." 
		     " using (subId=%d,SubKey=%d)"
		     " Error code: %d Error Message: %s",
		     subId,
		     subKey,
		     err,
		     GrepError::getErrorDesc((GrepError::Code)err));
	    break;
	  }
	case GrepEvent::GrepSS_SubStartDataRef: 
	  {
	    const int subId  = theData[2];
	    const int subKey = theData[3];
	    const int err    = theData[4];
	    ::snprintf(m_text, m_text_len, 
		       "Grep::SSCoord: Logging FAILED to start on table data "
		       " changes using (subId=%d,SubKey=%d)"
		       " Error code: %d Error Message: %s",
		       subId,
		       subKey,
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err));
	    break;
	  }
	case GrepEvent::GrepSS_SubSyncMetaRef: 
	  {
	    const int subId   = theData[2];
	    const int subKey  = theData[3];
	    const int err     = theData[4];
	    ::snprintf(m_text, m_text_len, 
		       "Grep::SSCoord: Synchronization FAILED (META SCAN) on "
		       " meta data using (subId=%d,SubKey=%d)"
		       " Error code: %d Error Message: %s",
		       subId,
		       subKey,
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err));
	    break;
	}
	case GrepEvent::GrepSS_SubSyncDataRef: 
	  {
	    const int subId   =  theData[2];
	    const int subKey  =  theData[3];
	    const int err     =  theData[4];
	    const int gci     =  theData[5];
	    ::snprintf(m_text, m_text_len, 
		       "Grep::SSCoord: Synchronization FAILED (DATA SCAN) on "
		       "table data using (subId=%d,SubKey=%d). GCI = %d"
		       " Error code: %d Error Message: %s", 
		       subId,
		       subKey,
		       gci,
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err));
	    break;
	  }
	case GrepEvent::GrepSS_SubRemoveRef: 
	  {
	    const int subId  = theData[2];
	    const int subKey = theData[3];
	    const int err    = theData[4];
	    ::snprintf(m_text, m_text_len, 
		     "Grep::SSCoord: Failed to remove subscription "
		       "(subId=%d,SubKey=%d). "
		       " Error code: %d Error Message: %s",
		       subId,
		       subKey,
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err)
		       );
	    break;
	  }	
	
	case GrepEvent::GrepPS_CreateSubIdRef: 
	  {
	    const int subId  = theData[2];
	    const int subKey = theData[3];
	    const int err    = theData[4];
	    ::snprintf(m_text, m_text_len, 
		       "Grep::PSCoord: Error code: %d Error Message: %s"
		       " (subId=%d,SubKey=%d)",
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err),
		       subId,
		       subKey);
	    break;
	  }
	case GrepEvent::GrepPS_SubCreateRef: 
	  {
	    const int subId   = theData[2];
	    const int subKey  = theData[3];
	    const int err     = theData[4];
	    ::snprintf(m_text, m_text_len, 
		       "Grep::PSCoord: FAILED to Created subscription using"
		       " (subId=%d,SubKey=%d)in primary system."
		       " Error code: %d Error Message: %s",
		       subId,
		       subKey,
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err));
	    break;
	  }
      case GrepEvent::GrepPS_SubStartMetaRef: 
	{
	  const int subId   = theData[2];
	  const int subKey  = theData[3];
	  const int err     = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::PSCoord: Logging failed to start on meta "
		     "data changes." 
		     " using (subId=%d,SubKey=%d)"
		     " Error code: %d Error Message: %s",
		     subId,
		     subKey,		       
		     err,
		     GrepError::getErrorDesc((GrepError::Code)err));
	  break;
	}
	case GrepEvent::GrepPS_SubStartDataRef: 
	  {
	    const int subId  = theData[2];
	    const int subKey = theData[3];
	    const int err    = theData[4];
	    ::snprintf(m_text, m_text_len, 
		       "Grep::PSCoord: Logging FAILED to start on table data "
		       " changes using (subId=%d,SubKey=%d)"
		       " Error code: %d Error Message: %s",
		       subId,
		       subKey,
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err));
	    break;
	  }
	case GrepEvent::GrepPS_SubSyncMetaRef: 
	{
	  const int subId   = theData[2];
	  const int subKey  = theData[3];
	  const int err     = theData[4];
	  ::snprintf(m_text, m_text_len, 
		     "Grep::PSCoord: Synchronization FAILED (META SCAN) on "
		     " meta data using (subId=%d,SubKey=%d)"
		     " Error code: %d Error Message: %s",
		     subId,
		     subKey,
		     err,
		     GrepError::getErrorDesc((GrepError::Code)err));
	  break;
	}
	case GrepEvent::GrepPS_SubSyncDataRef: 
	  {
	    const int subId   =  theData[2];
	    const int subKey  =  theData[3];
	    const int err     =  theData[4];
	    const int gci     =  theData[5];
	    ::snprintf(m_text, m_text_len, 
		       "Grep::PSCoord: Synchronization FAILED (DATA SCAN) on "
		       "table data using (subId=%d,SubKey=%d). GCI = %d. "
		       " Error code: %d Error Message: %s",
		       subId,
		       subKey,
		       gci,
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err));
	    break;
	  }
	case GrepEvent::GrepPS_SubRemoveRef: 
	  {
	    const int subId  = theData[2];
	    const int subKey = theData[3];
	    const int err    = theData[4];
	    ::snprintf(m_text, m_text_len, 
		     "Grep::PSCoord: Failed to remove subscription "
		       "(subId=%d,SubKey=%d)." 
		       " Error code: %d Error Message: %s",
		       subId,
		       subKey,
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err));
	    break;
	  }	
	case GrepEvent::Rep_Disconnect:
	  {
	    const int err       = theData[4];
	    const int nodeId    = theData[5];   
	    ::snprintf(m_text, m_text_len, 
		       "Rep: Node %d."
		       " Error code: %d Error Message: %s",
		       nodeId,
		       err,
		       GrepError::getErrorDesc((GrepError::Code)err));
	    break;
	  }	
	
	
	default:
	  ::snprintf(m_text, 
		     m_text_len, 
		     "%sUnknown GrepSubscriptionAlert event: %d",
		     theNodeId,
		     theData[1]);
	break;
	}
      break;
    }
  
  default:
    ::snprintf(m_text, 
	       m_text_len, 
	       "%sUnknown event: %d",
	       theNodeId,
	       theData[0]);
  
  }
  return m_text;
}

bool
EventLoggerBase::matchEventCategory(const char * str, 
				LogLevel::EventCategory * cat,
				bool exactMatch){
  unsigned i;
  if(cat == 0 || str == 0)
    return false;

  char * tmp = strdup(str);
  for(i = 0; i<strlen(tmp); i++)
    tmp[i] = toupper(tmp[i]);
  
  for(i = 0; i<noOfEventCategoryNames; i++){
    if(strcmp(tmp, eventCategoryNames[i].name) == 0){
      * cat = eventCategoryNames[i].category;
      free(tmp);
      return true;
    }
  }
  free(tmp);
  return false;
}

const char *
EventLoggerBase::getEventCategoryName(LogLevel::EventCategory cat){
  
  for(unsigned i = 0; i<noOfEventCategoryNames; i++){
    if(cat == eventCategoryNames[i].category){
      return eventCategoryNames[i].name;
    }
  }
  return 0;
}


EventLogger::EventLogger() : m_filterLevel(15)
{
  setCategory("EventLogger");
  m_logLevel.setLogLevel(LogLevel::llStartUp, m_filterLevel);
  m_logLevel.setLogLevel(LogLevel::llShutdown, m_filterLevel);
  m_logLevel.setLogLevel(LogLevel::llStatistic, m_filterLevel);
  m_logLevel.setLogLevel(LogLevel::llCheckpoint, m_filterLevel); 
  m_logLevel.setLogLevel(LogLevel::llNodeRestart, m_filterLevel); 
  m_logLevel.setLogLevel(LogLevel::llConnection, m_filterLevel);
  m_logLevel.setLogLevel(LogLevel::llError, m_filterLevel);
  m_logLevel.setLogLevel(LogLevel::llInfo, m_filterLevel);
  enable(Logger::Logger::LL_INFO, Logger::Logger::LL_ALERT); // Log INFO to ALERT
  
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

void 
EventLogger::log(int eventType, const Uint32* theData, NodeId nodeId)
{
  Uint32 threshold = 0;
  Logger::LoggerLevel severity = Logger::LL_WARNING;
  LogLevel::EventCategory cat;

  for(unsigned i = 0; i<EventLogger::matrixSize; i++){
    if(EventLogger::matrix[i].eventType == eventType){
      cat = EventLogger::matrix[i].eventCategory;
      threshold = EventLogger::matrix[i].threshold;
      severity = EventLogger::matrix[i].severity;
      break;
    }
  }

  if (threshold <= m_logLevel.getLogLevel(cat)){
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
