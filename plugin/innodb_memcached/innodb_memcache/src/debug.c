
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <pthread.h>

#include "debug.h"

FILE *debug_outfile;
pthread_mutex_t outfile_lock = PTHREAD_MUTEX_INITIALIZER;
int do_debug = 0;

void ndbmc_debug_init(const char *filename, bool enable) {
  if(! enable) return;
  
  do_debug = 1;
  if(filename) 
    debug_outfile = fopen(filename, "w");
  else 
    debug_outfile = fdopen(STDERR_FILENO, "a");
  assert(debug_outfile);
}


void ndbmc_debug_print(int thread_id, 
                       const char *thread_name, 
                       const char *function, 
                       const char *fmt, ... ) {
  va_list args;

  pthread_mutex_lock(& outfile_lock);
  if(thread_name && *thread_name) 
    fprintf(debug_outfile, "t%d.%s ", thread_id, thread_name);

  fprintf(debug_outfile,"%s(): ", function);
  va_start(args, fmt);
  vfprintf(debug_outfile, fmt, args);
  va_end(args);
  fputc('\n', debug_outfile);
  pthread_mutex_unlock(& outfile_lock);
}



void ndbmc_debug_enter(int thread_id, 
                       const char *thread_name, 
                       const char *func) {
  if(thread_name && *thread_name) 
    fprintf(debug_outfile, "t%d.%s --> %s()\n", thread_id, thread_name, func);
  else
    fprintf(debug_outfile, " --> %s()\n", func);
}

