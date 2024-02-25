/*
   Copyright (c) 2009, 2023, Oracle and/or its affiliates.
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

#include <signaldata/LocalRouteOrd.hpp>
#include <DebuggerNames.hpp>
#include <RefConvert.hpp>

bool printLOCAL_ROUTE_ORD(FILE* output,
                          const Uint32* theData,
                          Uint32 len,
                          Uint16 /*rbn*/)
{
  const LocalRouteOrd * sig = (const LocalRouteOrd*)theData;
  Uint32 pathcnt = sig->cnt >> 16;
  Uint32 dstcnt = sig->cnt & 0xFFFF;

  fprintf(output, " pathcnt: %u dstcnt: %u\n", pathcnt, dstcnt);
  fprintf(output, " gsn: %u(%s) prio: %u\n",
          sig->gsn, getSignalName(sig->gsn), sig->prio);

  const Uint32 * ptr = sig->path;
  fprintf(output, " path:");
  for (Uint32 i = 0; i<pathcnt; i++)
  {
    fprintf(output, " [ hop: 0x%x(%s) prio: %u ]",
            ptr[0], getBlockName(refToMain(ptr[0])), ptr[1]);
    ptr += 2;
  }

  fprintf(output, "\n dst:");
  for (Uint32 i = 0; i<dstcnt; i++)
  {
    fprintf(output, " [ 0x%x(%s) ]",
            ptr[0], getBlockName(refToMain(ptr[0])));
  }
  fprintf(output, "\n");

  if (ptr < (theData + len))
  {
    fprintf(output, " data:");
    while (ptr < (theData + len))
    {
      fprintf(output, " %.8x", * ptr++);
    }
    fprintf(output, "\n");
  }
  return true;
}
