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

#include <signaldata/UtilLock.hpp>

bool
printUTIL_LOCK_REQ (FILE * output, const Uint32 * theData,
		    Uint32 len, Uint16 receiverBlockNo)
{
  const UtilLockReq *const sig = (UtilLockReq *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  fprintf (output, " requestInfo: %x\n", sig->requestInfo);
  return true;
}

bool
printUTIL_LOCK_CONF (FILE * output, const Uint32 * theData,
		     Uint32 len, Uint16 receiverBlockNo)
{
  const UtilLockConf *const sig = (UtilLockConf *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  fprintf (output, " lockKey: %x\n", sig->lockKey);
  return true;
}

bool
printUTIL_LOCK_REF (FILE * output, const Uint32 * theData,
		    Uint32 len, Uint16 receiverBlockNo)
{
  const UtilLockRef *const sig = (UtilLockRef *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  fprintf (output, " errorCode: %x\n", sig->errorCode);
  return true;
}

bool
printUTIL_UNLOCK_REQ (FILE * output, const Uint32 * theData,
		      Uint32 len, Uint16 receiverBlockNo)
{
  const UtilUnlockReq *const sig = (UtilUnlockReq *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  fprintf (output, " lockKey: %x\n", sig->lockKey);
  return true;
}

bool
printUTIL_UNLOCK_CONF (FILE * output, const Uint32 * theData,
		       Uint32 len, Uint16 receiverBlockNo)
{
  const UtilUnlockConf *const sig = (UtilUnlockConf *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  return true;
}

bool
printUTIL_UNLOCK_REF (FILE * output, const Uint32 * theData,
		      Uint32 len, Uint16 receiverBlockNo)
{
  const UtilUnlockRef *const sig = (UtilUnlockRef *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  fprintf (output, " errorCode: %x\n", sig->errorCode);
  return true;
}

bool
printUTIL_CREATE_LOCK_REQ (FILE * output, const Uint32 * theData,
			   Uint32 len, Uint16 receiverBlockNo)
{
  const UtilCreateLockReq *const sig = (UtilCreateLockReq *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  fprintf (output, " lockType: %x\n", sig->lockType);
  return true;
}

bool
printUTIL_CREATE_LOCK_REF (FILE * output, const Uint32 * theData,
			   Uint32 len, Uint16 receiverBlockNo)
{
  const UtilCreateLockRef *const sig = (UtilCreateLockRef *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  fprintf (output, " errorCode: %x\n", sig->errorCode);
  return true;
}

bool
printUTIL_CREATE_LOCK_CONF (FILE * output, const Uint32 * theData,
			    Uint32 len, Uint16 receiverBlockNo)
{
  const UtilCreateLockConf *const sig = (UtilCreateLockConf *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  return true;
}

bool
printUTIL_DESTROY_LOCK_REQ (FILE * output, const Uint32 * theData,
			    Uint32 len, Uint16 receiverBlockNo)
{
  const UtilDestroyLockReq *const sig = (UtilDestroyLockReq *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  fprintf (output, " lockKey: %x\n", sig->lockKey);
  return true;
}

bool
printUTIL_DESTROY_LOCK_REF (FILE * output, const Uint32 * theData,
			    Uint32 len, Uint16 receiverBlockNo)
{
  const UtilDestroyLockRef *const sig = (UtilDestroyLockRef *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  fprintf (output, " errorCode: %x\n", sig->errorCode);
  return true;
}

bool
printUTIL_DESTROY_LOCK_CONF (FILE * output, const Uint32 * theData,
			     Uint32 len, Uint16 receiverBlockNo)
{
  const UtilDestroyLockConf *const sig = (UtilDestroyLockConf *) theData;
  fprintf (output, " senderData: %x\n", sig->senderData);
  fprintf (output, " senderRef: %x\n", sig->senderRef);
  fprintf (output, " lockId: %x\n", sig->lockId);
  return true;
}
