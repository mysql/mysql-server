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

#include <BlockNumbers.h>
#include <kernel_types.h>
#include <signaldata/PrepFailReqRef.hpp>

bool printPREPFAILREQREF(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  const PrepFailReqRef *cc = (const PrepFailReqRef *)theData;

  fprintf(output, " xxxBlockRef = (%d, %d) failNo = %d noOfNodes = %d\n",
          refToBlock(cc->xxxBlockRef), refToNode(cc->xxxBlockRef), cc->failNo,
          cc->noOfNodes);

  int hits = 0;
  if (len == cc->SignalLength_v1) {
    fprintf(output, " Nodes: ");
    for (int i = 0; i < MAX_NDB_NODES_v1; i++) {
      if (NdbNodeBitmask48::get(cc->theNodes, i)) {
        hits++;
        fprintf(output, " %d", i);
      }
      if (hits == 16) {
        fprintf(output, "\n Nodes: ");
        hits = 0;
      }
    }
    if (hits != 0) fprintf(output, "\n");
  } else {
    fprintf(output, " theNodes in signal section\n");
  }
  return true;
}
