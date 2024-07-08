/*
  Copyright (c) 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/psi/mysql_file.h"

#ifdef HAVE_PSI_INTERFACE
PSI_file_key key_file_misc;
#endif

#ifdef NDEBUG

struct CODE_STATE;

// provide the dbug symbols as no-ops if they are not added to the mysys lib to
// make exporting symbols easier.
void _db_enter_(const char *_func_ [[maybe_unused]],
                int func_len [[maybe_unused]],
                const char *_file_ [[maybe_unused]],
                uint _line_ [[maybe_unused]],
                struct _db_stack_frame_ *_stack_frame_ [[maybe_unused]]) {}

void _db_return_(uint _line_ [[maybe_unused]],
                 struct _db_stack_frame_ *_stack_frame_ [[maybe_unused]]) {}

void _db_pargs_(uint _line_ [[maybe_unused]],
                const char *keyword [[maybe_unused]]) {}

int _db_enabled_() { return 0; }

void _db_doprnt_(const char *format [[maybe_unused]], ...) {}
void _db_dump_(uint _line_ [[maybe_unused]],
               const char *keyword [[maybe_unused]],
               const unsigned char *memory [[maybe_unused]],
               size_t length [[maybe_unused]]) {}

int _db_keyword_(CODE_STATE *cs [[maybe_unused]],
                 const char *keyword [[maybe_unused]],
                 int strict [[maybe_unused]]) {
  return 0;
}
void _db_set_(const char *control [[maybe_unused]]) {}
#endif
