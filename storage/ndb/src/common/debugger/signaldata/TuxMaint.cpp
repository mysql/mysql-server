/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.
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

#include <signaldata/TuxMaint.hpp>
#include <SignalLoggerManager.hpp>
#include <AttributeHeader.hpp>

bool printTUX_MAINT_REQ(FILE* output,
                        const Uint32* theData,
                        Uint32 len,
                        Uint16 /*rbn*/)
{
  if (len < TuxMaintReq::SignalLength)
  {
    assert(false);
    return false;
  }

  //const bool inOut = rbn & (1 << 15);
  const TuxMaintReq* const sig = (const TuxMaintReq*)theData;
  fprintf(output, " errorCode=%d\n", sig->errorCode);
  fprintf(output, " table: id=%u", sig->tableId);
  fprintf(output, " index: id=%u", sig->indexId);
  fprintf(output, " fragment: id=%u\n", sig->fragId);
  fprintf(output, " tuple: loc=%u.%u version=%u\n", sig->pageId, sig->pageIndex, sig->tupVersion);
  const Uint32 opCode = sig->opInfo & 0xFF;
  const Uint32 opFlag = sig->opInfo >> 8;
  switch (opCode ) {
  case TuxMaintReq::OpAdd:
    fprintf(output, " opCode=Add opFlag=%u\n", opFlag);
    break;
  case TuxMaintReq::OpRemove:
    fprintf(output, " opCode=Remove opFlag=%u\n", opFlag);
    break;
  default:
    fprintf(output, " opInfo=%x ***invalid***\n", sig->opInfo);
    break;
  }
  return true;
}
