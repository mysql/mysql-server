/*
 * Module: semaphore.h
 *
 * Purpose:
 *      Semaphores aren't actually part of the PThreads standard.
 *      They are defined by the POSIX Standard:
 *
 *              POSIX 1003.1b-1993      (POSIX.1b)
 *
 * Pthreads-win32 - POSIX Threads Library for Win32
 * Copyright (C) 1998
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA
 */

/* This is hacked by Monty to be included in mysys library */

#ifndef _my_semaphore_h_
#define _my_semaphore_h_

#ifdef THREAD

C_MODE_START
#ifdef HAVE_SEMAPHORE_H
#include <semaphore.h>
#elif !defined(__bsdi__)
#ifdef __WIN__
typedef HANDLE sem_t;
#else
typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t  cond;
  uint            count;
} sem_t;
#endif /* __WIN__ */

int sem_init(sem_t * sem, int pshared, unsigned int value);
int sem_destroy(sem_t * sem);
int sem_trywait(sem_t * sem);
int sem_wait(sem_t * sem);
int sem_post(sem_t * sem);
int sem_post_multiple(sem_t * sem, unsigned int count);
int sem_getvalue(sem_t * sem, unsigned int * sval);

#endif /* !__bsdi__ */

C_MODE_END

#endif /* THREAD */

#endif /* !_my_semaphore_h_ */
