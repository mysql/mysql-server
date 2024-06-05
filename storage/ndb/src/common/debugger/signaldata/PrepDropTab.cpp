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

#include <signaldata/PrepDropTab.hpp>

bool printPREP_DROP_TAB_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                            Uint16 /*receiverBlockNo*/) {
  if (len < PrepDropTabReq::SignalLength) {
    assert(false);
    return false;
  }

  const PrepDropTabReq *const sig = (const PrepDropTabReq *)theData;

  fprintf(output, " senderRef: %x senderData: %d TableId: %d\n", sig->senderRef,
          sig->senderData, sig->tableId);
  return true;
}

bool printPREP_DROP_TAB_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                             Uint16 /*receiverBlockNo*/) {
  if (len < PrepDropTabConf::SignalLength) {
    assert(false);
    return false;
  }

  const PrepDropTabConf *const sig = (const PrepDropTabConf *)theData;

  fprintf(output, " senderRef: %x senderData: %d TableId: %d\n", sig->senderRef,
          sig->senderData, sig->tableId);

  return true;
}

bool printPREP_DROP_TAB_REF(FILE *output, const Uint32 *theData, Uint32 len,
                            Uint16 /*receiverBlockNo*/) {
  if (len < PrepDropTabReq::SignalLength) {
    assert(false);
    return false;
  }

  const PrepDropTabRef *const sig = (const PrepDropTabRef *)theData;

  fprintf(output, " senderRef: %x senderData: %d TableId: %d errorCode: %d\n",
          sig->senderRef, sig->senderData, sig->tableId, sig->errorCode);

  return true;
}
