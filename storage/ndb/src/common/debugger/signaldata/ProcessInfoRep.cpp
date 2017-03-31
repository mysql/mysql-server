/*
   Copyright (C) 2016, 2017 Oracle and/or its affiliates. All rights reserved.
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

#include <signaldata/ProcessInfoRep.hpp>

bool
printPROCESSINFO_REP(FILE * out, const Uint32 * data, Uint32 len, Uint16 bno) {
  ProcessInfoRep * sig = (ProcessInfoRep *) data;

  fprintf(out, " process_name: %s", (char *) sig->process_name);
  fprintf(out, " process_id: %d  angel_process_id: %d  application_port: %d",
          sig->process_id, sig->angel_process_id, sig->application_port);

  return true;
}
