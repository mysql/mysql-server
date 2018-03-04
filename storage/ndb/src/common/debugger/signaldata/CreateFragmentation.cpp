/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <signaldata/CreateFragmentation.hpp>

bool
printCREATE_FRAGMENTATION_REQ(FILE * output, const Uint32 * theData, 
			      Uint32 len, Uint16 receiverBlockNo) {
  const CreateFragmentationReq * const sig = (CreateFragmentationReq *)theData;
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " fragmentationType: %x\n", sig->fragmentationType);
  fprintf(output, " noOfFragments: %x\n", sig->noOfFragments);
  fprintf(output, " partitionBalance: %d\n", sig->partitionBalance);
  if (sig->primaryTableId == RNIL)
    fprintf(output, " primaryTableId: none\n");
  else
    fprintf(output, " primaryTableId: %x\n", sig->primaryTableId);
  fprintf(output, " partitionCount: %x\n", sig->partitionCount);
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

