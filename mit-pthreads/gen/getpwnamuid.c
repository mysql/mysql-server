/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getpwnamuid.c	5.3 (Berkeley) 12/21/87";
#endif

#include <stdio.h>
#include <pwd.h>
#include <sys/file.h>
#include "pwd_internal.h"

/*
 * The following are shared with getpwent.c
 */

#ifdef DBM_PWD_SUPPORT
static struct passwd *
fetchpw(key)
	datum key;
{
    char *cp, *tp;
	pwf_context_t *_data;

	_data = _pw_get_data();
	if (!_data)
	    return 0;
    if (key.dptr == 0)
        return ((struct passwd *)NULL);
	key = dbm_fetch(_data->pw_db, key);
	if (key.dptr == 0)
                return ((struct passwd *)NULL);
        cp = key.dptr;
	tp = _data->line;

#define	EXPAND(e)	_data->passwd.e = tp; while (*tp++ = *cp++);
	EXPAND(pw_name);
	EXPAND(pw_passwd);
	memcpy((char *)&_data->passwd.pw_uid, cp, sizeof (int));
	cp += sizeof (int);
	memcpy((char *)&_data->passwd.pw_gid, cp, sizeof (int));
	cp += sizeof (int);
	EXPAND(pw_gecos);
	EXPAND(pw_dir);
	EXPAND(pw_shell);
    return (&_data->passwd);
}
#endif /* DBM_PWD_SUPPORT */

struct passwd *
getpwnam(nam)
	const char *nam;
{
#ifdef DBM_PWD_SUPPORT
    datum key;
#endif
	struct passwd *pw, *getpwent();
	pwf_context_t *_data;

	_data = _pw_get_data();
	if (!_data)
	    return 0;

#ifdef DBM_PWD_SUPPORT
    if (_data->pw_db == (DBM *)0 &&
		(_data->pw_db = dbm_open(_data->pw_file, O_RDONLY)) == (DBM *)0) {
	oldcode:
#endif
	  setpwent();
	  while ((pw = getpwent()) && strcmp(nam, pw->pw_name))
		;
	  if (!_data->pw_stayopen)
		endpwent();
	  return (pw);
#ifdef DBM_PWD_SUPPORT
	}
	if (flock(dbm_dirfno(_data->pw_db), LOCK_SH) < 0) {
		dbm_close(_data->pw_db);
		_data->pw_db = (DBM *)0;
		goto oldcode;
	}
	key.dptr = nam;
	key.dsize = strlen(nam);
	pw = fetchpw(key);
	(void) flock(dbm_dirfno(_data->pw_db), LOCK_UN);
	if (!_data->pw_stayopen) {
	  dbm_close(_data->pw_db);
	  _data->pw_db = (DBM *)0;
	}
	return (pw);
#endif
}

struct passwd *
getpwuid(uid)
	uid_t uid;
{
#ifdef DBM_PWD_SUPPORT
        datum key;
#endif
	struct passwd *pw, *getpwent();
	pwf_context_t *_data;

	_data = _pw_get_data();
	if (!_data)
	    return 0;
#ifdef DBM_PWD_SUPPORT
    if (_data->pw_db == (DBM *)0 &&
	    (_data->pw_db = dbm_open(_data->pw_file, O_RDONLY)) == (DBM *)0) {
	oldcode:
#endif
	  setpwent();
	  while ((pw = getpwent()) && pw->pw_uid != uid)
		;
	  if (!_data->pw_stayopen)
		endpwent();
	  return (pw);
#ifdef DBM_PWD_SUPPORT
	}
	if (flock(dbm_dirfno(_data->pw_db), LOCK_SH) < 0) {
		dbm_close(_data->pw_db);
		_data->pw_db = (DBM *)0;
		goto oldcode;
	}
    key.dptr = (char *) &uid;
    key.dsize = sizeof uid;
	pw = fetchpw(key);
	(void) flock(dbm_dirfno(_data->pw_db), LOCK_UN);
	if (!_data->pw_stayopen) {
		dbm_close(_data->pw_db);
		_data->pw_db = (DBM *)0;
	}
    return (pw);
#endif
}
