/*
   Copyright (c) 2018, 2021, Oracle and/or its affiliates.

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

#include <ndb_global.h>
#include "../../kernel/vm/mt-asm.h"
#include <NdbSpin.h>
#include <NdbTick.h>

static Uint64 glob_num_spin_loops = 10;
static Uint64 glob_current_spin_nanos = 800;

Uint64 NdbSpin_get_num_spin_loops()
{
  return glob_num_spin_loops;
}

Uint64 NdbSpin_get_current_spin_nanos()
{
  return glob_current_spin_nanos;
}

void NdbSpin_Init()
{
  Uint64 loops = 0;
#ifdef NDB_HAVE_CPU_PAUSE
  Uint64 spin_nanos = glob_current_spin_nanos;
  Uint64 min_nanos_per_call = 0xFFFFFFFF;
  for (Uint32 i = 0; i < 5; i++)
  {
    Uint32 loop_count = 100;
    NDB_TICKS start = NdbTick_getCurrentTicks();
    for (Uint32 j = 0; j < loop_count; j++)
    {
      NdbSpin();
    }
    NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint64 nanos_passed = NdbTick_Elapsed(start, now).nanoSec();
    const Uint64 nanos_per_call = nanos_passed / loop_count;
    if (nanos_per_call < min_nanos_per_call)
    {
      min_nanos_per_call = nanos_per_call;
    }
  }
  loops = ((min_nanos_per_call - 1) + spin_nanos) / min_nanos_per_call;
#endif
  if (loops == 0)
  {
    loops = 1;
  }
  glob_num_spin_loops = loops;
}

void NdbSpin_Change(Uint64 spin_nanos)
{
  (void)spin_nanos;
#ifdef NDB_HAVE_CPU_PAUSE
  if (spin_nanos < 300)
  {
    spin_nanos = 300;
  }
  const Uint64 current_spin_nanos = glob_current_spin_nanos;
  const Uint64 current_loops = glob_num_spin_loops;
  Uint64 new_spin_loops = (spin_nanos * current_loops) /
                          current_spin_nanos;
  if (new_spin_loops == 0)
  {
    new_spin_loops = 1;
  }
  glob_current_spin_nanos = spin_nanos;
  glob_num_spin_loops = Uint32(new_spin_loops);
#endif
}

bool NdbSpin_is_supported()
{
#ifdef NDB_HAVE_CPU_PAUSE
  return true;
#else
  return false;
#endif
}

#ifdef NDB_HAVE_CPU_PAUSE
void NdbSpin()
{
  Uint64 loops = glob_num_spin_loops;
  for (Uint64 i = 0; i < loops; i++)
  {
    /**
     * The maximum of  a pause instructions should be around
     * 200 ns, so smaller spintime than this we will not
     * handle. This is on Skylake CPUs, older CPUs will spin
     * through those in around 25 ns or less instead. So loop
     * count is likely to be much higher on those CPUs.
     */
    cpu_pause();
  }
}
#else
void NdbSpin()
{
  /* Should not happen */
  abort();
}
#endif
