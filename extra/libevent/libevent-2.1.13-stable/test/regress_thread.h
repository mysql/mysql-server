/*
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef REGRESS_THREAD_H_INCLUDED_
#define REGRESS_THREAD_H_INCLUDED_

#if defined(_WIN32) /** _WIN32 */
#define THREAD_T void * /* HANDLE */
#define THREAD_FN unsigned __stdcall
#define THREAD_RETURN() return (0)
#define THREAD_SELF() GetCurrentThreadId()
#define THREAD_START(threadvar, fn, arg) do {                         \
	uintptr_t threadhandle = _beginthreadex(NULL,0,fn,(arg),0,NULL);  \
	(threadvar) = (THREAD_T)threadhandle;                             \
	thread_setup(threadvar);                                          \
} while (0)
#define THREAD_JOIN(th) WaitForSingleObject(th, INFINITE)
#else /* !_WIN32 */
#include <pthread.h>
#define THREAD_T pthread_t
#define THREAD_FN void *
#define THREAD_RETURN() return (NULL)
#define THREAD_SELF() pthread_self()
#define THREAD_START(threadvar, fn, arg) do {          \
	if (!pthread_create(&(threadvar), NULL, fn, arg))  \
		thread_setup(threadvar);                       \
} while (0)
#define THREAD_JOIN(th) pthread_join(th, NULL)
#endif /* \!_WIN32 */

void thread_setup(THREAD_T pthread);

#endif
