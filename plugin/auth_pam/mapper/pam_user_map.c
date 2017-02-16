/*
  Pam module to change user names arbitrarily in the pam stack.

  Compile as
  
     gcc pam_user_map.c -shared -lpam -fPIC -o pam_user_map.so

  Install as appropriate (for example, in /lib/security/).
  Add to your /etc/pam.d/mysql (preferrably, at the end) this line:
=========================================================
auth            required        pam_user_map.so
=========================================================

  And create /etc/security/user_map.conf with the desired mapping
  in the format:  orig_user_name: mapped_user_name
=========================================================
#comments and emty lines are ignored
john: jack
bob:  admin
top:  accounting
=========================================================

*/

#include <stdio.h>
#include <syslog.h>
#include <security/pam_modules.h>

#define FILENAME "/etc/security/user_map.conf"
#define skip(what) while (*s && (what)) s++

int pam_sm_authenticate(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{
  int pam_err, line= 0;
  const char *username;
  char buf[256];
  FILE *f;

  f= fopen(FILENAME, "r");
  if (f == NULL)
  {
    pam_syslog(pamh, LOG_ERR, "Cannot open '%s'\n", FILENAME);
    return PAM_SYSTEM_ERR;
  }

  pam_err = pam_get_item(pamh, PAM_USER, (const void**)&username);
  if (pam_err != PAM_SUCCESS)
    goto ret;

  while (fgets(buf, sizeof(buf), f) != NULL)
  {
    char *s= buf, *from, *to, *end_from, *end_to;
    line++;

    skip(isspace(*s));
    if (*s == '#' || *s == 0) continue;
    from= s;
    skip(isalnum(*s) || (*s == '_'));
    end_from= s;
    skip(isspace(*s));
    if (end_from == from || *s++ != ':') goto syntax_error;
    skip(isspace(*s));
    to= s;
    skip(isalnum(*s) || (*s == '_'));
    end_to= s;
    if (end_to == to) goto syntax_error;

    *end_from= *end_to= 0;
    if (strcmp(username, from) == 0)
    {
      pam_err= pam_set_item(pamh, PAM_USER, to);
      goto ret;
    }
  }
  pam_err= PAM_SUCCESS;
  goto ret;

syntax_error:
  pam_syslog(pamh, LOG_ERR, "Syntax error at %s:%d", FILENAME, line);
  pam_err= PAM_SYSTEM_ERR;
ret:
  fclose(f);
  return pam_err;
}

int pam_sm_setcred(pam_handle_t *pamh, int flags,
                   int argc, const char *argv[])
{

    return PAM_SUCCESS;
}

