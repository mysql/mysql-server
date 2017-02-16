#include <my_global.h>
#include <m_string.h>
#include <my_sys.h>
#include <my_pthread.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif

struct passwd *my_check_user(const char *user, myf MyFlags)
{
  struct passwd *user_info;
  uid_t user_id= geteuid();
  DBUG_ENTER("my_check_user");

  // Don't bother if we aren't superuser
  if (user_id)
  {
    if (user)
    {
      /* Don't give a warning, if real user is same as given with --user */
      user_info= getpwnam(user);
      if (!user_info || user_id != user_info->pw_uid)
      {
        my_errno= EPERM;
        if (MyFlags & MY_WME)
          my_printf_error(my_errno, "One can only use the --user switch if "
                         "running as root", MYF(ME_JUST_WARNING|ME_NOREFRESH));
      }
    }
    DBUG_RETURN(NULL);
  }
  if (!user)
  {
    if (MyFlags & MY_FAE)
    {
      my_errno= EINVAL;
      my_printf_error(my_errno, "Please consult the Knowledge Base to find "
                      "out how to run mysqld as root!", MYF(ME_NOREFRESH));
    }
    DBUG_RETURN(NULL);
  }
  if (!strcmp(user,"root"))
    DBUG_RETURN(NULL);

  if (!(user_info= getpwnam(user)))
  {
    // Allow a numeric uid to be used
    int err= 0;
    user_id= my_strtoll10(user, NULL, &err);
    if (err || !(user_info= getpwuid(user_id)))
    {
      my_errno= EINVAL;
      my_printf_error(my_errno, "Can't change to run as user '%s'.  Please "
                      "check that the user exists!", MYF(ME_NOREFRESH), user);
      DBUG_RETURN(NULL);
    }
  }
  DBUG_ASSERT(user_info);
  DBUG_RETURN(user_info);
}

int my_set_user(const char *user, struct passwd *user_info, myf MyFlags)
{
  DBUG_ENTER("my_set_user");

  DBUG_ASSERT(user_info != 0);
#ifdef HAVE_INITGROUPS
  initgroups(user, user_info->pw_gid);
#endif
  if (setgid(user_info->pw_gid) == -1 || setuid(user_info->pw_uid) == -1)
  {
    my_errno= errno;
    if (MyFlags & MY_WME)
      my_printf_error(errno, "Cannot change uid/gid (errno: %d)", MYF(ME_NOREFRESH),
                      errno);
    DBUG_RETURN(my_errno);
  }
  DBUG_RETURN(0);
}
