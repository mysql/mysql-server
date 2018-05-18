/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/errors.cc
*/

#include "my_config.h"
#include "my_loglevel.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "my_dbug.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysys_err.h"

const char *globerrs[GLOBERRS] = {
    "Can't create/write to file '%s' (OS errno %d - %s)",
    "Error reading file '%s' (OS errno %d - %s)",
    "Error writing file '%s' (OS errno %d - %s)",
    "Error on close of '%s' (OS errno %d - %s)",
    "Out of memory (Needed %u bytes)",
    "Error on delete of '%s' (OS errno %d - %s)",
    "Error on rename of '%s' to '%s' (OS errno %d - %s)",
    "",
    "Unexpected EOF found when reading file '%s' (OS errno %d - %s)",
    "Can't lock file (OS errno %d - %s)",
    "Can't unlock file (OS errno %d - %s)",
    "Can't read dir of '%s' (OS errno %d - %s)",
    "Can't get stat of '%s' (OS errno %d - %s)",
    "Can't change size of file (OS errno %d - %s)",
    "Can't open stream from handle (OS errno %d - %s)",
    "Can't get working directory (OS errno %d - %s)",
    "Can't change dir to '%s' (OS errno %d - %s)",
    "Warning: '%s' had %d links",
    "Warning: %d files and %d streams is left open\n",
    "Disk is full writing '%s' (OS errno %d - %s). Waiting for someone to free "
    "space...",
    "Can't create directory '%s' (OS errno %d - %s)",
    "Character set '%s' is not a compiled character set and is not specified "
    "in the '%s' file",
    "Out of resources when opening file '%s' (OS errno %d - %s)",
    "Can't read value for symlink '%s' (Error %d - %s)",
    "Can't create symlink '%s' pointing at '%s' (Error %d - %s)",
    "Error on realpath() on '%s' (Error %d - %s)",
    "Can't sync file '%s' to disk (OS errno %d - %s)",
    "Collation '%s' is not a compiled collation and is not specified in the "
    "'%s' file",
    "File '%s' not found (OS errno %d - %s)",
    "File '%s' (fileno: %d) was not closed",
    "Cannot change ownership of the file '%s' (OS errno %d - %s)",
    "Cannot change permissions of the file '%s' (OS errno %d - %s)",
    "Cannot seek in file '%s' (OS errno %d - %s)",
    "Memory capacity exceeded (capacity %llu bytes)",
    "Disk is full writing '%s' (OS errno %d - %s). Waiting for someone to free "
    "space... Retry in %d secs. Message reprinted in %d secs.",
    "Failed to create timer (OS errno %d).",
    "Failed to delete timer (OS errno %d).",
    "Failed to create timer queue (OS errno %d).",
    "Failed to start timer notify thread.",
    "Failed to create event to interrupt timer notifier thread (OS errno %d).",
    "Failed to register timer event with queue (OS errno %d), exiting timer "
    "notifier thread.",
    "LoadLibrary(\"kernel32.dll\") failed: GetLastError returns %lu.",
    "%s.",
    "Failed to determine large page size.",
    "Error in my_thread_global_end(): %d thread(s) did not exit.",
    "Failed to create IO completion port (OS errno %d).",
    "Failed to open required defaults file: %s",
    "Fatal error in defaults handling. Program aborted!",
    "Wrong '!%s' directive in config file %s at line %d.",
    "Skipping '%s' directive as maximum include recursion level was"
    " reached in file %s at line %d.",
    "Wrong group definition in config file %s at line %d.",
    "Found option without preceding group in config file %s at line %d.",
    "%s should be readable/writable only by current user.",
    "World-writable config file '%s' is ignored.",
    "%s: Option '%s' was used, but is disabled.",
    "%s: Option '-%c' was used, but is disabled.",
    "Using a password on the command line interface can be insecure.",
    "Unknown suffix '%c' used for variable '%s' (value '%s').",
    "SSL error: %s from '%s'.",
    "SSL error: %s.",
    "%d  %s.",
    "Packets out of order (found %u, expected %u).",
    "Unknown option to protocol: %s.",
    "Failed to locate server public key '%s'.",
    "Public key is not in Privacy Enhanced Mail format: '%s'.",
    "%s.",
    "unknown variable '%s'.",
    "unknown option '--%s'.",
    "%s: unknown option '-%c'.",
    "%s: option '--%s' cannot take an argument.",
    "%s: option '--%s' requires an argument.",
    "%s: option '-%c' requires an argument.",
    "%s: ignoring option '--%s' due to invalid value '%s'.",
    "%s: Empty value for '%s' specified.",
    "%s: Maximum value of '%s' cannot be set.",
    "option '%s': boolean value '%s' was not recognized. Set to OFF.",
    "%s: Error while setting value '%s' to '%s'.",
    "Incorrect integer value: '%s'.",
    "Incorrect unsigned integer value: '%s'.",
    "option '%s': signed value %s adjusted to %s.",
    "option '%s': unsigned value %s adjusted to %s.",
    "option '%s': value %s adjusted to %s.",
    "option '%s': value %g adjusted to %g.",
    "Invalid decimal value for option '%s'.",
    "%s.",
    "Failed to reset before a primary ignorable character %s.",
    "Failed to reset before a territory ignorable character %s.",
    "Shift character out of range: %s.",
    "Reset character out of range: %s.",
    "Unknown LDML tag: '%.*s'."};

/*
 We cannot call my_error/my_printf_error here in this function.
  Those functions will set status variable in diagnostic area
  and there is no provision to reset them back.
  Here we are waiting for free space and will wait forever till
  space is created. So just giving warning in the error file
  should be enough.
*/
void wait_for_free_space(const char *filename, int errors) {
  size_t time_to_sleep = MY_WAIT_FOR_USER_TO_FIX_PANIC;

  if (!(errors % MY_WAIT_GIVE_USER_A_MESSAGE)) {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_message_local(
        ERROR_LEVEL, EE_DISK_FULL_WITH_RETRY_MSG, filename, my_errno(),
        my_strerror(errbuf, sizeof(errbuf), my_errno()),
        MY_WAIT_FOR_USER_TO_FIX_PANIC,
        MY_WAIT_GIVE_USER_A_MESSAGE * MY_WAIT_FOR_USER_TO_FIX_PANIC);
  }
  DBUG_EXECUTE_IF("simulate_no_free_space_error", { time_to_sleep = 1; });
  DBUG_EXECUTE_IF("force_wait_for_disk_space", { time_to_sleep = 1; });
  DBUG_EXECUTE_IF("simulate_io_thd_wait_for_disk_space",
                  { time_to_sleep = 1; });
  DBUG_EXECUTE_IF("simulate_random_io_thd_wait_for_disk_space",
                  { time_to_sleep = 1; });
  // Answer more promptly to a KILL signal
  do {
    (void)sleep(1);
  } while (--time_to_sleep > 0 && !is_killed_hook(NULL));
}

const char *get_global_errmsg(int nr) { return globerrs[nr - EE_ERROR_FIRST]; }
