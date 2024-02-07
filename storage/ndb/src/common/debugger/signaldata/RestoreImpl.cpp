/*
   Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
#include <signaldata/RestoreImpl.hpp>

bool printRESTORE_LCP_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*receiverBlockNo*/) {
  if (len < RestoreLcpReq::SignalLength) {
    assert(false);
    return false;
  }
  const RestoreLcpReq *const sig = (const RestoreLcpReq *)theData;
  fprintf(output, "senderData: H'%.8x, senderRef: H'%.8x, lcpNo: %u\n",
          sig->senderData, sig->senderRef, sig->lcpNo);
  fprintf(output,
          "tableId: %u, fragmentId: %u, lcpId: %u, restoreGcpId: %u"
          ", maxGciCompleted: %u, createGci: %u\n",
          sig->tableId, sig->fragmentId, sig->lcpId, sig->restoreGcpId,
          sig->maxGciCompleted, sig->createGci);
  return true;
}

bool printRESTORE_LCP_REF(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*receiverBlockNo*/) {
  const RestoreLcpRef *const sig = (const RestoreLcpRef *)theData;
  fprintf(output, "senderData: H'%.8x, senderRef: H'%.8x, errorCode: %u\n",
          sig->senderData, sig->senderRef, sig->errorCode);
  for (Uint32 i = 3; i < len; i++) {
    fprintf(output, "extra[%u]: %u", (i - 3), theData[i]);
  }
  fprintf(output, "\n");
  return true;
}

bool printRESTORE_LCP_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                           Uint16 /*receiverBlockNo*/) {
  if (len < RestoreLcpConf::SignalLength) {
    assert(false);
    return false;
  }
  const RestoreLcpConf *const sig = (const RestoreLcpConf *)theData;
  fprintf(output, "senderData: H'%.8x, senderRef: H'%.8x, restoredLcpId: %u",
          sig->senderData, sig->senderRef, sig->restoredLcpId);
  fprintf(output, ", restoredLocalLcpId: %u\n", sig->restoredLocalLcpId);
  fprintf(output, "maxGciCompleted: %u, afterRestore: %u\n",
          sig->maxGciCompleted, sig->afterRestore);
  return true;
}
