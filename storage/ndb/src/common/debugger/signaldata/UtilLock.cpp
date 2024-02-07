/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

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

#include <signaldata/UtilLock.hpp>

bool printUTIL_LOCK_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                        Uint16 /*receiverBlockNo*/) {
  if (len < UtilLockReq::SignalLength) {
    assert(false);
    return false;
  }

  const UtilLockReq *const sig = (const UtilLockReq *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  fprintf(output, " requestInfo: %x\n", sig->requestInfo);
  fprintf(output, " extra: %x\n", sig->extra);
  return true;
}

bool printUTIL_LOCK_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  if (len < UtilLockConf::SignalLength) {
    assert(false);
    return false;
  }

  const UtilLockConf *const sig = (const UtilLockConf *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  fprintf(output, " extra: %x\n", sig->extra);
  return true;
}

bool printUTIL_LOCK_REF(FILE *output, const Uint32 *theData, Uint32 len,
                        Uint16 /*receiverBlockNo*/) {
  if (len < UtilLockRef::SignalLength) {
    assert(false);
    return false;
  }

  const UtilLockRef *const sig = (const UtilLockRef *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  fprintf(output, " extra: %x\n", sig->extra);
  return true;
}

bool printUTIL_UNLOCK_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*receiverBlockNo*/) {
  if (len < UtilUnlockReq::SignalLength) {
    assert(false);
    return false;
  }

  const UtilUnlockReq *const sig = (const UtilUnlockReq *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  return true;
}

bool printUTIL_UNLOCK_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                           Uint16 /*receiverBlockNo*/) {
  if (len < UtilUnlockConf::SignalLength) {
    assert(false);
    return false;
  }

  const UtilUnlockConf *const sig = (const UtilUnlockConf *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  return true;
}

bool printUTIL_UNLOCK_REF(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*receiverBlockNo*/) {
  if (len < UtilUnlockRef::SignalLength) {
    assert(false);
    return false;
  }

  const UtilUnlockRef *const sig = (const UtilUnlockRef *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return true;
}

bool printUTIL_CREATE_LOCK_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                               Uint16 /*receiverBlockNo*/) {
  if (len < UtilCreateLockReq::SignalLength) {
    assert(false);
    return false;
  }

  const UtilCreateLockReq *const sig = (const UtilCreateLockReq *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  fprintf(output, " lockType: %x\n", sig->lockType);
  return true;
}

bool printUTIL_CREATE_LOCK_REF(FILE *output, const Uint32 *theData, Uint32 len,
                               Uint16 /*receiverBlockNo*/) {
  if (len < UtilCreateLockRef::SignalLength) {
    assert(false);
    return false;
  }

  const UtilCreateLockRef *const sig = (const UtilCreateLockRef *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return true;
}

bool printUTIL_CREATE_LOCK_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                                Uint16 /*receiverBlockNo*/) {
  if (len < UtilCreateLockConf::SignalLength) {
    assert(false);
    return false;
  }

  const UtilCreateLockConf *const sig = (const UtilCreateLockConf *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  return true;
}

bool printUTIL_DESTROY_LOCK_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                                Uint16 /*receiverBlockNo*/) {
  if (len < UtilDestroyLockReq::SignalLength) {
    assert(false);
    return false;
  }

  const UtilDestroyLockReq *const sig = (const UtilDestroyLockReq *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  return true;
}

bool printUTIL_DESTROY_LOCK_REF(FILE *output, const Uint32 *theData, Uint32 len,
                                Uint16 /*receiverBlockNo*/) {
  if (len < UtilDestroyLockRef::SignalLength) {
    assert(false);
    return false;
  }

  const UtilDestroyLockRef *const sig = (const UtilDestroyLockRef *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return true;
}

bool printUTIL_DESTROY_LOCK_CONF(FILE *output, const Uint32 *theData,
                                 Uint32 len, Uint16 /*receiverBlockNo*/) {
  if (len < UtilDestroyLockConf::SignalLength) {
    assert(false);
    return false;
  }

  const UtilDestroyLockConf *const sig = (const UtilDestroyLockConf *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " lockId: %x\n", sig->lockId);
  return true;
}
