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

#include <SignalLoggerManager.hpp>
#include <signaldata/AccLock.hpp>

bool printACC_LOCKREQ(FILE *output, const Uint32 *theData, Uint32 len,
                      Uint16 /*rbn*/) {
  const AccLockReq *const sig = (const AccLockReq *)theData;
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
    if (len < AccLockReq::LockSignalLength) {
      assert(false);
      return false;
    }

    fprintf(output, " userPtr: 0x%x userRef: 0x%x\n", sig->userPtr,
            sig->userRef);
    fprintf(output, " table: id=%u", sig->tableId);
    fprintf(output, " fragment: id=%u ptr=0x%x\n", sig->fragId, sig->fragPtrI);
    fprintf(output, " tuple: addr=%u/%u hashValue=%x\n", sig->page_id,
            sig->page_idx, sig->hashValue);
    fprintf(output, " transid: %08x %08x\n", sig->transId1, sig->transId2);
  }
  return true;
}
