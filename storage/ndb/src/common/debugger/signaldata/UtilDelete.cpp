/*
   Copyright (C) 2003, 2005, 2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <signaldata/UtilDelete.hpp>

bool 
printUTIL_DELETE_REQ(FILE * out, const Uint32 * data, Uint32 l, Uint16 b){
  (void)l;  // Don't want compiler warning
  (void)b;  // Don't want compiler warning

  UtilDeleteReq* sig = (UtilDeleteReq*)data;
  fprintf(out, " senderData: %d prepareId: %d totalDataLen: %d\n",
	  sig->senderData,
	  sig->prepareId,
	  sig->totalDataLen);
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
          sig->attrData[21]
          );

  return true;
}

bool 
printUTIL_DELETE_CONF(FILE * out, const Uint32 * data, Uint32 l, Uint16 b){
  (void)l;  // Don't want compiler warning
  (void)b;  // Don't want compiler warning

  UtilDeleteConf* sig = (UtilDeleteConf*)data;
  fprintf(out, " senderData: %d\n", sig->senderData);
  return true;
}

bool 
printUTIL_DELETE_REF(FILE * out, const Uint32 * data, Uint32 l, Uint16 b){
  (void)l;  // Don't want compiler warning
  (void)b;  // Don't want compiler warning

  UtilDeleteRef* sig = (UtilDeleteRef*)data;
  fprintf(out, " senderData: %d\n", sig->senderData);
  fprintf(out, " errorCode: %d\n", sig->errorCode);
  return true;
}
