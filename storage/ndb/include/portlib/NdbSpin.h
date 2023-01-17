/*
   Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_SPIN_H
#define NDB_SPIN_H

#include <ndb_types.h>

/**
 * NdbSpin_Init() is called to calibrate the NdbSpin() function.
 * The aim of this function is to estimate the number of spin
 * cycles to use for NdbSpin() to take the number of microseconds
 * as proposed in the init call.
 *
 * It is possible to later call NdbSpin_Change to change the spinning
 * time in the NdbSpin-function.
 */
void NdbSpin_Init();
void NdbSpin_Change(Uint64 spin_nanos);
void NdbSpin();
bool NdbSpin_is_supported();
Uint64 NdbSpin_get_num_spin_loops();
Uint64 NdbSpin_get_current_spin_nanos();
#endif
