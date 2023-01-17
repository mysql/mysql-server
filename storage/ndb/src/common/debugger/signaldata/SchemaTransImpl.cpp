/* Copyright (c) 2007, 2023, Oracle and/or its affiliates.
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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <signaldata/SchemaTransImpl.hpp>
#include <signaldata/DictSignal.hpp>
#include <signaldata/SignalData.hpp>
#include <SignalLoggerManager.hpp>
#include <DebuggerNames.hpp>

bool
printSCHEMA_TRANS_IMPL_REQ(FILE* output, const Uint32* theData,
                           Uint32 len, Uint16 rbn)
{
  const SchemaTransImplReq* sig = (const SchemaTransImplReq*)theData;
  //const Uint32 phaseInfo = sig->phaseInfo;
  //Uint32 mode = SchemaTransImplReq::getMode(phaseInfo);
  //Uint32 phase = SchemaTransImplReq::getPhase(phaseInfo);
  const Uint32 requestInfo = sig->requestInfo;
  const Uint32 rt = DictSignal::getRequestType(requestInfo);
  Uint32 opExtra = DictSignal::getRequestExtra(requestInfo);
  //const Uint32 operationInfo = sig->operationInfo;
  //Uint32 opIndex = SchemaTransImplReq::getOpIndex(operationInfo);
  //Uint32 opDepth = SchemaTransImplReq::getOpDepth(operationInfo);
  //const Uint32 iteratorInfo = sig->iteratorInfo;
  //Uint32 listId = SchemaTransImplReq::getListId(iteratorInfo);
  //Uint32 listIndex = SchemaTransImplReq::getListIndex(iteratorInfo);
  //Uint32 itRepeat = SchemaTransImplReq::getItRepeat(iteratorInfo);
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " transKey: %u", sig->transKey);
  fprintf(output, " opKey: %u", sig->opKey);
  fprintf(output, "\n");
/*
  fprintf(output, " mode: %u [%s] phase: %u [%s]",
          mode, DictSignal::getTransModeName(mode),
          phase, DictSignal::getTransPhaseName(phase));
  fprintf(output, "\n");
*/
  fprintf(output, " requestInfo: 0x%x", requestInfo);
  switch(rt) {
  case(SchemaTransImplReq::RT_START):
    fprintf(output, " RequestType: RT_START");
    break;
  case(SchemaTransImplReq::RT_PARSE):
    fprintf(output, " RequestType: RT_PARSE");
    break;
  case(SchemaTransImplReq::RT_FLUSH_PREPARE):
    fprintf(output, " RequestType: RT_FLUSH_PREPARE");
    break;
  case(SchemaTransImplReq::RT_PREPARE):
    fprintf(output, " RequestType: RT_PREPARE");
    break;
  case(SchemaTransImplReq::RT_ABORT_PARSE):
    fprintf(output, " RequestType: RT_ABORT_PARSE");
    break;
  case(SchemaTransImplReq::RT_ABORT_PREPARE):
    fprintf(output, " RequestType: RT_ABORT_PREPARE");
    break;
  case(SchemaTransImplReq::RT_FLUSH_COMMIT):
    fprintf(output, " RequestType: RT_FLUSH_COMMIT");
    break;
  case(SchemaTransImplReq::RT_COMMIT):
    fprintf(output, " RequestType: RT_COMMIT");
    break;
  case(SchemaTransImplReq::RT_FLUSH_COMPLETE):
    fprintf(output, " RequestType: RT_FLUSH_COMPLETE");
    break;
  case(SchemaTransImplReq::RT_COMPLETE):
    fprintf(output, " RequestType: RT_COMPLETE");
    break;
  case(SchemaTransImplReq::RT_END):
    fprintf(output, " RequestType: RT_END");
    break;
  }
  fprintf(output, " opExtra: %u", opExtra);
  fprintf(output, " requestFlags: [%s]",
          DictSignal::getRequestFlagsText(requestInfo));
  fprintf(output, "\n");
