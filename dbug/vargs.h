/******************************************************************************
 *									      *
 *				   N O T I C E				      *
 *									      *
 *		      Copyright Abandoned, 1987, Fred Fish		      *
 *									      *
 *									      *
 *	This previously copyrighted work has been placed into the  public     *
 *	domain	by  the  author  and  may be freely used for any purpose,     *
 *	private or commercial.						      *
 *									      *
 *	Because of the number of inquiries I was receiving about the  use     *
 *	of this product in commercially developed works I have decided to     *
 *	simply make it public domain to further its unrestricted use.	I     *
 *	specifically  would  be  most happy to see this material become a     *
 *	part of the standard Unix distributions by AT&T and the  Berkeley     *
 *	Computer  Science  Research Group, and a standard part of the GNU     *
 *	system from the Free Software Foundation.			      *
 *									      *
 *	I would appreciate it, as a courtesy, if this notice is  left  in     *
 *	all copies and derivative works.  Thank you.			      *
 *									      *
 *	The author makes no warranty of any kind  with	respect  to  this     *
 *	product  and  explicitly disclaims any implied warranties of mer-     *
 *	chantability or fitness for any particular purpose.		      *
 *									      *
 ******************************************************************************
 */


/*
 *  FILE
 *
 *	vargs.h    include file for environments without varargs.h
 *
 *  SCCS
 *
 *	@(#)vargs.h	1.2	5/8/88
 *
 *  SYNOPSIS
 *
 *	#include "vargs.h"
 *
 *  DESCRIPTION
 *
 *	This file implements a varargs macro set for use in those
 *	environments where there is no system supplied varargs.  This
 *	generally works because systems which don't supply a varargs
 *	package are precisely those which don't strictly need a varargs
 *	package.  Using this one then allows us to minimize source
 *	code changes.  So in some sense, this is a "portable" varargs
 *	since it is only used for convenience, when it is not strictly
 *	needed.
 *
 */

/*
 *	These macros allow us to rebuild an argument list on the stack
 *	given only a va_list.  We can use these to fake a function like
 *	vfprintf, which gets a fixed number of arguments, the last of
 *	which is a va_list, by rebuilding a stack and calling the variable
 *	argument form fprintf.	Of course this only works when vfprintf
 *	is not available in the host environment, and thus is not available
 *	for fprintf to call (which would give us an infinite loop).
 *
 *	Note that ARGS_TYPE is a long, which lets us get several bytes
 *	at a time while also preventing lots of "possible pointer alignment
 *	problem" messages from lint.  The messages are valid, because this
 *	IS nonportable, but then we should only be using it in very
 *	nonrestrictive environments, and using the real varargs where it
 *	really counts.
 *
 */

#define ARG0 a0
#define ARG1 a1
#define ARG2 a2
#define ARG3 a3
#define ARG4 a4
#define ARG5 a5
#define ARG6 a6
#define ARG7 a7
#define ARG8 a8
#define ARG9 a9

#define ARGS_TYPE long
#define ARGS_LIST ARG0,ARG1,ARG2,ARG3,ARG4,ARG5,ARG6,ARG7,ARG8,ARG9
#define ARGS_DCL auto ARGS_TYPE ARGS_LIST

/*
 *	A pointer of type "va_list" points to a section of memory
 *	containing an array of variable sized arguments of unknown
 *	number.  This pointer is initialized by the va_start
 *	macro to point to the first byte of the first argument.
 *	We can then use it to walk through the argument list by
 *	incrementing it by the size of the argument being referenced.
 */

typedef char *va_list;

/*
 *	The first variable argument overlays va_alist, which is
 *	nothing more than a "handle" which allows us to get the
 *	address of the first argument on the stack.  Note that
 *	by definition, the va_dcl macro includes the terminating
 *	semicolon, which makes use of va_dcl in the source code
 *	appear to be missing a semicolon.
 */

#define va_dcl ARGS_TYPE va_alist;

/*
 *	The va_start macro takes a variable of type "va_list" and
 *	initializes it.  In our case, it initializes a local variable
 *	of type "pointer to char" to point to the first argument on
 *	the stack.
 */

#define va_start(list) list = (char *) &va_alist

/*
 *	The va_end macro is a null operation for our use.
 */

#define va_end(list)

/*
 *	The va_arg macro is the tricky one.  This one takes
 *	a va_list as the first argument, and a type as the second
 *	argument, and returns a value of the appropriate type
 *	while advancing the va_list to the following argument.
 *	For our case, we first increment the va_list arg by the
 *	size of the type being recovered, cast the result to
 *	a pointer of the appropriate type, and then dereference
 *	that pointer as an array to get the previous arg (which
 *	is the one we wanted.
 */

#define va_arg(list,type) ((type *) (list += sizeof (type)))[-1]
