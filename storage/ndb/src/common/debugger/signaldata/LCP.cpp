/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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


#include <RefConvert.hpp>
#include <signaldata/LCP.hpp>
#include <DebuggerNames.hpp>

bool
printSTART_LCP_REQ(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo){
  
  const StartLcpReq * const sig = (StartLcpReq *) theData;
 
  char buf1[8*_NDB_NODE_BITMASK_SIZE+1];
  char buf2[8*_NDB_NODE_BITMASK_SIZE+1];
  fprintf(output, 
	  " Sender: %d LcpId: %d PauseStart: %d\n"
	  " ParticipatingDIH = %s\n"
	  " ParticipatingLQH = %s\n",
	  refToNode(sig->senderRef), sig->lcpId, sig->pauseStart,
	  sig->participatingDIH.getText(buf1),
	  sig->participatingLQH.getText(buf2));
  
  return true;
}

bool
printSTART_LCP_CONF(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo){
  
  const StartLcpConf * const sig = (StartLcpConf *) theData;
  
  fprintf(output, " Sender: %d LcpId: %d\n",
	  refToNode(sig->senderRef), sig->lcpId);
  
  return true;
}

bool
printLCP_FRAG_ORD(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo){
  
  const LcpFragOrd * const sig = (LcpFragOrd *) theData;
  
  fprintf(output, " LcpId: %d LcpNo: %d Table: %d Fragment: %d\n",
	  sig->lcpId, sig->lcpNo, sig->tableId, sig->fragmentId);
  
  fprintf(output, " KeepGCI: %d LastFragmentFlag: %d\n",
	  sig->keepGci, sig->lastFragmentFlag);
  return true;
}

bool
printLCP_FRAG_REP(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo){
  
  const LcpFragRep * const sig = (LcpFragRep *) theData;
  
  fprintf(output, " LcpId: %d LcpNo: %d NodeId: %d Table: %d Fragment: %d\n",
	  sig->lcpId, sig->lcpNo, sig->nodeId, sig->tableId, sig->fragId);
  fprintf(output, " Max GCI Started: %d Max GCI Completed: %d\n",
	  sig->maxGciStarted, sig->maxGciCompleted);
  return true;
}

bool
printLCP_COMPLETE_REP(FILE * output, const Uint32 * theData, 
		      Uint32 len, Uint16 receiverBlockNo){
  
  const LcpCompleteRep * const sig = (LcpCompleteRep *) theData;
  
  fprintf(output, " LcpId: %d NodeId: %d Block: %s\n",
	  sig->lcpId, sig->nodeId, getBlockName(sig->blockNo));
  return true;
}

bool
printLCP_STATUS_REQ(FILE * output, const Uint32 * theData, 
                    Uint32 len, Uint16 receiverBlockNo){
  const LcpStatusReq* const sig = (LcpStatusReq*) theData;
  
  fprintf(output, " SenderRef : %x SenderData : %u\n", 
          sig->senderRef, sig->senderData);
  return true;
}

bool
printLCP_STATUS_CONF(FILE * output, const Uint32 * theData, 
                     Uint32 len, Uint16 receiverBlockNo){
  const LcpStatusConf* const sig = (LcpStatusConf*) theData;
  
  fprintf(output, " SenderRef : %x SenderData : %u LcpState : %u tableId : %u fragId : %u\n",
          sig->senderRef, sig->senderData, sig->lcpState, sig->tableId, sig->fragId);
  fprintf(output, " replica(Progress : %llu), lcpDone (Rows : %llu, Bytes : %llu)\n",
          (((Uint64)sig->completionStateHi) << 32) + sig->completionStateLo,
          (((Uint64)sig->lcpDoneRowsHi) << 32) + sig->lcpDoneRowsLo,
          (((Uint64)sig->lcpDoneBytesHi) << 32) + sig->lcpDoneBytesLo);
  fprintf(output, "lcpScannedPages : %u\n", sig->lcpScannedPages);
  return true;
}

