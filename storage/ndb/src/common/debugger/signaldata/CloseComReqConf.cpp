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
#include <signaldata/CloseComReqConf.hpp>

bool printCLOSECOMREQCONF(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*receiverBlockNo*/) {
  const CloseComReqConf *cc = (const CloseComReqConf *)theData;

  if (len == 1) {
    fprintf(output, " xxxBlockRef = (%d, %d)\n", refToBlock(cc->xxxBlockRef),
            refToNode(cc->xxxBlockRef));
  } else {
    fprintf(output, " xxxBlockRef = (%d, %d) failNo = %d noOfNodes = %d\n",
            refToBlock(cc->xxxBlockRef), refToNode(cc->xxxBlockRef), cc->failNo,
            cc->noOfNodes);

    if (cc->failedNodeId == 0) {
      fprintf(output, " Nodes in bitmask\n");
    } else {
      fprintf(output, " Node: %d\n", cc->failedNodeId);
    }
  }
  return true;
}
