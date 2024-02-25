/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.
    Use is subject to license terms.

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

#include <signaldata/CreateEvnt.hpp>

static void print_request_info(FILE *output,
                               CreateEvntReq::RequestType req_type,
                               Uint32 req_flags) {
  fprintf(output, " requestType: ");
  switch(req_type) {
    case CreateEvntReq::RT_UNDEFINED:
      fprintf(output, "'Undefined'");
      break;
    case CreateEvntReq::RT_USER_CREATE:
      fprintf(output, "'Create'");
      break;
    case CreateEvntReq::RT_USER_GET:
      fprintf(output, "'Get'");
      break;
    default:
      fprintf(output, "0x%08x", req_type);
      break;
  }
  if (req_flags) {
    fprintf(output, " flags: 0x%08x [", req_flags);
    if (req_flags & CreateEvntReq::RT_DICT_AFTER_GET)
      fprintf(output, "DICT_AFTER_GET ");
    fprintf(output, "]");
  }
  fprintf(output, "\n");
}

bool printCREATE_EVNT_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16) {
  if (len < CreateEvntReq::SignalLengthGet)
  {
    assert(false);
    return false;
  }

  const CreateEvntReq * const sig = (const CreateEvntReq *) theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  print_request_info(output, sig->getRequestType(), sig->getRequestFlag());

  if (len <= CreateEvntReq::SignalLengthGet)
    return true;

  fprintf(output, " tableId: %u tableVersion: %u\n",
          sig->m_tableId, sig->m_tableVersion);
  // attrListBitmask;
  fprintf(output, " m_eventType: 0x%08x [eventType: %u, reportFlags: 0x%08x]\n",
          sig->m_eventType, sig->getEventType(), sig->getReportFlags());
  fprintf(output, " eventId: %u eventKey: %u\n", sig->m_eventId,
          sig->m_eventKey);

  return false;
}

bool printCREATE_EVNT_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                           Uint16) {
  if (len < CreateEvntConf::SignalLength_v8_0_31)
  {
    assert(false);
    return false;
  }

  const CreateEvntConf * const sig = (const CreateEvntConf *) theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  print_request_info(output, sig->getRequestType(), 0);
  fprintf(output, " tableId: %u tableVersion: %u\n", sig->m_tableId, sig->m_tableVersion);
  // attrListBitmask;
  fprintf(output, " m_eventType: 0x%08x [eventType: %u]\n", sig->m_eventType, sig->getEventType());
  fprintf(output, " eventId: %u eventKey: %u\n", sig->m_eventId, sig->m_eventKey);
  if (len > CreateEvntConf::SignalLength_v8_0_31) {
    fprintf(output, " reportFlags: 0x%08x\n", sig->m_reportFlags);
  }

  return false;
}

bool printCREATE_EVNT_REF(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16) {
  if (len < CreateEvntRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const CreateEvntRef * const sig = (const CreateEvntRef *) theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  print_request_info(output, sig->getRequestType(), 0);
  fprintf(output, " errorCode: %u\n", sig->errorCode);
  fprintf(output, " errorLine: %u\n", sig->m_errorLine);
  fprintf(output, " errorRef: 0x%08x\n", sig->m_errorNode);
  if (len >= CreateEvntRef::SignalLength2)
    fprintf(output, " masterNodeId: %u\n", sig->m_masterNodeId);

  return false;
}
