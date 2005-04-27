/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include "NdbDaemon.h"

#define NdbDaemon_ErrorSize 500
long NdbDaemon_DaemonPid = 0;
int NdbDaemon_ErrorCode = 0;
char NdbDaemon_ErrorText[NdbDaemon_ErrorSize] = "";

int
NdbDaemon_Make(const char* lockfile, const char* logfile, unsigned flags)
{
  int lockfd = -1, logfd = -1, n;
  char buf[64];

  (void)flags; /* remove warning for unused parameter */

  /* Check that we have write access to lock file */
  assert(lockfile != NULL);
  lockfd = open(lockfile, O_CREAT|O_RDWR, 0644);
  if (lockfd == -1) {
    NdbDaemon_ErrorCode = errno;
    snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	"%s: open for write failed: %s", lockfile, strerror(errno));
    return -1;
  }
  /* Read any old pid from lock file */
  buf[0] = 0;
  n = read(lockfd, buf, sizeof(buf));
  if (n < 0) {
    NdbDaemon_ErrorCode = errno;
    snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	"%s: read failed: %s", lockfile, strerror(errno));
    return -1;
  }
  NdbDaemon_DaemonPid = atol(buf);
  if (lseek(lockfd, 0, SEEK_SET) == -1) {
    NdbDaemon_ErrorCode = errno;
    snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	"%s: lseek failed: %s", lockfile, strerror(errno));
    return -1;
  }
#ifdef F_TLOCK
  /* Test for lock before becoming daemon */
  if (lockf(lockfd, F_TLOCK, 0) == -1) 
  {
    if (errno == EACCES || errno == EAGAIN) {   /* results may vary */
      snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	       "%s: already locked by pid=%ld", lockfile, NdbDaemon_DaemonPid);
      return -1;
    }
    NdbDaemon_ErrorCode = errno;
    snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
        "%s: lock test failed: %s", lockfile, strerror(errno));
    return -1;
  }
#endif
  /* Test open log file before becoming daemon */
  if (logfile != NULL) {
    logfd = open(logfile, O_CREAT|O_WRONLY|O_APPEND, 0644);
    if (logfd == -1) {
      NdbDaemon_ErrorCode = errno;
      snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	  "%s: open for write failed: %s", logfile, strerror(errno));
      return -1;
    }
  }
#ifdef F_TLOCK
  if (lockf(lockfd, F_ULOCK, 0) == -1) 
  {
    snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	     "%s: fail to unlock", lockfile);
    return -1;
  }
#endif
  
  /* Fork */
  n = fork();
  if (n == -1) {
    NdbDaemon_ErrorCode = errno;
    snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	"fork failed: %s", strerror(errno));
    return -1;
  }
  /* Exit if we are the parent */
  if (n != 0) {
    exit(0);
  }
  /* Running in child process */
  NdbDaemon_DaemonPid = getpid();
  /* Lock the lock file (likely to succeed due to test above) */
  if (lockf(lockfd, F_LOCK, 0) == -1) {
    NdbDaemon_ErrorCode = errno;
    snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	"%s: lock failed: %s", lockfile, strerror(errno));
    return -1;
  }
  /* Become process group leader */
  if (setsid() == -1) {
    NdbDaemon_ErrorCode = errno;
    snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	"setsid failed: %s", strerror(errno));
    return -1;
  }
  /* Write pid to lock file */
  if (ftruncate(lockfd, 0) == -1) {
    NdbDaemon_ErrorCode = errno;
    snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	"%s: ftruncate failed: %s", lockfile, strerror(errno));
    return -1;
  }
  sprintf(buf, "%ld\n", NdbDaemon_DaemonPid);
  n = strlen(buf);
  if (write(lockfd, buf, n) != n) {
    NdbDaemon_ErrorCode = errno;
    snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	"%s: write failed: %s", lockfile, strerror(errno));
    return -1;
  }
  /* Do input/output redirections (assume fd 0,1,2 not in use) */
  close(0);
  open("/dev/null", O_RDONLY);
  if (logfile != 0) {
    dup2(logfd, 1);
    dup2(logfd, 2);
    close(logfd);
  }
  /* Success */
  return 0;
}

#if 0
int
NdbDaemon_Make(const char* lockfile, const char* logfile, unsigned flags)
{
  /* Fail */
  snprintf(NdbDaemon_ErrorText, NdbDaemon_ErrorSize,
	   "Daemon mode not implemented");
  return -1;
}
#endif

#ifdef NDB_DAEMON_TEST

int
main()
{
  if (NdbDaemon_Make("test.pid", "test.log", 0) == -1) {
    fprintf(stderr, "NdbDaemon_Make: %s\n", NdbDaemon_ErrorText);
    return 1;
  }
  sleep(10);
  return 0;
}

#endif
