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

#include <signaldata/CopyGCIReq.hpp>
#include <BaseString.hpp>

static
void
print(char * buf, size_t buf_len, CopyGCIReq::CopyReason r){
  switch(r){
  case CopyGCIReq::IDLE:
    BaseString::snprintf(buf, buf_len, "IDLE");
    break;
  case CopyGCIReq::LOCAL_CHECKPOINT:
    BaseString::snprintf(buf, buf_len, "LOCAL_CHECKPOINT");
    break;
  case CopyGCIReq::RESTART:
    BaseString::snprintf(buf, buf_len, "RESTART");
    break;
  case CopyGCIReq::GLOBAL_CHECKPOINT:
    BaseString::snprintf(buf, buf_len, "GLOBAL_CHECKPOINT");
    break;
  case CopyGCIReq::INITIAL_START_COMPLETED:
    BaseString::snprintf(buf, buf_len, "INITIAL_START_COMPLETED");
    break;
  default:
    BaseString::snprintf(buf, buf_len, "<Unknown>");
  }
}

bool printCOPY_GCI_REQ(FILE* output,
                       const Uint32* theData,
                       Uint32 len,
                       Uint16 /*recBlockNo*/)
{
  // Use SignalLength, since the data[] of size 22 is not written out
  if (len < CopyGCIReq::SignalLength)
  {
    assert(false);
    return false;
  }

  const CopyGCIReq* sig = (const CopyGCIReq*)theData;

  static char buf[255];
  print(buf, sizeof(buf), (CopyGCIReq::CopyReason)sig->copyReason);

  fprintf(output, " SenderData: %d CopyReason: %s StartWord: %d\n",
	  sig->anyData,
	  buf,
	  sig->startWord);
  return false;
}
