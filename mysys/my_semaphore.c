/* Copyright (C) 2002 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Simple implementation of semaphores, needed to compile MySQL on systems
  that doesn't support semaphores.
*/

#include <my_global.h>
#include <my_semaphore.h>
#include <errno.h>

#if !defined(__WIN__) && !defined(HAVE_SEMAPHORE_H) && defined(THREAD)

int sem_init(sem_t * sem, int pshared, uint value)
{
  sem->count=value;
  pthread_cond_init(&sem->cond, 0);
  pthread_mutex_init(&sem->mutex, 0);
  return 0;
}

int sem_destroy(sem_t * sem)
{
  int err1,err2;
  err1=pthread_cond_destroy(&sem->cond);
  err2=pthread_mutex_destroy(&sem->mutex);
  if (err1 || err2)
  {
    errno=err1 ? err1 : err2;
    return -1;
  }
  return 0;
}

int sem_wait(sem_t * sem)
{
  if ((errno=pthread_mutex_lock(&sem->mutex)))
    return -1;
  while (!sem->count)
    pthread_cond_wait(&sem->cond, &sem->mutex);
  if (errno)
    return -1;
  sem->count--; /* mutex is locked here */
  pthread_mutex_unlock(&sem->mutex);
  return 0;
}

int sem_trywait(sem_t * sem)
{
  if ((errno=pthread_mutex_lock(&sem->mutex)))
    return -1;
  if (sem->count)
    sem->count--;
  else
    errno=EAGAIN;
  pthread_mutex_unlock(&sem->mutex);
  return errno ? -1 : 0;
}


int sem_post(sem_t * sem)
{
  if ((errno=pthread_mutex_lock(&sem->mutex)))
    return -1;
  sem->count++;
  pthread_mutex_unlock(&sem->mutex);    /* does it really matter what to do */
  pthread_cond_signal(&sem->cond);      /* first: x_unlock or x_signal ?    */
  return 0;
}

int sem_post_multiple(sem_t * sem, uint count)
{
  if ((errno=pthread_mutex_lock(&sem->mutex)))
    return -1;
  sem->count+=count;
  pthread_mutex_unlock(&sem->mutex);    /* does it really matter what to do */
  pthread_cond_broadcast(&sem->cond);   /* first: x_unlock or x_broadcast ? */
  return 0;
}

int sem_getvalue(sem_t * sem, uint *sval)
{
  if ((errno=pthread_mutex_lock(&sem->mutex)))
    return -1;
  *sval=sem->count;
  pthread_mutex_unlock(&sem->mutex);
  return 0;
}

#endif /* !defined(__WIN__) && !defined(HAVE_SEMAPHORE_H) && defined(THREAD) */
