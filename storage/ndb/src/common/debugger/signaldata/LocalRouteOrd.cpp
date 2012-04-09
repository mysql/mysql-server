/*
   Copyright 2009 Sun Microsystems, Inc.
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

#include <signaldata/LocalRouteOrd.hpp>
#include <DebuggerNames.hpp>
#include <RefConvert.hpp>

bool
printLOCAL_ROUTE_ORD(FILE* output,
                     const Uint32* theData, Uint32 len,
                     Uint16 rbn)
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
