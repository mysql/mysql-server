#ifndef _MATH_H_
#define _MATH_H_

/* Needed for HUGE_VAL */
#include <sys/__math.h>

/* XOPEN/SVID */

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
#define	M_E			2.7182818284590452354	/* e */
#define	M_LOG2E		1.4426950408889634074	/* log 2e */
#define	M_LOG10E	0.43429448190325182765	/* log 10e */
#define	M_LN2		0.69314718055994530942	/* log e2 */
#define	M_LN10		2.30258509299404568402	/* log e10 */
#define	M_PI		3.14159265358979323846	/* pi */
#define	M_PI_2		1.57079632679489661923	/* pi/2 */
#define	M_PI_4		0.78539816339744830962	/* pi/4 */
#define	M_1_PI		0.31830988618379067154	/* 1/pi */
#define	M_2_PI		0.63661977236758134308	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define	M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */

#define	MAXFLOAT	((float)3.40282346638528860e+38)

#if !defined(_XOPEN_SOURCE)

struct exception {
	int type;
	char *name;
	double arg1;
	double arg2;
	double retval;
};

#define	HUGE		MAXFLOAT

#define	DOMAIN		1
#define	SING		2
#define	OVERFLOW	3
#define	UNDERFLOW	4
#define	TLOSS		5
#define	PLOSS		6

#endif /* !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */

#include <sys/cdefs.h>

/* ANSI/POSIX */

__BEGIN_DECLS

double hypot	__P_((double, double));
double acos 	__P_((double));
double asin 	__P_((double));
double atan 	__P_((double));
double atan2 	__P_((double, double));
double cos 		__P_((double));
double sin 		__P_((double));
double tan 		__P_((double));

double cosh 	__P_((double));
double sinh 	__P_((double));
double tanh 	__P_((double));

double exp 		__P_((double));
double frexp 	__P_((double, int *));
double ldexp 	__P_((double, int));
double log 		__P_((double));
double log10 	__P_((double));
double modf 	__P_((double, double *));

double pow 		__P_((double, double));
double sqrt 	__P_((double));

double ceil 	__P_((double));
double fabs 	__P_((double));
double floor 	__P_((double));
double fmod 	__P_((double, double));
double rint 	__P_((double));			/* XOPEN; Added by Monty */
int finite	__P_((double dsrc));		/* math.h; added by Monty */
__END_DECLS

#endif /* _MATH_H_ */