bool
printLCP_STATUS_REF(FILE * output, const Uint32 * theData, 
                    Uint32 len, Uint16 receiverBlockNo){
  const LcpStatusRef* const sig = (LcpStatusRef*) theData;
  
  fprintf(output, " SenderRef : %x, SenderData : %u Error : %u\n", 
          sig->senderRef, sig->senderData, sig->error);
  return true;
}

bool
printLCP_PREPARE_REQ(FILE *output,
                     const Uint32 *theData,
                     Uint32 len,
                     Uint16 receiverBlockNo)
{
  const LcpPrepareReq* const sig = (LcpPrepareReq*)theData;

  fprintf(output, "senderData: %x, senderRef: %x, lcpNo: %u, tableId: %u, "
                  "fragmentId: %u\n"
                  "lcpId: %u, localLcpId: %u, backupPtr: %u, backupId: %u,"
                  " createGci: %u\n",
                  sig->senderData,
                  sig->senderRef,
                  sig->lcpNo,
                  sig->tableId,
                  sig->fragmentId,
                  sig->lcpId,
                  sig->localLcpId,
                  sig->backupPtr,
                  sig->backupId,
                  sig->createGci);
  return true;
}

bool
printLCP_PREPARE_CONF(FILE *output,
                      const Uint32 *theData,
                      Uint32 len,
                      Uint16 receiverBlockNo)
{
  const LcpPrepareConf* const sig = (LcpPrepareConf*)theData;

  fprintf(output, "senderData: %x, senderRef: %x, tableId: %u, fragmentId: %u\n",
          sig->senderData,
          sig->senderRef,
          sig->tableId,
          sig->fragmentId);
  return true;
}

bool
printLCP_PREPARE_REF(FILE *output,
                     const Uint32 *theData,
                     Uint32 len,
                     Uint16 receiverBlockNo)
{
  const LcpPrepareRef* const sig = (LcpPrepareRef*)theData;

  fprintf(output, "senderData: %x, senderRef: %x, tableId: %u, fragmentId: %u"
                  ", errorCode: %u\n",
          sig->senderData,
          sig->senderRef,
          sig->tableId,
          sig->fragmentId,
          sig->errorCode);
  return true;
}

bool
printSYNC_PAGE_CACHE_REQ(FILE *output,
                         const Uint32 *theData,
                         Uint32 len,
                         Uint16 receiverBlockNo)
{
  const SyncPageCacheReq* const sig = (SyncPageCacheReq*)theData;
  fprintf(output, "senderData: %x, senderRef: %x, tableId: %u, fragmentId: %u\n",
          sig->senderData,
          sig->senderRef,
          sig->tableId,
          sig->fragmentId);
  return true;
}

bool
printSYNC_PAGE_CACHE_CONF(FILE *output,
                          const Uint32 *theData,
                          Uint32 len,
                          Uint16 receiverBlockNo)
{
  const SyncPageCacheConf* const sig = (SyncPageCacheConf*)theData;
  fprintf(output, "senderData: %x, senderRef: %x, tableId: %u, fragmentId: %u\n"
                  "diskDataExistFlag: %u\n",
          sig->senderData,
          sig->senderRef,
          sig->tableId,
          sig->fragmentId,
          sig->diskDataExistFlag);
  return true;
}

bool
printEND_LCPREQ(FILE *output,
                const Uint32 *theData,
                Uint32 len,
                Uint16 receiverBlockNo)
{
  const EndLcpReq* const sig = (EndLcpReq*)theData;
  fprintf(output, "senderData: %x, senderRef: %x, backupPtr: %u, backupId: %u\n"
                  "proxyBlockNo: %u\n",
                  sig->senderData,
                  sig->senderRef,
                  sig->backupPtr,
                  sig->backupId,
                  sig->proxyBlockNo);
  return true;
}

bool
printEND_LCPCONF(FILE *output,
                 const Uint32 *theData,
                 Uint32 len,
                 Uint16 receiverBlockNo)
{
  const EndLcpConf* const sig = (EndLcpConf*)theData;
  fprintf(output, "senderData: %x, senderRef: %x\n",
          sig->senderData,
          sig->senderRef);
  return true;
}
