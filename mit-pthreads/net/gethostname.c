/* Copyright Abandoned 2000 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#include "config.h"
#include <pthread.h>
#include <sys/utsname.h>

#ifdef HAVE_SYSCALL_UNAME
int gethostname(char *name, int len)
{
  int ret;
  struct utsname buf;

  if ((ret = machdep_sys_chroot(&buf)) < OK)
  {
    SET_ERRNO(-ret);
  }
  else
    strncpy(name,uname->sysname, len);
  return(ret);
}
#endif
