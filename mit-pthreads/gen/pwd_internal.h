#ifndef _PWD_INTERNAL_H_
#define _PWD_INTERNAL_H_

#if 0 /* Turn this off for now until we suck in ndbm or use gdbm -- SNL */
#ifndef DBM_PWD_SUPPORT
#if !defined(__alpha) && !defined(linux) && !defined(hpux)
#define DBM_PWD_SUPPORT 1
#endif /* !alpha && !linux && !hpux */
#endif /* !DBM_PWD_SUPPORT */
#endif

#ifdef DBM_PWD_SUPPORT
#include <ndbm.h>
#endif /* DBM_PWD_SUPPORT */

typedef struct pwf_context {
  FILE *pwf;
  char line[BUFSIZ+1];
  struct passwd passwd;
  int	pw_stayopen;
  char *pw_file;
#ifdef DBM_PWD_SUPPORT
  DBM	*pw_db;
#endif /* DBM_PWD_SUPPORT */
} pwf_context_t;

pwf_context_t *_pw_get_data __P_((void));

#endif /* _PWD_INTERNAL_H_ */
