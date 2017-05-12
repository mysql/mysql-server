#ifndef DEBUG_SYNC_INCLUDED
#define DEBUG_SYNC_INCLUDED

/* Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file

  Declarations for the Debug Sync Facility. See debug_sync.cc for details.
*/

#include <stddef.h>
#include <sys/types.h>

#include "m_string.h"
#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_sharedlib.h"

class THD;

#if defined(ENABLED_DEBUG_SYNC)

/* Macro to be put in the code at synchronization points. */
#define DEBUG_SYNC(_thd_, _sync_point_name_)                            \
          do { if (unlikely(opt_debug_sync_timeout))                    \
               debug_sync(_thd_, STRING_WITH_LEN(_sync_point_name_));   \
             } while (0)

/* Command line option --debug-sync-timeout. See mysqld.cc. */
extern MYSQL_PLUGIN_IMPORT uint opt_debug_sync_timeout;

/* Default WAIT_FOR timeout if command line option is given without argument. */
#define DEBUG_SYNC_DEFAULT_WAIT_TIMEOUT 300

/* Debug Sync prototypes. See debug_sync.cc. */
extern int  debug_sync_init(void);
extern void debug_sync_end(void);
extern void debug_sync_init_thread(THD *thd);
extern void debug_sync_claim_memory_ownership(THD *thd);
extern void debug_sync_end_thread(THD *thd);
extern void debug_sync(THD *thd, const char *sync_point_name, size_t name_len);
extern bool debug_sync_set_action(THD *thd, const char *action_str, size_t len);
extern bool debug_sync_update(THD *thd, char *val_str);
extern uchar *debug_sync_value_ptr(THD *thd);

#else /* defined(ENABLED_DEBUG_SYNC) */

#define DEBUG_SYNC(_thd_, _sync_point_name_)    /* disabled DEBUG_SYNC */

#endif /* defined(ENABLED_DEBUG_SYNC) */

#endif /* DEBUG_SYNC_INCLUDED */
