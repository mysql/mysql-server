/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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

#include <signaldata/CntrStart.hpp>

bool printCNTR_START_REQ(FILE *output,
                         const Uint32 *theData,
                         Uint32 len,
                         Uint16 /*receiverBlockNo*/
)
{
  // Using old signal length, since lastLcpId is not written out
  if (len < CntrStartReq::OldSignalLength)
  {
    assert(false);
    return false;
  }

  const CntrStartReq *const sig = (const CntrStartReq *)theData;
  fprintf(output, " nodeId: %x\n", sig->nodeId);
  fprintf(output, " startType: %x\n", sig->startType);
  fprintf(output, " lastGci: %x\n", sig->lastGci);
  return true;
}

bool printCNTR_START_REF(FILE *output,
                         const Uint32 *theData,
                         Uint32 len,
                         Uint16 /*receiverBlockNo*/)
{
  if (len < CntrStartRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const CntrStartRef *const sig = (const CntrStartRef *)theData;
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  fprintf(output, " masterNodeId: %x\n", sig->masterNodeId);
  return true;
}

bool printCNTR_START_CONF(FILE *output,
                          const Uint32 *theData,
                          Uint32 len,
                          Uint16 /*receiverBlockNo*/)
{
  const CntrStartConf *const sig = (const CntrStartConf *)theData;
  fprintf(output, " startType: %x\n", sig->startType);
  fprintf(output, " startGci: %x\n", sig->startGci);
  fprintf(output, " masterNodeId: %x\n", sig->masterNodeId);
  fprintf(output, " noStartNodes: %x\n", sig->noStartNodes);

  if (len == sig->SignalLength_v1)
  {
    char buf[NdbNodeBitmask::TextLength + 1];
    fprintf(output, " startedNodes: %s\n",
      BitmaskImpl::getText(NdbNodeBitmask48::Size, sig->startedNodes_v1, buf));
    fprintf(output, " startingNodes: %s\n",
      BitmaskImpl::getText(NdbNodeBitmask48::Size, sig->startingNodes_v1, buf));
  }
  else
  {
    fprintf(output, " startedNodes and startingNodes in signal section");
  }
  return true;
}
