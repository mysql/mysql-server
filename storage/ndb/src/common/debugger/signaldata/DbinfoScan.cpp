/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.
   Use is subject to license terms.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <signaldata/DbinfoScan.hpp>

bool printDBINFO_SCAN(FILE *output, const Uint32 *theData, Uint32 len, Uint16) {
  if (len < DbinfoScan::SignalLength) {
    assert(false);
    return false;
  }

  const DbinfoScan *sig = (const DbinfoScan *)theData;
  fprintf(output, " resultData: 0x%x", sig->resultData);
  fprintf(output, " transid: { 0x%x, 0x%x}", sig->transId[0], sig->transId[1]);
  fprintf(output, " resultRef: 0x%x", sig->resultRef);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " colBitmap: { 0x%x, 0x%x }", sig->colBitmap[0],
          sig->colBitmap[1]);
  fprintf(output, " requestInfo: 0x%x", sig->requestInfo);
  fprintf(output, "\n");
  fprintf(output, " maxRows: %u", sig->maxRows);
  fprintf(output, " maxBytes: %u", sig->maxBytes);
  fprintf(output, "\n");
  fprintf(output, " returnedRows: %u", sig->returnedRows);
  fprintf(output, "\n");
  fprintf(output, " cursor_sz: %u\n", sig->cursor_sz);
  const Uint32 *cursor_data = DbinfoScan::getCursorPtr(sig);
  fprintf(output, " senderRef: 0x%x saveSenderRef: 0x%x\n", cursor_data[0],
          cursor_data[1]);
  fprintf(output, " currRef: 0x%x saveCurrRef: 0x%x flags: 0x%x\n",
          cursor_data[2], cursor_data[3], cursor_data[4]);
  fprintf(output, " data: [ 0x%x, 0x%x, 0x%x, 0x%x ]\n", cursor_data[5],
          cursor_data[6], cursor_data[7], cursor_data[8]);
  fprintf(output, " totalRows: %u totalBytes: %u\n", cursor_data[9],
          cursor_data[10]);
  return true;
}

bool printDBINFO_SCAN_REF(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16) {
  if (len < DbinfoScanRef::SignalLength) {
    assert(false);
    return false;
  }

  const DbinfoScanRef *sig = (const DbinfoScanRef *)theData;
  fprintf(output, " resultData: 0x%x", sig->resultData);
  fprintf(output, " transid: { 0x%x, 0x%x}", sig->transId[0], sig->transId[1]);
  fprintf(output, " resultRef: 0x%x", sig->resultRef);
  fprintf(output, "\n");
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, "\n");
  return true;
}
