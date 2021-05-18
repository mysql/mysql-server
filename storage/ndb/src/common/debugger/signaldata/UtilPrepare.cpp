/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <signaldata/UtilPrepare.hpp>

bool 
printUTIL_PREPARE_REQ(FILE* out, const Uint32 * data, Uint32 len, Uint16 rec)
{
  UtilPrepareReq* sig = (UtilPrepareReq*)data;
  fprintf(out, " senderRef: H'%.8x senderData: H'%.8x schemaTransId: H'%.8x\n",
	  sig->senderRef,
	  sig->senderData,
          sig->schemaTransId);

  return true;
}

bool 
printUTIL_PREPARE_CONF(FILE* out, const Uint32 * data, Uint32 len, Uint16 rec)
{
  UtilPrepareConf* sig = (UtilPrepareConf*)data;
  fprintf(out, " senderData: H'%.8x prepareId: %d\n",
	  sig->senderData,
	  sig->prepareId);
  return true;
}

bool 
printUTIL_PREPARE_REF(FILE* out, const Uint32 * data, Uint32 len, Uint16 rec)
{
  UtilPrepareRef* sig = (UtilPrepareRef*)data;
  fprintf(out, " senderData: H'%.8x, ", sig->senderData);
  fprintf(out, " error: %d, ", sig->errorCode);

  fprintf(out, " errorMsg: ");
  switch(sig->errorCode) {
  case UtilPrepareRef::PREPARE_REF_NO_ERROR:
    fprintf(out, "No error");
    break;
  case UtilPrepareRef::PREPARE_SEIZE_ERROR:
    fprintf(out, "Failed to seize Prepare record");
    break;
  case UtilPrepareRef::PREPARED_OPERATION_SEIZE_ERROR:
    fprintf(out, "Failed to seize PreparedOperation record");
    break;
  case UtilPrepareRef::DICT_TAB_INFO_ERROR:
    fprintf(out, "Failed to get table info from DICT");
    break;
  }
  fprintf(out, "\n");
  return true;
}
