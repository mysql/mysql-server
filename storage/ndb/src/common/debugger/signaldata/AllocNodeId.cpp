/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <signaldata/AllocNodeId.hpp>
#include <RefConvert.hpp>

bool printALLOC_NODEID_REQ(FILE *output,
                           const Uint32 *theData,
                           Uint32 len,
                           Uint16 /*recBlockNo*/)
{
  const AllocNodeIdReq *sig = (const AllocNodeIdReq *)&theData[0];

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

bool printALLOC_NODEID_CONF(FILE *output,
                            const Uint32 *theData,
                            Uint32 len,
                            Uint16 /*recBlockNo*/)
{
  const AllocNodeIdConf *sig = (const AllocNodeIdConf *)&theData[0];

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

bool printALLOC_NODEID_REF(FILE *output,
                           const Uint32 *theData,
                           Uint32 len,
                           Uint16 /*recBlockNo*/)
{
  const AllocNodeIdRef *sig = (const AllocNodeIdRef *)&theData[0];

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
