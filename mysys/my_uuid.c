/* Copyright (C) 2007 MySQL AB, Sergei Golubchik & Michael Widenius

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

/*
  implements Universal Unique Identifiers (UUIDs), as in
    DCE 1.1: Remote Procedure Call,
    Open Group Technical Standard Document Number C706, October 1997,
    (supersedes C309 DCE: Remote Procedure Call 8/1994,
    which was basis for ISO/IEC 11578:1996 specification)

  A UUID has the following structure:

  Field                     NDR Data Type  Octet #          Note
 time_low                   unsigned long    0-3     The low field of the
                                                     timestamp.
 time_mid                   unsigned short   4-5     The middle field of
                                                     the timestamp.
 time_hi_and_version        unsigned short   6-7     The high field of the
                                                     timestamp multiplexed
                                                     with the version number.
 clock_seq_hi_and_reserved  unsigned small   8       The high field of the
                                                     clock sequence multi-
                                                     plexed with the variant.
 clock_seq_low              unsigned small   9       The low field of the
                                                     clock sequence.
 node                       character        10-15   The spatially unique node
                                                     identifier.
*/

#include "mysys_priv.h"
#include <m_string.h>
#include <myisampack.h> /* mi_int2store, mi_int4store */

static my_bool my_uuid_inited= 0;
static struct my_rnd_struct uuid_rand;
static uint nanoseq;
static ulonglong uuid_time= 0;
static uchar uuid_suffix[2+6]; /* clock_seq and node */

#ifdef THREAD
pthread_mutex_t LOCK_uuid_generator;
#endif

/*
  Number of 100-nanosecond intervals between
  1582-10-15 00:00:00.00 and 1970-01-01 00:00:00.00
*/

#define UUID_TIME_OFFSET ((ulonglong) 141427 * 24 * 60 * 60 * \
                          1000 * 1000 * 10)
#define UUID_VERSION      0x1000
#define UUID_VARIANT      0x8000


/* Helper function */

static void set_clock_seq()
{
  uint16 clock_seq= ((uint)(my_rnd(&uuid_rand)*16383)) | UUID_VARIANT;
  mi_int2store(uuid_suffix, clock_seq);
}


/**
  Init structures needed for my_uuid

  @func my_uuid_init()
  @param seed1		Seed for random generator
  @param seed2		Seed for random generator

  @note
    Seed1 & seed2 should NOT depend on clock. This is to be able to
    generate a random mac address according to UUID specs.
*/

void my_uuid_init(ulong seed1, ulong seed2)
{
  uchar *mac= uuid_suffix+2;
  ulonglong now;

  if (my_uuid_inited)
    return;
  my_uuid_inited= 1;
  now= my_getsystime();
  nanoseq= 0;

  if (my_gethwaddr(mac))
  {
    uint i;
    /*
      Generating random "hardware addr"

      Specs explicitly specify that node identifier should NOT
      correlate with a clock_seq value, so we use a separate
      randominit() here.
    */
    /* purecov: begin inspected */
    my_rnd_init(&uuid_rand, (ulong) (seed2+ now/2), (ulong) (now+rand()));
    for (i=0; i < array_elements(uuid_suffix) -2 ; i++)
      mac[i]= (uchar)(my_rnd(&uuid_rand)*255);
    /* purecov: end */
  }
  my_rnd_init(&uuid_rand, (ulong) (seed1 + now), (ulong) (now/2+ getpid()));
  set_clock_seq();
  pthread_mutex_init(&LOCK_uuid_generator, MY_MUTEX_INIT_FAST);
}


/**
   Create a global unique identifier (uuid)

   @func  my_uuid()
   @param to   Store uuid here. Must be of size MY_uuid_SIZE (16)
*/

void my_uuid(uchar *to)
{
  ulonglong tv;
  uint32 time_low;
  uint16 time_mid, time_hi_and_version;

  DBUG_ASSERT(my_uuid_inited);

  pthread_mutex_lock(&LOCK_uuid_generator);
  tv= my_getsystime() + UUID_TIME_OFFSET + nanoseq;

  if (likely(tv > uuid_time))
  {
    /*
      Current time is ahead of last timestamp, as it should be.
      If we "borrowed time", give it back, just as long as we
      stay ahead of the previous timestamp.
    */
    if (nanoseq)
    {
      ulong delta;
      DBUG_ASSERT((tv > uuid_time) && (nanoseq > 0));
      /*
        -1 so we won't make tv= uuid_time for nanoseq >= (tv - uuid_time)
      */
      delta= min(nanoseq, (ulong)(tv - uuid_time -1));
      tv-= delta;
      nanoseq-= delta;
    }
  }
  else
  {
    if (unlikely(tv == uuid_time))
    {
      /*
        For low-res system clocks. If several requests for UUIDs
        end up on the same tick, we add a nano-second to make them
        different.
        ( current_timestamp + nanoseq * calls_in_this_period )
        may end up > next_timestamp; this is OK. Nonetheless, we'll
        try to unwind nanoseq when we get a chance to.
        If nanoseq overflows, we'll start over with a new numberspace
        (so the if() below is needed so we can avoid the ++tv and thus
        match the follow-up if() if nanoseq overflows!).
      */
      if (likely(++nanoseq))
        ++tv;
    }

    if (unlikely(tv <= uuid_time))
    {
      /*
        If the admin changes the system clock (or due to Daylight
        Saving Time), the system clock may be turned *back* so we
        go through a period once more for which we already gave out
        UUIDs.  To avoid duplicate UUIDs despite potentially identical
        times, we make a new random component.
        We also come here if the nanoseq "borrowing" overflows.
        In either case, we throw away any nanoseq borrowing since it's
        irrelevant in the new numberspace.
      */
      set_clock_seq();
      tv= my_getsystime() + UUID_TIME_OFFSET;
      nanoseq= 0;
      DBUG_PRINT("uuid",("making new numberspace"));
    }
  }

  uuid_time=tv;
  pthread_mutex_unlock(&LOCK_uuid_generator);

  time_low=            (uint32) (tv & 0xFFFFFFFF);
  time_mid=            (uint16) ((tv >> 32) & 0xFFFF);
  time_hi_and_version= (uint16) ((tv >> 48) | UUID_VERSION);

  /*
    Note, that the standard does NOT specify byte ordering in
    multi-byte fields. it's implementation defined (but must be
    the same for all fields).
    We use big-endian, so we can use memcmp() to compare UUIDs
    and for straightforward UUID to string conversion.
  */
  mi_int4store(to, time_low);
  mi_int2store(to+4, time_mid);
  mi_int2store(to+6, time_hi_and_version);
  bmove(to+8, uuid_suffix, sizeof(uuid_suffix));
}


/**
   Convert uuid to string representation

   @func  my_uuid2str()
   @param guid uuid
   @param s    Output buffer.Must be at least MY_UUID_STRING_LENGTH+1 large.
*/
void my_uuid2str(const uchar *guid, char *s)
{
  int i;
  for (i=0; i < MY_UUID_SIZE; i++)
  {
    *s++= _dig_vec_lower[guid[i] >>4];
    *s++= _dig_vec_lower[guid[i] & 15];
    if(i == 3 || i == 5 || i == 7 || i == 9)
      *s++= '-';
  }
}

void my_uuid_end()
{
  if (my_uuid_inited)
  {
    my_uuid_inited= 0;
    pthread_mutex_destroy(&LOCK_uuid_generator);
  }
}
