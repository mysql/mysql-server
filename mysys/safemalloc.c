/* Copyright (C) 2000 MySQL AB, 2011 Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/********************************************************************
  memory debugger
  based on safemalloc, memory sub-system, written by Bjorn Benson
********************************************************************/


#include "mysys_priv.h"
#include <my_stacktrace.h>      /* my_addr_resolve */

#if HAVE_EXECINFO_H
#include <execinfo.h>
#endif

/*
  this can be set to 1 if we leak memory and know it
  (to disable memory leak tests on exit)
*/
int sf_leaking_memory= 0;

#ifdef SAFEMALLOC

/* this mutex protects all sf_* variables, and nothing else*/
static pthread_mutex_t sf_mutex;
static int init_done= 0;

#ifndef SF_REMEMBER_FRAMES
#define SF_REMEMBER_FRAMES 8
#endif

/* ignore the first two frames (sf_malloc itself, and my_malloc) */
#define SF_FRAMES_SKIP          2

/*
  Structure that stores information of an allocated memory block
  The data is at &struct_adr+sizeof(struct irem)
  Note that sizeof(struct st_irem) % sizeof(double) == 0
*/
struct st_irem
{
  struct st_irem *next;        /* Linked list of structures       */
  struct st_irem *prev;        /* Other link                      */
  size_t datasize;             /* Size requested                  */
#ifdef HAVE_BACKTRACE
  void *frame[SF_REMEMBER_FRAMES]; /* call stack                  */
#endif
  uint32 marker;               /* Underrun marker value           */
};

static int    sf_malloc_count= 0;              /* Number of allocated chunks */

static void  *sf_min_adress= (void*) (intptr)~0ULL,
             *sf_max_adress= 0;

static struct st_irem *sf_malloc_root = 0;

#define MAGICSTART      0x14235296      /* A magic value for underrun key */

#define MAGICEND0       0x68            /* Magic values for overrun keys  */
#define MAGICEND1       0x34            /*              "                 */
#define MAGICEND2       0x7A            /*              "                 */
#define MAGICEND3       0x15            /*              "                 */

static int bad_ptr(const char *where, void *ptr);
static void free_memory(void *ptr);
static void sf_terminate();

/**
  allocates memory
*/

void *sf_malloc(size_t size)
{
  struct st_irem *irem;
  uchar *data;

  /*
    this style of initialization looks like race conditon prone,
    but it is safe under the assumption that a program does
    at least one malloc() while still being single threaded.
  */
  if (!init_done)
  {
    pthread_mutex_init(&sf_mutex, NULL);
    /* disable deadlock detector, because it calls my_malloc() */
    safe_mutex_setflags(&sf_mutex, MYF_NO_DEADLOCK_DETECTION);
    atexit(sf_terminate);
    init_done= 1;
  }

  irem= (struct st_irem *) malloc (sizeof(struct st_irem) + size + 4);

  if (!irem)
    return 0;

  /* we guarantee the alignment */
  compile_time_assert(sizeof(struct st_irem) % sizeof(double) == 0);

  /* Fill up the structure */
  data= (uchar*) (irem + 1);
  irem->datasize= size;
  irem->prev=     0;
  irem->marker=   MAGICSTART; 
  data[size + 0]= MAGICEND0;
  data[size + 1]= MAGICEND1;
  data[size + 2]= MAGICEND2;
  data[size + 3]= MAGICEND3;

#ifdef HAVE_BACKTRACE
  {
    void *frame[SF_REMEMBER_FRAMES + SF_FRAMES_SKIP];
    int frames= backtrace(frame, array_elements(frame));
    if (frames < SF_FRAMES_SKIP)
      frames= 0;
    else
    {
      frames-= SF_FRAMES_SKIP;
      memcpy(irem->frame, frame + SF_FRAMES_SKIP, sizeof(void*)*frames);
    }
    if (frames < SF_REMEMBER_FRAMES)
      irem->frame[frames]= 0;
  }
#endif

  pthread_mutex_lock(&sf_mutex);

  /* Add this structure to the linked list */
  if ((irem->next= sf_malloc_root))
    sf_malloc_root->prev= irem;
  sf_malloc_root= irem;

  /* Keep the statistics */
  sf_malloc_count++;
  set_if_smaller(sf_min_adress, (void*)data);
  set_if_bigger(sf_max_adress, (void*)data);

  pthread_mutex_unlock(&sf_mutex);

  TRASH_ALLOC(data, size);
  return data;
}

void *sf_realloc(void *ptr, size_t size)
{
  char *data;

  if (!ptr)
    return sf_malloc(size);

  if (bad_ptr("Reallocating", ptr))
    return 0;

  if ((data= sf_malloc(size)))
  {
    struct st_irem *irem= (struct st_irem *)ptr - 1;
    set_if_smaller(size, irem->datasize);
    memcpy(data, ptr, size);
    free_memory(ptr);
  }
  return data;
}

