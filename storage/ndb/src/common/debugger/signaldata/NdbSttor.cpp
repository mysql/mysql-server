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

#include <signaldata/NdbSttor.hpp>

bool printNDB_STTOR(FILE *output,
                    const Uint32 *theData,
                    Uint32 len,
                    Uint16 /*receiverBlockNo*/)
{
  const NdbSttor *const sig = (const NdbSttor *)theData;
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " nodeId: %x\n", sig->nodeId);
  fprintf(output, " internalStartPhase: %x\n", sig->internalStartPhase);
  fprintf(output, " typeOfStart: %x\n", sig->typeOfStart);
  fprintf(output, " masterNodeId: %x\n", sig->masterNodeId);

  int left = len - NdbSttor::SignalLength;
  if(left > 0){
    fprintf(output, " config: ");
    for(int i = 0; i<left; i++){
      fprintf(output, "%x ", sig->config[i]);
      if(((i + 1) % 7) == 0 && (i+1) < left){
	fprintf(output, "\n config: ");
      }
    }
    fprintf(output, "\n");
  }
  return true;
}

bool printNDB_STTORRY(FILE *output,
                      const Uint32 *theData,
                      Uint32 len,
                      Uint16 /*receiverBlockNo*/)
{
  if (len < NdbSttorry::SignalLength)
  {
    assert(false);
    return false;
  }

  const NdbSttorry *const sig = (const NdbSttorry *)theData;
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  return true;
}
