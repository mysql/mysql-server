/* Copyright (c) 2008, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include <signaldata/AllocNodeId.hpp>
#include <RefConvert.hpp>

bool
printALLOC_NODEID_REQ(FILE * output,
                     const Uint32 * theData,
                     Uint32 len,
                     Uint16 recBlockNo)
{
  AllocNodeIdReq * sig = (AllocNodeIdReq *)&theData[0];

  switch (len)
  {
  case AllocNodeIdReq::SignalLength:
    fprintf(output,
            " senderRef: (node: %d, block: %d)\n"
            " senderData: %u\n"
            " nodeId: %u\n"
            " nodeType: %u\n"
            " timeout: %u\n",
            refToNode(sig->senderRef), refToBlock(sig->senderRef),
            sig->senderData,
            sig->nodeId,
            sig->nodeType,
            sig->timeout);
    return true;
  case AllocNodeIdReq::SignalLengthQMGR:
    fprintf(output,
            " senderRef: (node: %d, block: %d)\n"
            " senderData: %u\n"
            " nodeId: %u\n"
            " nodeType: %u\n"
            " timeout: %u\n"
            " secret: %08x %08x\n",
            refToNode(sig->senderRef), refToBlock(sig->senderRef),
            sig->senderData,
            sig->nodeId,
            sig->nodeType,
            sig->timeout,
            sig->secret_hi, sig->secret_lo);
    return true;
  }
  return false;
}

bool
printALLOC_NODEID_CONF(FILE * output,
                      const Uint32 * theData,
                      Uint32 len,
                      Uint16 recBlockNo)
{
  AllocNodeIdConf * sig = (AllocNodeIdConf *)&theData[0];

  if (len == AllocNodeIdConf::SignalLength)
  {
    fprintf(output,
            " senderRef: (node: %d, block: %d)\n"
            " senderData: %u\n"
            " nodeId: %u\n"
            " secret: %08x %08x\n",
            refToNode(sig->senderRef), refToBlock(sig->senderRef),
            sig->senderData,
            sig->nodeId,
            sig->secret_hi, sig->secret_lo);
    return true;
  }
  return false;
}

static
const char *
get_text_AllocNodeIdRef_ErrorCodes(Uint32 errorCode)
{
  switch (errorCode)
  {
  case AllocNodeIdRef::NoError: return "NoError";
  case AllocNodeIdRef::Undefined: return "Undefined";
  case AllocNodeIdRef::NF_FakeErrorREF: return "NF_FakeErrorREF";
  case AllocNodeIdRef::Busy: return "Busy";
  case AllocNodeIdRef::NotMaster: return "NotMaster";
  case AllocNodeIdRef::NodeReserved: return "NodeReserved";
  case AllocNodeIdRef::NodeConnected: return "NodeConnected";
  case AllocNodeIdRef::NodeFailureHandlingNotCompleted: return "NodeFailureHandlingNotCompleted";
  case AllocNodeIdRef::NodeTypeMismatch: return "NodeTypeMismatch";
  default: return "<Unknown error code>";
  }
}

bool
printALLOC_NODEID_REF(FILE * output,
                      const Uint32 * theData,
                      Uint32 len,
                      Uint16 recBlockNo)
{
  AllocNodeIdRef * sig = (AllocNodeIdRef *)&theData[0];

  if (len == AllocNodeIdRef::SignalLength)
  {
    fprintf(output,
            " senderRef: (node: %d, block: %d)\n"
            " senderData: %u\n"
            " nodeId: %u\n"
            " errorCode: %u %s\n"
            " masterRef: (node: %d, block: %d)\n",
            refToNode(sig->senderRef), refToBlock(sig->senderRef),
            sig->senderData,
            sig->nodeId,
            sig->errorCode, get_text_AllocNodeIdRef_ErrorCodes(sig->errorCode),
            refToNode(sig->masterRef), refToBlock(sig->masterRef));
    return true;
  }
  return false;
}
