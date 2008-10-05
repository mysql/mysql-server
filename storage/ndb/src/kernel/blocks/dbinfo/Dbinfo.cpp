/* Copyright (C) 2007 MySQL AB

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

#include "Dbinfo.hpp"

Dbinfo::Dbinfo(Block_context& ctx) :
  SimulatedBlock(DBINFO, ctx)
{
  BLOCK_CONSTRUCTOR(Dbinfo);

  /* Add Received Signals */
  addRecSignal(GSN_STTOR, &Dbinfo::execSTTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &Dbinfo::execDUMP_STATE_ORD);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbinfo::execREAD_CONFIG_REQ, true);
}

Dbinfo::~Dbinfo()
{
  /* Nothing */
}

BLOCK_FUNCTIONS(Dbinfo)

void Dbinfo::execSTTOR(Signal *signal)
{
  jamEntry();

  sendSTTORRY(signal);
  return;
}

void Dbinfo::execREAD_CONFIG_REQ(Signal *signal)
{
  jamEntry();
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  /* In the future, do something sensible here. */

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal,
	     ReadConfigConf::SignalLength, JBB);
}

void Dbinfo::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 6, JBB);
}

void Dbinfo::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();

  /* Do some coool DUMP stuff here */
}
