/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#include <signaldata/UtilDelete.hpp>

bool printUTIL_DELETE_REQ(FILE *out, const Uint32 *data, Uint32 l, Uint16 b) {
  (void)l;  // Don't want compiler warning
  (void)b;  // Don't want compiler warning

  const UtilDeleteReq *sig = (const UtilDeleteReq *)data;
  fprintf(out, " senderData: %d prepareId: %d totalDataLen: %d\n",
          sig->senderData, sig->prepareId, sig->totalDataLen);
  fprintf(out,
          " H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x\n"
          " H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x\n"
          " H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x\n",
          sig->attrData[0], sig->attrData[1], sig->attrData[2],
          sig->attrData[3], sig->attrData[4], sig->attrData[5],
          sig->attrData[6], sig->attrData[7], sig->attrData[8],
          sig->attrData[9], sig->attrData[10], sig->attrData[11],
          sig->attrData[12], sig->attrData[13], sig->attrData[14],
          sig->attrData[15], sig->attrData[16], sig->attrData[17],
          sig->attrData[18], sig->attrData[19], sig->attrData[20],
          sig->attrData[21]);

  return true;
}

bool printUTIL_DELETE_CONF(FILE *out, const Uint32 *data, Uint32 l, Uint16 b) {
  (void)l;  // Don't want compiler warning
  (void)b;  // Don't want compiler warning

  const UtilDeleteConf *sig = (const UtilDeleteConf *)data;
  fprintf(out, " senderData: %d\n", sig->senderData);
  return true;
}

bool printUTIL_DELETE_REF(FILE *out, const Uint32 *data, Uint32 l, Uint16 b) {
  (void)l;  // Don't want compiler warning
  (void)b;  // Don't want compiler warning

  const UtilDeleteRef *sig = (const UtilDeleteRef *)data;
  fprintf(out, " senderData: %d\n", sig->senderData);
  fprintf(out, " errorCode: %d\n", sig->errorCode);
  return true;
}