//  fprintf(output, " opIndex: %u", opIndex);
//  fprintf(output, " opDepth: %u", opDepth);
//  fprintf(output, "\n");
//  fprintf(output, " listId: %u", listId);
//  fprintf(output, " listIndex: %u", listIndex);
//  fprintf(output, " itRepeat: %u", itRepeat);
//  fprintf(output, "\n");
  if (len == SchemaTransImplReq::SignalLength)
    fprintf(output, " clientRef: 0x%x", sig->start.clientRef);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, "\n");
  const Uint32 fixed_len = SchemaTransImplReq::SignalLength;
  if (len > fixed_len) {
    Uint32 gsn = sig->parse.gsn;
    fprintf(output, "piggy-backed: %u %s\n", gsn, getSignalName(gsn));
    const Uint32* pb_data = &theData[fixed_len];
    const Uint32 pb_len = len - fixed_len;
    switch (gsn) {
      // internal operation signals
    case GSN_SCHEMA_TRANS_BEGIN_REQ:
      printSCHEMA_TRANS_BEGIN_REQ(output, pb_data, pb_len, rbn);
      break;
    case GSN_CREATE_TAB_REQ:
      printCREATE_TAB_REQ(output, pb_data, pb_len, rbn);
      break;
    case GSN_DROP_TAB_REQ:
      printDROP_TAB_REQ(output, pb_data, pb_len, rbn);
      break;
    case GSN_ALTER_TAB_REQ:
      printALTER_TAB_REQ(output, pb_data, pb_len, rbn);
      break;
    case GSN_CREATE_TRIG_IMPL_REQ:
      printCREATE_TRIG_IMPL_REQ(output, pb_data, pb_len, rbn);
      break;
    case GSN_DROP_TRIG_IMPL_REQ:
      printDROP_TRIG_IMPL_REQ(output, pb_data, pb_len, rbn);
      break;
    case GSN_CREATE_INDX_IMPL_REQ:
      printCREATE_INDX_IMPL_REQ(output, pb_data, pb_len, rbn);
      break;
    case GSN_DROP_INDX_IMPL_REQ:
      printDROP_INDX_IMPL_REQ(output, pb_data, pb_len, rbn);
      break;
    case GSN_ALTER_INDX_IMPL_REQ:
      printALTER_INDX_IMPL_REQ(output, pb_data, pb_len, rbn);
      break;
    case GSN_BUILD_INDX_IMPL_REQ:
      printBUILD_INDX_IMPL_REQ(output, pb_data, pb_len, rbn);
      break;
    case GSN_INDEX_STAT_IMPL_REQ:
      printINDEX_STAT_IMPL_REQ(output, pb_data, pb_len, rbn);
      break;
    default:
    {
      Uint32 i;
      for (i = 0; i < len - fixed_len; i++) {
        if (i > 0 && i % 7 == 0)
          fprintf(output, "\n");
        fprintf(output, " H'%08x", theData[fixed_len + i]);
      }
      fprintf(output, "\n");
    }
    break;
    }
  }
  return true;
}

bool printSCHEMA_TRANS_IMPL_CONF(FILE* output,
                                 const Uint32* theData,
                                 Uint32 len,
                                 Uint16 /*rbn*/)
{
  if (len < SchemaTransImplConf::SignalLength)
  {
    assert(false);
    return false;
  }

  const SchemaTransImplConf* sig = (const SchemaTransImplConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " transKey: %u", sig->transKey);
  fprintf(output, "\n");
  return true;
}

bool printSCHEMA_TRANS_IMPL_REF(FILE* output,
                                const Uint32* theData,
                                Uint32 len,
                                Uint16 /*rbn*/)
{
  if (len < SchemaTransImplRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const SchemaTransImplRef* sig = (const SchemaTransImplRef*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " transKey: %u", sig->transKey);
  fprintf(output, "\n");
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine);
  fprintf(output, "\n");
  return true;
}
