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

#include <ndb_global.h>
#include "portlib/mt-asm.h"
#include <NdbSpin.h>
#include <NdbTick.h>


/**
 * Initial guess, calibrated by NdbSpin_Init() and adjusted
 * with NdbSpin_Change() if needed.
 *
 * We want a single call to NdbSpin() to pause the thread on-cpu
 * for ~1000 nanos (1us) by making one or more calls to cpu_pause().
 * We need to calibrate how many calls results in 1000 nanos of
 * pausing on this system'
 *
 * Note that we need to NdbSpin_Init() in order for
 * NdbSpin_is_supported() to return true.
 */
static Uint64 glob_num_spin_loops = 10;
static Uint64 glob_current_spin_nanos = 1000;
static bool glob_spin_enabled = false;

bool NdbSpin_is_supported()
{
  return glob_spin_enabled;
}

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
  /**
   * Sample the cpu_pause() duration which is platform dependent
   * in the order of a few to tens of nanoseconds.
   *
   * Find how many times we need to pause in order to get a
   * a total pause duration of ~1000ns. (glob_current_spin_nanos.)
   * As each 'pause' is short, in the ns range, and resolution of
   * NdbTick is not guaranteed (platform dependent) we sample 1000
   * pauses.
   *
   * In theory we can still end up with a measured 'nanos_passed'
   * time of 0 nanos. In such cases 'glob_spin_enabled' will remain
   * 'false' and NdbSpin_is_supported() -> false.
   *
   * If it takes < 1 cpu_pause() instruction to delay for 1000 nanos
   * then we will delay for > 1000 nanos. (We are not aware of any
   * platforms where a pause takes longer than 10's of nanos.)
   *
   * We do 5 rounds of sampling and takes the highest number of
   * calculated loop spins in order to reduce the risks mentioned
   * above.
   */
  const Uint64 spin_nanos = glob_current_spin_nanos;
  for (Uint32 i = 0; i < 5; i++)
  {
    constexpr Uint32 loop_count = 1000;
    const NDB_TICKS start = NdbTick_getCurrentTicks();
    for (Uint32 j = 0; j < loop_count; j++)
    {
      cpu_pause();
    }
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint64 nanos_passed = NdbTick_Elapsed(start, now).nanoSec();
    if (nanos_passed > 0)
    {
      const Uint64 new_spin_loops = (loop_count*spin_nanos) / nanos_passed;
      if (new_spin_loops > loops)
      {
        loops = new_spin_loops;
        glob_spin_enabled = true;
      }
    }
  }
#endif
  if (loops == 0)
  {
    loops = 1;
  }
  glob_num_spin_loops = loops;
}

void NdbSpin_Change(Uint64 spin_nanos[[maybe_unused]])
{
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
  /**
   * If a 'pause' implementation is not available on the platform, we do
   * not want the CPU to do spin-waiting either. Let compiler enforce it
   * by not implementing the NdbSpin() at all in such cases.
   */
#endif
