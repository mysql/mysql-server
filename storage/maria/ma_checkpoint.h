/* Copyright (C) 2006,2007 MySQL AB

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
  WL#3071 Maria checkpoint
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

/* This is the interface of this module. */

typedef enum enum_ma_checkpoint_level {
  CHECKPOINT_NONE= 0,
  /* just write dirty_pages, transactions table and sync files */
  CHECKPOINT_INDIRECT,
  /* also flush all dirty pages which were already dirty at prev checkpoint */
  CHECKPOINT_MEDIUM,
  /* also flush all dirty pages */
  CHECKPOINT_FULL
} CHECKPOINT_LEVEL;

C_MODE_START
int ma_checkpoint_init(ulong interval);
void ma_checkpoint_end(void);
int ma_checkpoint_execute(CHECKPOINT_LEVEL level, my_bool no_wait);
C_MODE_END

/**
   @brief reads some LSNs with special trickery

   If a 64-bit variable transitions between both halves being zero to both
   halves being non-zero, and back, this function can be used to do a read of
   it (without mutex, without atomic load) which always produces a correct
   (though maybe slightly old) value (even on 32-bit CPUs). The value is at
   least as new as the latest mutex unlock done by the calling thread.
   The assumption is that the system sets both 4-byte halves either at the
   same time, or one after the other (in any order), but NOT some bytes of the
   first half then some bytes of the second half then the rest of bytes of the
   first half. With this assumption, the function can detect when it is
   seeing an inconsistent value.

   @param LSN              pointer to the LSN variable to read

   @return LSN part (most significant byte always 0)
*/
#if ( SIZEOF_CHARP >= 8 )
/* 64-bit CPU, 64-bit reads are atomic */
#define lsn_read_non_atomic LSN_WITH_FLAGS_TO_LSN
#else
static inline LSN lsn_read_non_atomic_32(const volatile LSN *x)
{
  /*
    32-bit CPU, 64-bit reads may give a mixed of old half and new half (old
    low bits and new high bits, or the contrary).
  */
  for (;;) /* loop until no atomicity problems */
  {
    /*
      Remove most significant byte in case this is a LSN_WITH_FLAGS object.
      Those flags in TRN::first_undo_lsn break the condition on transitions so
      they must be removed below.
    */
    LSN y= LSN_WITH_FLAGS_TO_LSN(*x);
    if (likely((y == LSN_IMPOSSIBLE) || LSN_VALID(y)))
      return y;
  }
}
#define lsn_read_non_atomic(x) lsn_read_non_atomic_32(&x)
#endif

/**
   prints a message from a task not connected to any user (checkpoint
   and recovery for example).

   @param  level           0 if error, ME_JUST_WARNING if warning,
                           ME_JUST_INFO if info
   @param  sentence        text to write
*/
#define ma_message_no_user(level, sentence)                               \
  my_printf_error(HA_ERR_GENERIC, "Aria engine: %s", MYF(level), sentence)
