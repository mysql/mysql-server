/* ==== panic.c =======================================================
 * Copyright (c) 1996 by Larry V. Streepy, Jr.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Larry V. Streepy, Jr.
 * 4. The name of Larry V. Streepy, Jr. may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Larry V. Streepy, Jr. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Larry V. Streepy, Jr. BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : pthread kernel panic
 *
 * 02 Oct 1996 - Larry V. Streepy, Jr.
 *      - Initial coding
 */

#include <pthread.h>
#include <stdio.h>
/*----------------------------------------------------------------------
 * Function:	panic_kernel
 * Purpose:		print a message and panic the pthreads kernel
 * Args:		file name, line number, and function
 * Returns:		doesn't
 * Notes:
 *----------------------------------------------------------------------*/  void  
panic_kernel( const char *file, unsigned int line, const char *func )
{
#ifdef __GNUC__
    (void) fprintf( stderr, "%s:%u: %s%sPhtreads kernel panic.\n",
		    file, line, func ? func : "", func ? ": " : "" );
    (void) fflush (stderr);
#else
    (void) fprintf( stderr, "%s:%u: Phtreads kernel panic.\n", file, line );
    (void) fflush (stderr);
#endif
    abort();
}
