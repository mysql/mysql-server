/*  File   : strcat.c
    Author : Richard A. O'Keefe.
    Updated: 10 April 1984
    Defines: strcat()

    strcat(s, t) concatenates t on the end of s.  There  had  better  be
    enough  room  in  the  space s points to; strcat has no way to tell.
    Note that strcat has to search for the end of s, so if you are doing
    a lot of concatenating it may be better to use strmov, e.g.
	strmov(strmov(strmov(strmov(s,a),b),c),d)
    rather than
	strcat(strcat(strcat(strcpy(s,a),b),c),d).
    strcat returns the old value of s.
*/

#include "strings.h"

char *strcat(register char *s, register const char *t)
{
	char *save;

	for (save = s; *s++; ) ;
	for (--s; *s++ = *t++; ) ;
	return save;
    }
