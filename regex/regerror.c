#include <my_global.h>
#include <m_string.h>
#include <m_ctype.h>

#include "my_regex.h"
#include "utils.h"
#include "regerror.ih"

/*
 = #define	MY_REG_NOMATCH	 1
 = #define	MY_REG_BADPAT	 2
 = #define	MY_REG_ECOLLATE	 3
 = #define	MY_REG_ECTYPE	 4
 = #define	MY_REG_EESCAPE	 5
 = #define	MY_REG_ESUBREG	 6
 = #define	MY_REG_EBRACK	 7
 = #define	MY_REG_EPAREN	 8
 = #define	MY_REG_EBRACE	 9
 = #define	MY_REG_BADBR	10
 = #define	MY_REG_ERANGE	11
 = #define	MY_REG_ESPACE	12
 = #define	MY_REG_BADRPT	13
 = #define	MY_REG_EMPTY	14
 = #define	MY_REG_ASSERT	15
 = #define	MY_REG_INVARG	16
 = #define	MY_REG_ATOI	255	// convert name to number (!)
 = #define	MY_REG_ITOA	0400	// convert number to name (!)
 */
static struct rerr {
	int code;
	const char *name;
        const char *explain;
} rerrs[] = {
	{MY_REG_NOMATCH,	"MY_REG_NOMATCH",	"regexec() failed to match"},
	{MY_REG_BADPAT,	"MY_REG_BADPAT",	"invalid regular expression"},
	{MY_REG_ECOLLATE,	"MY_REG_ECOLLATE",	"invalid collating element"},
	{MY_REG_ECTYPE,	"MY_REG_ECTYPE",	"invalid character class"},
	{MY_REG_EESCAPE,	"MY_REG_EESCAPE",	"trailing backslash (\\)"},
	{MY_REG_ESUBREG,	"MY_REG_ESUBREG",	"invalid backreference number"},
	{MY_REG_EBRACK,	"MY_REG_EBRACK",	"brackets ([ ]) not balanced"},
	{MY_REG_EPAREN,	"MY_REG_EPAREN",	"parentheses not balanced"},
	{MY_REG_EBRACE,	"MY_REG_EBRACE",	"braces not balanced"},
	{MY_REG_BADBR,	"MY_REG_BADBR",	"invalid repetition count(s)"},
	{MY_REG_ERANGE,	"MY_REG_ERANGE",	"invalid character range"},
	{MY_REG_ESPACE,	"MY_REG_ESPACE",	"out of memory"},
	{MY_REG_BADRPT,	"MY_REG_BADRPT",	"repetition-operator operand invalid"},
	{MY_REG_EMPTY,	"MY_REG_EMPTY",	"empty (sub)expression"},
	{MY_REG_ASSERT,	"MY_REG_ASSERT",	"\"can't happen\" -- you found a bug"},
	{MY_REG_INVARG,	"MY_REG_INVARG",	"invalid argument to regex routine"},
	{0,		"",		"*** unknown regexp error code ***"},
};

/*
 - regerror - the interface to error numbers
 = extern size_t regerror(int, const regex_t *, char *, size_t);
 */
/* ARGSUSED */
size_t
my_regerror(int errcode, const my_regex_t *preg, char *errbuf, size_t errbuf_size)
{
	register struct rerr *r;
	register size_t len;
	register int target = errcode &~ MY_REG_ITOA;
	register char *s;
	char convbuf[50];

	if (errcode == MY_REG_ATOI)
		s = regatoi(preg, convbuf);
	else {
		for (r = rerrs; r->code != 0; r++)
			if (r->code == target)
				break;

		if (errcode&MY_REG_ITOA) {
			if (r->code != 0)
				(void) strcpy(convbuf, r->name);
			else
				sprintf(convbuf, "MY_REG_0x%x", target);
			assert(strlen(convbuf) < sizeof(convbuf));
			s = convbuf;
		} else
			s = (char*) r->explain;
	}

	len = strlen(s) + 1;
	if (errbuf_size > 0) {
		if (errbuf_size > len)
			(void) strcpy(errbuf, s);
		else {
			(void) strncpy(errbuf, s, errbuf_size-1);
			errbuf[errbuf_size-1] = '\0';
		}
	}

	return(len);
}

/*
 - regatoi - internal routine to implement MY_REG_ATOI
 == static char *regatoi(const regex_t *preg, char *localbuf);
 */
static char *
regatoi(preg, localbuf)
const my_regex_t *preg;
char *localbuf;
{
	register struct rerr *r;
	for (r = rerrs; r->code != 0; r++)
		if (strcmp(r->name, preg->re_endp) == 0)
			break;
	if (r->code == 0)
		return((char*) "0");

	sprintf(localbuf, "%d", r->code);
	return(localbuf);
}
