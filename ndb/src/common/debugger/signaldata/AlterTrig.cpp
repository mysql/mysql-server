/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <signaldata/AlterTrig.hpp>

bool printALTER_TRIG_REQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
  const AlterTrigReq * const sig = (AlterTrigReq *) theData;

  fprintf(output, "User: %u, ", sig->getUserRef());
  fprintf(output, "Trigger id: %u, ", sig->getTriggerId());
  fprintf(output, "\n");  

  return false;
}

bool printALTER_TRIG_CONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
  const AlterTrigConf * const sig = (AlterTrigConf *) theData;

  fprintf(output, "User: %u, ", sig->getUserRef());
  fprintf(output, "Trigger id: %u, ", sig->getTriggerId());
  fprintf(output, "\n");  

  return false;
}

bool printALTER_TRIG_REF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
  const AlterTrigRef * const sig = (AlterTrigRef *) theData;

  fprintf(output, "User: %u, ", sig->getUserRef());
  fprintf(output, "Trigger id: %u, ", sig->getTriggerId());
  fprintf(output, "Error code: %u, ", sig->getErrorCode());
  fprintf(output, "\n");  
  
  return false;
}
