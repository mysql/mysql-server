/* This function is a replacement for the version in libgcc.a.  This
   is needed because typically libgcc.a won't have been compiled
   against the threads library, so its references to "stderr" will
   come out wrong.  */

#include <stdio.h>

void __eprintf (const char *fmt, const char *expr, int line, const char *file)
{
  /* Considering the very special circumstances where this function
     would be called, perhaps we might want to disable the thread
     scheduler and break any existing locks on stderr?  Well, maybe if
     we could be sure that stderr was in a useable state...  */
  fprintf (stderr, fmt, expr, line, file);
  fflush (stderr);

  abort ();
}