void sf_free(void *ptr)
{
  if (!ptr || bad_ptr("Freeing", ptr))
    return;

  free_memory(ptr);
}

static void free_memory(void *ptr)
{
  struct st_irem *irem= (struct st_irem *)ptr - 1;

  pthread_mutex_lock(&sf_mutex);
  /* Remove this structure from the linked list */
  if (irem->prev)
    irem->prev->next= irem->next;
   else
    sf_malloc_root= irem->next;

  if (irem->next)
    irem->next->prev= irem->prev;

  /* Handle the statistics */
  sf_malloc_count--;
  pthread_mutex_unlock(&sf_mutex);

  /* only trash the data and magic values, but keep the stack trace */
  TRASH_FREE((uchar*)(irem + 1) - 4, irem->datasize + 8);
  free(irem);
  return;
}

#ifdef HAVE_BACKTRACE
static void print_stack(void **frame)
{
  const char *err;
  int i;

  if ((err= my_addr_resolve_init()))
  {
    fprintf(stderr, "(my_addr_resolve failure: %s)\n", err);
    return;
  }

  for (i=0; i < SF_REMEMBER_FRAMES && frame[i]; i++)
  {
    my_addr_loc loc;
    if (i)
      fprintf(stderr, ", ");

    if (my_addr_resolve(frame[i], &loc))
      fprintf(stderr, "%p", frame[i]);
    else
      fprintf(stderr, "%s:%u", loc.file, loc.line);
  }
  fprintf(stderr, "\n");
}
#else
#define print_stack(X)          fprintf(stderr, "???\n")
#endif

static void warn(const char *format,...)
{
  va_list args;
  va_start(args,format);
  vfprintf(stderr, format, args);
  va_end(args);

#ifdef HAVE_BACKTRACE
  {
    void *frame[SF_REMEMBER_FRAMES + SF_FRAMES_SKIP];
    int frames= backtrace(frame, array_elements(frame));
    fprintf(stderr, " ");
    if (frames < SF_REMEMBER_FRAMES + SF_FRAMES_SKIP)
      frame[frames]= 0;
    print_stack(frame + SF_FRAMES_SKIP);
  }
#endif
}

static int bad_ptr(const char *where, void *ptr)
{
  struct st_irem *irem= (struct st_irem *)ptr - 1;
  const uchar *magicend;

  if (((intptr) ptr) % sizeof(double))
  {
    warn("Error: %s wrong aligned pointer", where);
    return 1;
  }
  if (ptr < sf_min_adress || ptr > sf_max_adress)
  {
    warn("Error: %s pointer out of range", where);
    return 1;
  }
  if (irem->marker != MAGICSTART)
  {
    warn("Error: %s unallocated data or underrun buffer", where);
    return 1;
  }

  magicend= (uchar*)ptr + irem->datasize;
  if (magicend[0] != MAGICEND0 ||
      magicend[1] != MAGICEND1 ||
      magicend[2] != MAGICEND2 ||
      magicend[3] != MAGICEND3)
  {
    warn("Error: %s overrun buffer ", where);
    fprintf(stderr, "Allocated at ");
    print_stack(irem->frame);
    return 1;
  }

  return 0;
}

/* check all allocated memory list for consistency */
static int sf_sanity()
{
  struct st_irem *irem;
  int flag= 0;
  int count= 0;

  pthread_mutex_lock(&sf_mutex);
  count= sf_malloc_count;
  for (irem= sf_malloc_root; irem && count > 0; count--, irem= irem->next)
    flag+= bad_ptr("Safemalloc", irem + 1);
  pthread_mutex_unlock(&sf_mutex);
  if (count || irem)
  {
    warn("Error: Safemalloc link list destroyed");
    return 1;
  }
  return 0;
}

/**
  report on all the memory pieces that have not been free'd
*/

static void sf_terminate()
{
  size_t total= 0;
  struct st_irem *irem;

  sf_sanity();

  /* Report on all the memory that was allocated but not free'd */
  if (!sf_leaking_memory && sf_malloc_root)
  {
    for (irem= sf_malloc_root; irem; irem= irem->next)
    {
      fprintf(stderr, "Warning: %4lu bytes lost, allocated at ",
              (ulong) irem->datasize);
      print_stack(irem->frame);
      total+= irem->datasize;
    }
    fprintf(stderr, "Memory lost: %lu bytes in %d chunks\n",
            (ulong) total, sf_malloc_count);
  }

  pthread_mutex_destroy(&sf_mutex);
  return;
}

#endif
