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

#ifndef __NDBSTRING_H_INCLUDED__
#define __NDBSTRING_H_INCLUDED__

#include <ndb_global.h>
#include <sys/types.h>
#include <string.h>

#ifdef  __cplusplus
extern "C" {
#endif
	
#ifndef HAVE_STRDUP
extern char * strdup(const char *s);
#endif

#ifndef HAVE_STRLCPY
extern size_t strlcpy (char *dst, const char *src, size_t dst_sz);
#endif

#ifndef HAVE_STRLCAT
extern size_t strlcat (char *dst, const char *src, size_t dst_sz);
#endif

#ifndef HAVE_STRCASECMP
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#ifdef  __cplusplus
}
#endif

#endif /* !__NDBSTRING_H_INCLUDED__ */
