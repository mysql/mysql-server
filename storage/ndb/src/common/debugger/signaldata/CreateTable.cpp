/* Copyright (c) 2007, 2024, Oracle and/or its affiliates.
   Use is subject to license terms

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <signaldata/CreateTable.hpp>

bool printCREATE_TABLE_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                           Uint16) {
  if (len < CreateTableReq::SignalLength) {
    assert(false);
    return false;
  }

  const CreateTableReq *sig = (const CreateTableReq *)theData;
  fprintf(output, " clientRef: 0x%x", sig->clientRef);
  fprintf(output, " clientData: %u", sig->clientData);
  fprintf(output, " requestInfo: %x", sig->requestInfo);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, " transKey: %u", sig->transKey);
  fprintf(output, "\n");
  return true;
}

bool printCREATE_TABLE_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                            Uint16) {
  if (len < CreateTableConf::SignalLength) {
    assert(false);
    return false;
  }

  const CreateTableConf *sig = (const CreateTableConf *)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " clientData: %u", sig->clientData);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " tableVersion: %u", sig->tableVersion);
  fprintf(output, "\n");
  return true;
}

bool printCREATE_TABLE_REF(FILE *output, const Uint32 *theData, Uint32 len,
                           Uint16) {
  if (len < CreateTableRef::SignalLength) {
    assert(false);
    return false;
  }

  const CreateTableRef *sig = (const CreateTableRef *)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " clientData: %u", sig->clientData);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, "\n");
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine);
  fprintf(output, " errorNodeId: %u", sig->errorNodeId);
  fprintf(output, " masterNodeId: %u", sig->masterNodeId);
  fprintf(output, "\n");
  fprintf(output, " errorStatus: %u", sig->errorStatus);
  fprintf(output, " errorKey: %u", sig->errorKey);
  fprintf(output, "\n");
  return true;
}
