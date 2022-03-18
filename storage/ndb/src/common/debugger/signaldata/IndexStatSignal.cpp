/* Copyright (c) 2003, 2022, Oracle and/or its affiliates.
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

#include <signaldata/IndexStatSignal.hpp>

static void
get_req_rt_name(Uint32 rt, char* rt_name)
{
  strcpy(rt_name, "Unknown");
#define set_req_rt_name(x) if (rt == IndexStatReq::x) strcpy(rt_name, #x)
  set_req_rt_name(RT_UPDATE_STAT);
  set_req_rt_name(RT_CLEAN_NEW);
  set_req_rt_name(RT_SCAN_FRAG);
  set_req_rt_name(RT_CLEAN_OLD);
  set_req_rt_name(RT_START_MON);
  set_req_rt_name(RT_DELETE_STAT);
  set_req_rt_name(RT_STOP_MON);
  set_req_rt_name(RT_DROP_HEAD);
  set_req_rt_name(RT_CLEAN_ALL);
#undef set_req_rt_name
}

static void
get_rep_rt_name(Uint32 rt, char* rt_name)
{
  strcpy(rt_name, "Unknown");
#define set_rep_rt_name(x) if (rt == IndexStatRep::x) strcpy(rt_name, #x)
  set_rep_rt_name(RT_UPDATE_REQ);
  set_rep_rt_name(RT_UPDATE_CONF);
#undef set_rep_rt_name
}

bool
printINDEX_STAT_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < IndexStatReq::SignalLength)
  {
    assert(false);
    return false;
  }

  const IndexStatReq* sig = (const IndexStatReq*)theData;
  fprintf(output, " clientRef: 0x%x", sig->clientRef);
  fprintf(output, " clientData: %u", sig->clientData);
  fprintf(output, "\n");
  Uint32 rt = sig->requestInfo & 0xFF;
  char rt_name[40];
  get_req_rt_name(rt, rt_name);
  fprintf(output, " requestType: %s[%u]", rt_name, rt);
  fprintf(output, " requestFlag: 0x%x", sig->requestFlag);
  fprintf(output, "\n");
  fprintf(output, " indexId: %u", sig->indexId);
  fprintf(output, " indexVersion: %u", sig->indexVersion);
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, "\n");
  return true;
}

bool
printINDEX_STAT_IMPL_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < IndexStatImplReq::SignalLength)
  {
    assert(false);
    return false;
  }

  const IndexStatImplReq* sig = (const IndexStatImplReq*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  Uint32 rt = sig->requestType;
  char rt_name[40];
  get_req_rt_name(rt, rt_name);
  fprintf(output, " requestType: %s[%u]", rt_name, rt);
  fprintf(output, " requestFlag: 0x%x", sig->requestFlag);
  fprintf(output, "\n");
  fprintf(output, " indexId: %u", sig->indexId);
  fprintf(output, " indexVersion: %u", sig->indexVersion);
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " fragId: %u", sig->fragId);
  fprintf(output, " fragCount: %u", sig->fragCount);
  fprintf(output, "\n");
  return true;
}

bool
printINDEX_STAT_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < IndexStatConf::SignalLength)
  {
    assert(false);
    return false;
  }

  const IndexStatConf* sig = (const IndexStatConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  return true;
}

bool
printINDEX_STAT_IMPL_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < IndexStatImplConf::SignalLength)
  {
    assert(false);
    return false;
  }

  const IndexStatImplConf* sig = (const IndexStatImplConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  return true;
}

bool
printINDEX_STAT_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < IndexStatRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const IndexStatRef* sig = (const IndexStatRef*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine);
  fprintf(output, "\n");
  return true;
}

bool
printINDEX_STAT_IMPL_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < IndexStatImplRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const IndexStatImplRef* sig = (const IndexStatImplRef*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine);
  fprintf(output, "\n");
  return true;
}

bool
printINDEX_STAT_REP(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < IndexStatRep::SignalLength)
  {
    assert(false);
    return false;
  }

  const IndexStatRep* sig = (const IndexStatRep*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  Uint32 rt = sig->requestType;
  char rt_name[40];
  get_rep_rt_name(rt, rt_name);
  fprintf(output, " requestType: %s[%u]", rt_name, rt);
  fprintf(output, " requestFlag: 0x%x", sig->requestFlag);
  fprintf(output, "\n");
  fprintf(output, " indexId: %u", sig->indexId);
  fprintf(output, " indexVersion: %u", sig->indexVersion);
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, "\n");
  fprintf(output, " fragId: %u", sig->fragId);
  fprintf(output, " loadTime: %u", sig->loadTime);
  fprintf(output, "\n");
  return true;
}
