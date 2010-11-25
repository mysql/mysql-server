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
  WL#3072 Maria recovery
  First version written by Guilhem Bichot on 2006-04-27.
*/

/* This is the interface of this module. */

/* Performs recovery of the engine at start */

C_MODE_START
enum maria_apply_log_way
{ MARIA_LOG_APPLY, MARIA_LOG_DISPLAY_HEADER, MARIA_LOG_CHECK };
int maria_recovery_from_log(void);
int maria_apply_log(LSN lsn, LSN lsn_end, enum maria_apply_log_way apply,
                    FILE *trace_file,
                    my_bool execute_undo_phase, my_bool skip_DDLs,
                    my_bool take_checkpoints, uint *warnings_count);
C_MODE_END
