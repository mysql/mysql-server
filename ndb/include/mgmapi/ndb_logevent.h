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

#ifndef NDB_LOGEVENT_H
#define NDB_LOGEVENT_H

#ifdef __cplusplus
extern "C" {
#endif

  enum Ndb_logevent_type {
    /* CONNECTION */
    NDB_LE_Connected = 0,
    NDB_LE_Disconnected = 1,
    NDB_LE_CommunicationClosed = 2,
    NDB_LE_CommunicationOpened = 3,
    NDB_LE_ConnectedApiVersion = 51,
    /* CHECKPOINT */
    NDB_LE_GlobalCheckpointStarted = 4,
    NDB_LE_GlobalCheckpointCompleted = 5,
    NDB_LE_LocalCheckpointStarted = 6,
    NDB_LE_LocalCheckpointCompleted = 7,
    NDB_LE_LCPStoppedInCalcKeepGci = 8,
    NDB_LE_LCPFragmentCompleted = 9,
    /* STARTUP */
    NDB_LE_NDBStartStarted = 10,
    NDB_LE_NDBStartCompleted = 11,
    NDB_LE_STTORRYRecieved = 12,
    NDB_LE_StartPhaseCompleted = 13,
    NDB_LE_CM_REGCONF = 14,
    NDB_LE_CM_REGREF = 15,
    NDB_LE_FIND_NEIGHBOURS = 16,
    NDB_LE_NDBStopStarted = 17,
    NDB_LE_NDBStopAborted = 18,
    NDB_LE_StartREDOLog = 19,
    NDB_LE_StartLog = 20,
    NDB_LE_UNDORecordsExecuted = 21,

    /* NODERESTART */
    NDB_LE_NR_CopyDict = 22,
    NDB_LE_NR_CopyDistr = 23,
    NDB_LE_NR_CopyFragsStarted = 24,
    NDB_LE_NR_CopyFragDone = 25,
    NDB_LE_NR_CopyFragsCompleted = 26,

    /* NODEFAIL */
    NDB_LE_NodeFailCompleted = 27,
    NDB_LE_NODE_FAILREP = 28,
    NDB_LE_ArbitState = 29,
    NDB_LE_ArbitResult = 30,
    NDB_LE_GCP_TakeoverStarted = 31,
    NDB_LE_GCP_TakeoverCompleted = 32,
    NDB_LE_LCP_TakeoverStarted = 33,
    NDB_LE_LCP_TakeoverCompleted = 34,

    /* STATISTIC */
    NDB_LE_TransReportCounters = 35,
    NDB_LE_OperationReportCounters = 36,
    NDB_LE_TableCreated = 37,
    NDB_LE_UndoLogBlocked = 38,
    NDB_LE_JobStatistic = 39,
    NDB_LE_SendBytesStatistic = 40,
    NDB_LE_ReceiveBytesStatistic = 41,
    NDB_LE_MemoryUsage = 50,

    /* ERROR */
    NDB_LE_TransporterError = 42,
    NDB_LE_TransporterWarning = 43,
    NDB_LE_MissedHeartbeat = 44,
    NDB_LE_DeadDueToHeartbeat = 45,
    NDB_LE_WarningEvent = 46,

    /* INFO */
    NDB_LE_SentHeartbeat = 47,
    NDB_LE_CreateLogBytes = 48,
    NDB_LE_InfoEvent = 49,

    /* GREP */
    NDB_LE_GrepSubscriptionInfo = 52,
    NDB_LE_GrepSubscriptionAlert = 53,

    /* BACKUP */
    NDB_LE_BackupStarted = 54,
    NDB_LE_BackupFailedToStart = 55,
    NDB_LE_BackupCompleted = 56,
    NDB_LE_BackupAborted = 57
  };

#ifdef __cplusplus
}
#endif

#endif
