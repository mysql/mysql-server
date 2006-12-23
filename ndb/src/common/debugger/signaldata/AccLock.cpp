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

#include <signaldata/AccLock.hpp>
#include <SignalLoggerManager.hpp>

bool
printACC_LOCKREQ(FILE* output, const Uint32* theData, Uint32 len, Uint16 rbn)
{
  const AccLockReq* const sig = (const AccLockReq*)theData;
  Uint32 reqtype = sig->requestInfo & 0xFF;
  switch (sig->returnCode) {
  case RNIL:
    fprintf(output, " returnCode=RNIL");
    break;
  case AccLockReq::Success:
    fprintf(output, " returnCode=Success");
    break;
  case AccLockReq::IsBlocked:
    fprintf(output, " returnCode=IsBlocked");
    break;
  case AccLockReq::WouldBlock:
    fprintf(output, " returnCode=WouldBlock");
    break;
  case AccLockReq::Refused:
    fprintf(output, " returnCode=Refused");
    break;
  case AccLockReq::NoFreeOp:
    fprintf(output, " returnCode=NoFreeOp");
    break;
  default:
    fprintf(output, " returnCode=%u?", sig->returnCode);
    break;
  }
  switch (reqtype) {
    case AccLockReq::LockShared:
    fprintf(output, " req=LockShared\n");
    break;
  case AccLockReq::LockExclusive:
    fprintf(output, " req=LockExclusive\n");
    break;
  case AccLockReq::Unlock:
    fprintf(output, " req=Unlock\n");
    break;
  case AccLockReq::Abort:
    fprintf(output, " req=Abort\n");
    break;
  default:
    fprintf(output, " req=%u\n", reqtype);
    break;
  }
  fprintf(output, " accOpPtr: 0x%x\n", sig->accOpPtr);
  if (reqtype == AccLockReq::LockShared ||
      reqtype == AccLockReq::LockExclusive) {
    fprintf(output, " userPtr: 0x%x userRef: 0x%x\n", sig->userPtr, sig->userRef);
    fprintf(output, " table: id=%u", sig->tableId);
    fprintf(output, " fragment: id=%u ptr=0x%x\n", sig->fragId, sig->fragPtrI);
    fprintf(output, " tuple: addr=0x%x hashValue=%x\n", sig->tupAddr, sig->hashValue);
    fprintf(output, " transid: %08x %08x\n", sig->transId1, sig->transId2);
  }
  return true;
}
