/*
 * Copyright (c) 1984 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getpwent.c	5.2 (Berkeley) 3/9/86";
#endif

#include <pthread.h>
#include <stdio.h>
#include <pwd.h>
#include "pwd_internal.h"

void
setpwent()
{
    pwf_context_t *_data;

    _data = _pw_get_data();

	if (_data) {
	  if (_data->pwf == NULL)
		_data->pwf = fopen(_data->pw_file, "r");
	  else
		rewind(_data->pwf);
	}
}

void
endpwent()
{
    pwf_context_t *_data;

	_data = _pw_get_data();

	if (_data) {
	  if (_data->pwf != NULL) {
		fclose(_data->pwf);
		_data->pwf = NULL;
	  }
#ifdef DBM_PWD_SUPPORT
	  if (_data->pw_db != (DBM *)0) {
		dbm_close(_data->pw_db);
		_data->pw_db = (DBM *)0;
		_data->pw_stayopen = 0;
	  }
#endif /* DBM_PWD_SUPPORT */
    }
}

static char *
pwskip(p)
	 char *p;
{
	while (*p && *p != ':' && *p != '\n')
		++p;
	if (*p)
		*p++ = 0;
	return(p);
}

struct passwd *
getpwent()
{
    pwf_context_t *_data;
	char *p;

	_data = _pw_get_data();
	if (!_data)
	    return 0;

	if (_data->pwf == NULL) {
		if ((_data->pwf = fopen(_data->pw_file, "r" )) == NULL)
			return(0);
	}
	p = fgets(_data->line, BUFSIZ, _data->pwf);
	if (p == NULL)
		return(0);
	_data->passwd.pw_name = p;
	p = pwskip(p);
	_data->passwd.pw_passwd = p;
	p = pwskip(p);
	_data->passwd.pw_uid = atoi(p);
	p = pwskip(p);
	_data->passwd.pw_gid = atoi(p);
	p = pwskip(p);
	_data->passwd.pw_gecos = p;
	p = pwskip(p);
	_data->passwd.pw_dir = p;
	p = pwskip(p);
	_data->passwd.pw_shell = p;
	while (*p && *p != '\n')
		p++;
	*p = '\0';
	return(&_data->passwd);
}

void
setpwfile(file)
	char *file;
{
    pwf_context_t *_data;

	_data = _pw_get_data();
	if (_data)
	    _data->pw_file = file;
}
