/* ==== pwd_internal.c ============================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
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
 * Description : Thread-safe password hacking functions.
 *
 *  1.00 95/02/08 snl
 *      -Started coding this file.
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include "pwd_internal.h"

static pthread_once_t __pw_init = PTHREAD_ONCE_INIT;
static pthread_key_t __pw_key;

void
_pw_null_cleanup(void *junkola)
{
  pwf_context_t *x = (pwf_context_t *)junkola;

  if (x) {
    if (x->pwf) {
      fclose(x->pwf);
      x->pwf = 0;
    }
#ifdef DBM_PWD_SUPPORT
    if (x->pw_db) {
      dbm_close(x->pw_db);
      x->pw_db = 0;
    }
#endif /* DBM_PWD_SUPPORT */
    free((void *)x);
  }
}

void
_pw_create_key()
{
  if (pthread_key_create(&__pw_key, _pw_null_cleanup)) {
    PANIC();
  }
}

pwf_context_t *
_pw_get_data()
{
  pwf_context_t *_data;

  pthread_once(&__pw_init, _pw_create_key);
  _data = (pwf_context_t *)pthread_getspecific(__pw_key);
  if (!_data) {
    _data = (pwf_context_t *)malloc(sizeof(pwf_context_t));
    if (_data) {
      _data->pwf = 0;
      _data->line[0] = '\0';
      _data->pw_stayopen = 0;
      _data->pw_file = "/etc/passwd";
#ifdef DBM_PWD_SUPPORT
      _data->pw_db = 0;
#endif /* DBM_PWD_SUPPORT */
      pthread_setspecific(__pw_key, (void *)_data);
    }
  }
  return _data;
}
