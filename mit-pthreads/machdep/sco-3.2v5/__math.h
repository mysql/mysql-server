/* Copyright 1994-1995 The Santa Cruz Operation, Inc. All Rights Reserved. */


#if defined(_NO_PROTOTYPE)	/* Old, crufty environment */
#include <oldstyle/__math.h>
#elif defined(_XOPEN_SOURCE) || defined(_XPG4_VERS)	/* Xpg4 environment */
#include <xpg4/__math.h>
#elif defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) /* Posix environment */
#include <posix/__math.h>
#elif _STRICT_ANSI 	/* Pure Ansi/ISO environment */
#include <ansi/__math.h>
#elif defined(_SCO_ODS_30) /* Old, Tbird compatible environment */
#include <ods_30_compat/__math.h>
#else 	/* Normal, default environment */
/*
 *   Portions Copyright (C) 1983-1995 The Santa Cruz Operation, Inc.
 *		All Rights Reserved.
 *
 *	The information in this file is provided for the exclusive use of
 *	the licensees of The Santa Cruz Operation, Inc.  Such users have the
 *	right to use, modify, and incorporate this code into other products
 *	for purposes authorized by the license agreement provided they include
 *	this notice and the associated copyright notice with any such product.
 *	The information in this file is provided "AS IS" without warranty.
 */

/*	Portions Copyright (c) 1990, 1991, 1992, 1993 UNIX System Laboratories, Inc. */
/*	Portions Copyright (c) 1979 - 1990 AT&T   */
/*	  All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF          */
/*	UNIX System Laboratories, Inc.                          */
/*	The copyright notice above does not evidence any        */
/*	actual or intended publication of such source code.     */

#ifndef ___MATH_H
#define ___MATH_H

#pragma comment(exestr, "xpg4plus @(#) math.h 20.1 94/12/04 ")

#pragma pack(4)

#ifdef __cplusplus
extern "C" {
#endif


extern double	acos(double);
extern double	asin(double);
extern double	atan(double);
extern double	atan2(double, double);
extern double	cos(double);
extern double	sin(double);
extern double	tan(double);

extern double	cosh(double);
extern double	sinh(double);
extern double	tanh(double);

extern double	exp(double);
extern double	frexp(double, int *);
extern double	ldexp(double, int);
extern double	log(double);
extern double	log10(double);
extern double	modf(double, double *);

extern double	pow(double, double);
extern double	sqrt(double);

extern double	ceil(double);
extern double	fabs(double);
extern double	floor(double);
extern double	fmod(double, double);

#ifndef HUGE_VAL
extern const double __huge_val;
#define HUGE_VAL (+__huge_val)
#endif


extern double	erf(double);
extern double	erfc(double);
extern double	gamma(double);
extern double	hypot(double, double);
extern double	j0(double);
extern double	j1(double);
extern double	jn(int, double);
extern double	y0(double);
extern double	y1(double);
extern double	yn(int, double);
extern double	lgamma(double);
extern int	isnan(double);

#define MAXFLOAT	((float)3.40282346638528860e+38)



#define HUGE	MAXFLOAT

/*
 * The following are all legal as XPG4 external functions but must only
 * be declared in the non standards environments as they conflict with
 * the user name space
 */
 
extern long double	frexpl(long double, int *);
extern long double	ldexpl(long double, int);
extern long double	modfl(long double, long double *);

extern float	acosf(float);
extern float	asinf(float);
extern float	atanf(float);
extern float	atan2f(float, float);
extern float	cosf(float);
extern float	sinf(float);
extern float	tanf(float);

extern float	coshf(float);
extern float	sinhf(float);
extern float	tanhf(float);

extern float	expf(float);
extern float	logf(float);
extern float	log10f(float);

extern float	powf(float, float);
extern float	sqrtf(float);

extern float	ceilf(float);
extern float	fabsf(float);
extern float	floorf(float);
extern float	fmodf(float, float);
extern float	modff(float, float *);

/* These are all extensions from XPG4 */

extern double	atof(const char *);
extern double	scalb(double, double);
extern double	logb(double);
extern double	log1p(double);
extern double	nextafter(double, double);
extern double	acosh(double);
extern double	asinh(double);
extern double	atanh(double);
extern double	cbrt(double);
extern double	copysign(double, double);
extern double	expm1(double);
extern int	ilogb(double);
extern double	remainder(double, double);
extern double	rint(double);
extern int	unordered(double, double);
extern int	finite(double);

extern long double	scalbl(long double, long double);
extern long double	logbl(long double);
extern long double	nextafterl(long double, long double);
extern int	unorderedl(long double, long double);
extern int	finitel(long double);




extern int	signgam;

#define M_E		2.7182818284590452354
#define M_LOG2E		1.4426950408889634074
#define M_LOG10E	0.43429448190325182765
#define M_LN2		0.69314718055994530942
#define M_LN10		2.30258509299404568402
#define M_PI		3.14159265358979323846
#define M_PI_2		1.57079632679489661923
#define M_PI_4		0.78539816339744830962
#define M_1_PI		0.31830988618379067154
#define M_2_PI		0.63661977236758134308
#define M_2_SQRTPI	1.12837916709551257390
#define M_SQRT2		1.41421356237309504880
#define M_SQRT1_2	0.70710678118654752440



#define _ABS(x)		((x) < 0 ? -(x) : (x))

#define _REDUCE(TYPE, X, XN, C1, C2)	{ \
	double x1 = (double)(TYPE)X, x2 = X - x1; \
	X = x1 - (XN) * (C1); X += x2; X -= (XN) * (C2); }

#define DOMAIN		1
#define	SING		2
#define	OVERFLOW	3
#define	UNDERFLOW	4
#define	TLOSS		5
#define	PLOSS		6

#define _POLY1(x, c)	((c)[0] * (x) + (c)[1])
#define _POLY2(x, c)	(_POLY1((x), (c)) * (x) + (c)[2])
#define _POLY3(x, c)	(_POLY2((x), (c)) * (x) + (c)[3])
#define _POLY4(x, c)	(_POLY3((x), (c)) * (x) + (c)[4])
#define _POLY5(x, c)	(_POLY4((x), (c)) * (x) + (c)[5])
#define _POLY6(x, c)	(_POLY5((x), (c)) * (x) + (c)[6])
#define _POLY7(x, c)	(_POLY6((x), (c)) * (x) + (c)[7])
#define _POLY8(x, c)	(_POLY7((x), (c)) * (x) + (c)[8])
#define _POLY9(x, c)	(_POLY8((x), (c)) * (x) + (c)[9])


#ifdef __cplusplus
}
inline int sqr(int i) {return(i*i);}
inline double sqr(double i) {return(i*i);}

#endif /* __cplusplus */

#pragma pack()

#if __cplusplus && !defined(PI)
#define PI M_PI
#endif  /* __cplusplus  */

#endif /* _MATH_H */
#endif
