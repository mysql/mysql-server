/* Copyright (C) 2003-2006 MySQL AB

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

#include <my_global.h>
#include <my_dir.h>
#include <my_sys.h>

#include <string.h>

#ifndef __WIN__
#include <pwd.h>
#include <grp.h>
#endif

#include "angel.h"
#include "log.h"
#include "manager.h"
#include "options.h"
#include "user_management_commands.h"

#ifdef __WIN__
#include "IMService.h"
#endif


/*
  Instance Manager consists of two processes: the angel process (IM-angel),
  and the manager process (IM-main). Responsibilities of IM-angel is to
  monitor IM-main, and restart it in case of failure/shutdown. IM-angel is
  started only if startup option '--run-as-service' is provided.

  IM-main consists of several subsystems (thread sets):

    - the signal handling thread

      The signal thread handles user signals and propagates them to the
      other threads. All other threads are accounted in the signal handler
      thread Thread Registry.

    - the listener

      The listener listens to all sockets. There is a listening socket for
      each subsystem (TCP/IP, UNIX socket).

    - mysql subsystem

      Instance Manager acts like an ordinary MySQL Server, but with very
      restricted command set. Each MySQL client connection is handled in a
      separate thread. All MySQL client connections threads constitute
      mysql subsystem.
*/

static int main_impl(int argc, char *argv[]);

#ifndef __WIN__
static struct passwd *check_user();
static bool switch_user();
#endif


/************************************************************************/
/**
  The entry point.
*************************************************************************/

int main(int argc, char *argv[])
{
  int return_value;

  puts("\n"
       "WARNING: This program is deprecated and will be removed in 6.0.\n");

  /* Initialize. */

  MY_INIT(argv[0]);
  log_init();
  umask(0117);
  srand((uint) time(0));

  /* Main function. */

  log_info("IM: started.");

  return_value= main_impl(argc, argv);

  log_info("IM: finished.");

  /* Cleanup. */

  Options::cleanup();
  my_end(0);

  return return_value;
}


/************************************************************************/
/**
  Instance Manager main functionality.
*************************************************************************/

int main_impl(int argc, char *argv[])
{
  int rc;

  if ((rc= Options::load(argc, argv)))
    return rc;

  if (Options::User_management::cmd)
    return Options::User_management::cmd->execute();

#ifndef __WIN__

  if (switch_user())
    return 1;

  return Options::Daemon::run_as_service ?
         Angel::main() :
         Manager::main();

#else

  return Options::Service::stand_alone ?
         Manager::main() :
         IMService::main();

#endif
}

/**************************************************************************
 OS-specific functions implementation.
**************************************************************************/

#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)

/************************************************************************/
/**
  Change to run as another user if started with --user.
*************************************************************************/

static struct passwd *check_user()
{
  const char *user= Options::Daemon::user;
  struct passwd *user_info;
  uid_t user_id= geteuid();

  /* Don't bother if we aren't superuser */
  if (user_id)
  {
    if (user)
    {
      /* Don't give a warning, if real user is same as given with --user */
      user_info= getpwnam(user);
      if ((!user_info || user_id != user_info->pw_uid))
        log_info("One can only use the --user switch if running as root\n");
    }
    return NULL;
  }
  if (!user)
  {
    log_info("You are running mysqlmanager as root! This might introduce security problems. It is safer to use --user option istead.\n");
    return NULL;
  }
  if (!strcmp(user, "root"))
    return NULL;                 /* Avoid problem with dynamic libraries */
 if (!(user_info= getpwnam(user)))
  {
    /* Allow a numeric uid to be used */
    const char *pos;
    for (pos= user; my_isdigit(default_charset_info, *pos); pos++)
    {}
    if (*pos)                                   /* Not numeric id */
      goto err;
    if (!(user_info= getpwuid(atoi(user))))
      goto err;
    else
      return user_info;
  }
  else
    return user_info;

err:
  log_error("Can not start under user '%s'.",
            (const char *) user);
  return NULL;
}


/************************************************************************/
/**
  Switch user.
*************************************************************************/

static bool switch_user()
{
  struct passwd *user_info= check_user();

  if (!user_info)
    return FALSE;

#ifdef HAVE_INITGROUPS
  initgroups(Options::Daemon::user, user_info->pw_gid);
#endif

  if (setgid(user_info->pw_gid) == -1)
  {
    log_error("setgid() failed");
    return TRUE;
  }

  if (setuid(user_info->pw_uid) == -1)
  {
    log_error("setuid() failed");
    return TRUE;
  }

  return FALSE;
}

#endif
