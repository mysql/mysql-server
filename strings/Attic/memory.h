/*  File   : memory.h
    Author : Richard A. O'Keefe.
    Updated: 1 June 1984
    Purpose: Header file for the System V "memory(3C)" package.

    All the functions in this package are the original work  of  Richard
    A. O'Keefe.   Any resemblance between them and any functions in AT&T
    or other licensed software is due entirely to my use of the System V
    memory(3C) manual page as a specification.	See the READ-ME to  find
    the conditions under which this material may be used and copied.

    The System V manual says that the mem* functions are declared in the
    <memory.h> file.  This file is also included in the <strings.h> file,
    but it does no harm to #include both in either order.
*/

#ifndef DGUX
#ifndef memeql

#define memeql	!memcmp
extern	int	memcmp(/*char^,char^,int*/);
#ifndef memcpy
extern	char	*memcpy(/*char^,char^,int*/);
#endif
extern	char	*memccpy(/*char^,char^,char,int*/);
extern	char	*memset(/*char^,char,int*/);
extern	char	*memchr(/*char^,char,int*/);
extern	char	*memrchr(/*char^,char,int*/);
extern	char	*memmov(/*char^,char^,int*/);
extern	void	memrev(/*char^,char^,int*/);

#endif				/* memeql */
#endif
