/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <signaldata/TupAccess.hpp>
#include <SignalLoggerManager.hpp>
#include <AttributeHeader.hpp>

bool
printTUP_READ_ATTRS(FILE* output, const Uint32* theData, Uint32 len, Uint16 rbn)
{
  const TupReadAttrs* const sig = (const TupReadAttrs*)theData;
  if (sig->errorCode == RNIL)
    fprintf(output, " errorCode=RNIL flags=%x\n", sig->requestInfo);
  else
    fprintf(output, " errorCode=%u flags=%x\n", sig->errorCode, sig->requestInfo);
  fprintf(output, " table: id=%u", sig->tableId);
  fprintf(output, " fragment: id=%u ptr=0x%x\n", sig->fragId, sig->fragPtrI);
  fprintf(output, " tuple: addr=0x%x version=%u", sig->tupAddr, sig->tupVersion);
  fprintf(output, " realPage=0x%x offset=%u\n", sig->pageId, sig->pageOffset);
  const Uint32* buffer = (const Uint32*)sig + TupReadAttrs::SignalLength;
  Uint32 attrCount = buffer[0];
  bool readKeys = (sig->requestInfo & TupReadAttrs::ReadKeys);
  if (sig->errorCode == RNIL && ! readKeys ||
      sig->errorCode == 0 && readKeys) {
    fprintf(output, " input: attrCount=%u\n", attrCount);
    for (unsigned i = 0; i < attrCount; i++) {
      AttributeHeader ah(buffer[1 + i]);
      fprintf(output, " %u: attrId=%u\n", i, ah.getAttributeId());
    }
  }
  if (sig->errorCode == 0) {
    fprintf(output, " output: attrCount=%u\n", attrCount);
    Uint32 pos = 1 + attrCount;
    for (unsigned i = 0; i < attrCount; i++) {
      AttributeHeader ah(buffer[pos++]);
      fprintf(output, " %u: attrId=%u dataSize=%u\n", i, ah.getAttributeId(), ah.getDataSize());
      Uint32 next = pos + ah.getDataSize();
      Uint32 printpos = 0;
      while (pos < next) {
        SignalLoggerManager::printDataWord(output, printpos, buffer[pos]);
        pos++;
      }
      if (ah.getDataSize() > 0)
        fprintf(output, "\n");
    }
  }
  return true;
}

bool
printTUP_QUERY_TH(FILE* output, const Uint32* theData, Uint32 len, Uint16 rbn)
{
  const TupQueryTh* const sig = (const TupQueryTh*)theData;
  fprintf(output, "tableId = %u, fragId = %u ", sig->tableId, sig->fragId);
  fprintf(output, "tuple: addr = 0x%x version = %u\n", sig->tupAddr,
          sig->tupVersion);
  fprintf(output, "transId1 = 0x%x, transId2 = 0x%x, savePointId = %u\n",
          sig->transId1, sig->transId2, sig->savePointId);
  return true;
}

bool
printTUP_STORE_TH(FILE* output, const Uint32* theData, Uint32 len, Uint16 rbn)
{
  const TupStoreTh* const sig = (const TupStoreTh*)theData;
  if (sig->errorCode == RNIL)
    fprintf(output, " errorCode=RNIL\n");
  else
    fprintf(output, " errorCode=%u\n", sig->errorCode);
  fprintf(output, " table: id=%u", sig->tableId);
  fprintf(output, " fragment: id=%u ptr=0x%x\n", sig->fragId, sig->fragPtrI);
  fprintf(output, " tuple: addr=0x%x", sig->tupAddr);
  if ((sig->tupAddr & 0x1) == 0) {
    fprintf(output, " fragPage=0x%x index=%u",
        sig->tupAddr >> MAX_TUPLES_BITS,
        (sig->tupAddr & ((1 <<MAX_TUPLES_BITS) - 1)) >> 1);
    fprintf(output, " realPage=0x%x offset=%u\n", sig->pageId, sig->pageOffset);
  } else {
    fprintf(output, " cacheId=%u\n",
        sig->tupAddr >> 1);
  }
  if (sig->tupVersion != 0) {
    fprintf(output, " version=%u ***invalid***\n", sig->tupVersion);
  }
  bool showdata = true;
  switch (sig->opCode) {
  case TupStoreTh::OpRead:
    fprintf(output, " operation=Read\n");
    showdata = false;
    break;
  case TupStoreTh::OpInsert:
    fprintf(output, " operation=Insert\n");
    break;
  case TupStoreTh::OpUpdate:
    fprintf(output, " operation=Update\n");
    break;
  case TupStoreTh::OpDelete:
    fprintf(output, " operation=Delete\n");
    showdata = false;
    break;
  default:
    fprintf(output, " operation=%u ***invalid***\n", sig->opCode);
    break;
  }
  fprintf(output, " data: offset=%u size=%u", sig->dataOffset, sig->dataSize);
  if (! showdata) {
    fprintf(output, " [not printed]\n");
  } else {
    fprintf(output, "\n");
    const Uint32* buffer = (const Uint32*)sig + TupStoreTh::SignalLength;
    Uint32 pos = 0;
    while (pos < sig->dataSize)
      SignalLoggerManager::printDataWord(output, pos, buffer[sig->dataOffset + pos]);
    if (sig->dataSize > 0)
      fprintf(output, "\n");
  }
  return true;
};
