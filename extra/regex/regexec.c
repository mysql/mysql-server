/*
 * the outer shell of regexec()
 *
 * This file includes engine.c *twice*, after muchos fiddling with the
 * macros that code uses.  This lets the same code operate on two different
 * representations for state sets.
 */
#include <assert.h>
#include <m_ctype.h>
#include <m_string.h>
#include <stdlib.h>
#ifdef _WIN32
#include  <limits.h>
#endif
#include "my_regex.h"
#include "regex2.h"
#include "utils.h"

/* for use in asserts */
#define nope 0

/* macros for manipulating states, small version */
#define	states	unsigned long long
#define	states1	unsigned long long /* for later use in regexec() decision. Ensure Win64 definition is correct.*/
#define	CLEAR(v)	((v) = 0)
#define	SET0(v, n)	((v) &= ~((states) 1 << (n)))
#define	SET1(v, n)	((v) |= (states) 1 << (n))
#define	ISSET(v, n)	((v) & ((states) 1 << (n)))
#define	ASSIGN(d, s)	((d) = (s))
#define	EQ(a, b)	((a) == (b))
#define	STATEVARS	int dummy	/* dummy version */
#define	STATESETUP(m, n)	/* nothing */
#define	STATETEARDOWN(m)	/* nothing */
#define	SETUP(v)	((v) = 0)
#define	onestate	unsigned long long
#define	INIT(o, n)	((o) = (states)1 << (n))
#define	INC(o)	((o) <<= 1)
#define	ISSTATEIN(v, o)	((v) & (o))
/* some abbreviations; note that some of these know variable names! */
/* do "if I'm here, I can also be there" etc without branches */
#define	FWD(dst, src, n)	((dst) |= ((states)(src)&(here)) << (n))
#define	BACK(dst, src, n)	((dst) |= ((states)(src)&(here)) >> (n))
#define	ISSETBACK(v, n)	((v) & ((states)here >> (n)))
/* function names */
#define SNAMES			/* engine.c looks after details */

#include "engine.c"

/* now undo things */
#undef	states
#undef	CLEAR
#undef	SET0
#undef	SET1
#undef	ISSET
#undef	ASSIGN
#undef	EQ
#undef	STATEVARS
#undef	STATESETUP
#undef	STATETEARDOWN
#undef	SETUP
#undef	onestate
#undef	INIT
#undef	INC
#undef	ISSTATEIN
#undef	FWD
#undef	BACK
#undef	ISSETBACK
#undef	SNAMES

/* macros for manipulating states, large version */
#define	states	char *
#define	CLEAR(v)	memset(v, 0, m->g->nstates)
#define	SET0(v, n)	((v)[n] = 0)
#define	SET1(v, n)	((v)[n] = 1)
#define	ISSET(v, n)	((v)[n])
#define	ASSIGN(d, s)	memcpy(d, s, m->g->nstates)
#define	EQ(a, b)	(memcmp(a, b, m->g->nstates) == 0)
#define	STATEVARS	int vn; char *space
#define	STATESETUP(m, nv)	{ (m)->space = malloc((nv)*(m)->g->nstates); \
				if ((m)->space == NULL) return(MY_REG_ESPACE); \
				(m)->vn = 0; }
#define	STATETEARDOWN(m)	{ free((m)->space); }
#define	SETUP(v)	((v) = &m->space[m->vn++ * m->g->nstates])
#define	onestate	unsigned long long
#define	INIT(o, n)	((o) = (n))
#define	INC(o)	((o)++)
#define	ISSTATEIN(v, o)	((v)[o])
/* some abbreviations; note that some of these know variable names! */
/* do "if I'm here, I can also be there" etc without branches */
#define	FWD(dst, src, n)	((dst)[here+(n)] |= (src)[here])
#define	BACK(dst, src, n)	((dst)[here-(n)] |= (src)[here])
#define	ISSETBACK(v, n)	((v)[here - (n)])
/* function names */
#define	LNAMES			/* flag */

#include "engine.c"

/*
 - regexec - interface for matching
 = extern int regexec(const regex_t *, const char *, size_t, \
 =					regmatch_t [], int);
 = #define	MY_REG_NOTBOL	00001
 = #define	MY_REG_NOTEOL	00002
 = #define	MY_REG_STARTEND	00004
 = #define	MY_REG_TRACE	00400	// tracing of execution
 = #define	MY_REG_LARGE	01000	// force large representation
 = #define	MY_REG_BACKR	02000	// force use of backref code
 *
 * We put this here so we can exploit knowledge of the state representation
 * when choosing which matcher to call.  Also, by this point the matchers
 * have been prototyped.
 */

/**
  my_regexec matches the compiled RE pointed to by preg against the
  string, subject to the flags in eflags, and reports results using
  nmatch, pmatch, and the returned value.  The RE must have been
  compiled by a previous invocation of my_regcomp.

  By default, the NULL-terminated string pointed to by string is
  considered to be the text of an entire line, minus any terminating
  newline.  The eflags argument is the bitwise OR of zero or more of
  the following flags:

  MY_REG_NOTBOL   The first character of the string is not the beginning of
                  a line, so the `^' anchor should not match before it.
                  This does not affect the behavior of newlines under
                  MY_REG_NEWLINE.

  MY_REG_NOTEOL   The NULL terminating the string does not end a line, so the
                  `$' anchor should not match before it. This does not affect
                  the behavior of newlines under MY_REG_NEWLINE.

  MY_REG_STARTEND The string is considered to start at string +
                  pmatch[0].rm_so and to have a terminating NUL located
                  at string + pmatch[0].rm_eo (there need not actually be
                  a NUL at that location), regardless of the value of nmatch.

  @return 0 success, MY_REG_NOMATCH failure
 */
int
my_regexec(preg, str, nmatch, pmatch, eflags)
const my_regex_t *preg;
const char *str;
size_t nmatch;
my_regmatch_t pmatch[];
int eflags;
{
	char *pstr = (char *) str;
	struct re_guts *g = preg->re_g;
#ifdef REDEBUG
#	define	GOODFLAGS(f)	(f)
#else
#	define	GOODFLAGS(f)	((f)&(MY_REG_NOTBOL|MY_REG_NOTEOL|MY_REG_STARTEND))
#endif

	if (preg->re_magic != MAGIC1 || g->magic != MAGIC2)
		return(MY_REG_BADPAT);
	assert(!(g->iflags&BAD));
	if (g->iflags&BAD)		/* backstop for no-debug case */
		return(MY_REG_BADPAT);
	eflags = GOODFLAGS(eflags);

	if ((size_t) g->nstates <= CHAR_BIT*sizeof(states1) &&
	    !(eflags&MY_REG_LARGE))
		return(smatcher(preg->charset, g, pstr, nmatch, pmatch, eflags));
	else
		return(lmatcher(preg->charset, g, pstr, nmatch, pmatch, eflags));
}
