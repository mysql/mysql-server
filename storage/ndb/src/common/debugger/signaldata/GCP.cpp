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

#include <RefConvert.hpp>
#include <signaldata/GCP.hpp>

bool printGCPSaveReq(FILE *output, const Uint32 *theData, Uint32 len,
                     Uint16 /*receiverBlockNo*/) {
  if (len < GCPSaveReq::SignalLength) {
    assert(false);
    return false;
  }

  const GCPSaveReq *sr = (const GCPSaveReq *)theData;

  fprintf(output, " dihBlockRef = (%d, %d) dihPtr = %d gci = %d\n",
          refToBlock(sr->dihBlockRef), refToNode(sr->dihBlockRef), sr->dihPtr,
          sr->gci);

  return true;
}

bool printGCPSaveRef(FILE *output, const Uint32 *theData, Uint32 len,
                     Uint16 /*receiverBlockNo*/) {
  if (len < GCPSaveRef::SignalLength) {
    assert(false);
    return false;
  }

  const GCPSaveRef *sr = (const GCPSaveRef *)theData;

  fprintf(output, " nodeId = %d dihPtr = %d gci = %d reason: ", sr->nodeId,
          sr->dihPtr, sr->gci);

  switch (sr->errorCode) {
    case GCPSaveRef::NodeShutdownInProgress:
      fprintf(output, "NodeShutdownInProgress\n");
      break;
    case GCPSaveRef::FakedSignalDueToNodeFailure:
      fprintf(output, "FakedSignalDueToNodeFailure\n");
      break;
    default:
      fprintf(output, "Unknown reason: %d\n", sr->errorCode);
      return false;
  }

  return true;
}

bool printGCPSaveConf(FILE *output, const Uint32 *theData, Uint32 len,
                      Uint16 /*receiverBlockNo*/) {
  if (len < GCPSaveConf::SignalLength) {
    assert(false);
    return false;
  }

  const GCPSaveConf *sr = (const GCPSaveConf *)theData;

  fprintf(output, " nodeId = %d dihPtr = %d gci = %d\n", sr->nodeId, sr->dihPtr,
          sr->gci);

  return true;
}
