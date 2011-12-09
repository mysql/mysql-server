/*
   Copyright (C) 2003-2006 MySQL AB
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

#include <signaldata/CreateFragmentation.hpp>

bool
printCREATE_FRAGMENTATION_REQ(FILE * output, const Uint32 * theData, 
			      Uint32 len, Uint16 receiverBlockNo) {
  const CreateFragmentationReq * const sig = (CreateFragmentationReq *)theData;
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " fragmentationType: %x\n", sig->fragmentationType);
  fprintf(output, " noOfFragments: %x\n", sig->noOfFragments);
  if (sig->primaryTableId == RNIL)
    fprintf(output, " primaryTableId: none\n");
  else
    fprintf(output, " primaryTableId: %x\n", sig->primaryTableId);
  return true;
}

bool
printCREATE_FRAGMENTATION_REF(FILE * output, const Uint32 * theData, 
			      Uint32 len, Uint16 receiverBlockNo) {
  const CreateFragmentationRef * const sig = (CreateFragmentationRef *)theData;
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return true;
}

bool
printCREATE_FRAGMENTATION_CONF(FILE * output, const Uint32 * theData, 
			       Uint32 len, Uint16 receiverBlockNo) {
  const CreateFragmentationConf * const sig = 
    (CreateFragmentationConf *)theData;
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " noOfReplicas: %x\n", sig->noOfReplicas);
  fprintf(output, " noOfFragments: %x\n", sig->noOfFragments);
  return true;
}

