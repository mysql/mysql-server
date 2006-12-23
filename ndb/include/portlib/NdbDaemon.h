/* Copyright (C) 2003 MySQL AB

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

#ifndef NDB_DAEMON_H
#define NDB_DAEMON_H

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Become a daemon.
 * lockfile     the "pid file" or other resource to lock exclusively
 * logfile      daemon output is directed here (input is set to /dev/null)
 *              if NULL, output redirection is not done
 * flags        none currently
 * returns      0 on success, on error -1
 */
extern int
NdbDaemon_Make(const char* lockfile, const char* logfile, unsigned flags);

/*
 * Test if the daemon is running (file is locked).
 * lockfile     the "pid file"
 * flags        none currently
 * return       0 no, 1 yes, -1 
 */
extern int
NdbDaemon_Test(const char* lockfile, unsigned flags);

/*
 * Kill the daemon.
 * lockfile     the "pid file"
 * flags        none currently
 * return       0 killed, 1 not running, -1 other error
 */
extern int
NdbDaemon_Kill(const char* lockfile, unsigned flags);

/*
 * Pid from last call, either forked off or found in lock file.
 */
extern long NdbDaemon_DaemonPid;

/*
 * Error code from last failed call.
 */
extern int NdbDaemon_ErrorCode;

/*
 * Error text from last failed call.
 */
extern char NdbDaemon_ErrorText[];

#ifdef  __cplusplus
}
#endif

#endif
