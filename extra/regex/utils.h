#include <limits.h>

/* utility definitions */
#ifdef _POSIX2_RE_DUP_MAX
#define	DUPMAX		_POSIX2_RE_DUP_MAX	/* xxx is this right? */
#else
#define DUPMAX		255
#endif
#define	RE_INFINITY	(DUPMAX + 1)
#define	NC		(CHAR_MAX - CHAR_MIN + 1)
typedef unsigned char uch;

/* switch off assertions (if not already off) if no REDEBUG */
#ifndef REDEBUG
#ifndef NDEBUG
#define	NDEBUG	/* no assertions please */
#endif
#endif
#include <assert.h>
