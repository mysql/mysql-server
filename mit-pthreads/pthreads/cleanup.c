/* ==== cleanup.c =======================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@mit.edu
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
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : Pthread attribute functions.
 *
 *  1.20 94/02/13 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

/* ==========================================================================
 * pthread_cleanup_push()
 */
int pthread_cleanup_push(void (*routine)(void *), void *routine_arg)
{
  struct pthread_cleanup *new;
  int ret;

  if ((new = (struct pthread_cleanup*)malloc(sizeof(struct pthread_cleanup))))
  {
    new->routine = routine;
    new->routine_arg = routine_arg;
    new->next = pthread_run->cleanup;

    pthread_run->cleanup = new;
    ret = OK;
  } else {
    ret = ENOMEM;
  }
  return(ret);
}

/* ==========================================================================
 * pthread_cleanup_pop()
 */
void pthread_cleanup_pop(int execute)
{
  struct pthread_cleanup *old;

  if ((old = pthread_run->cleanup))
  {
    pthread_run->cleanup = old->next;
    if (execute) {
      old->routine(old->routine_arg);
    }
    free(old);
  }
}

