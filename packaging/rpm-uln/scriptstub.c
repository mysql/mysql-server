#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Translate call of myself into call of same-named script in LIBDIR */
/* The macro LIBDIR must be defined as a double-quoted string */

int main (int argc, char **argv)
{
  char *basename;
  char *fullname;
  char **newargs;
  int i;

  basename = strrchr(argv[0], '/');
  if (basename)
    basename++;
  else
    basename = argv[0];
  fullname = malloc(strlen(LIBDIR) + strlen(basename) + 2);
  sprintf(fullname, "%s/%s", LIBDIR, basename);
  newargs = malloc((argc+1) * sizeof(char *));
  newargs[0] = fullname;
  for (i = 1; i < argc; i++)
    newargs[i] = argv[i];
  newargs[argc] = NULL;

  execvp(fullname, newargs);

  return 1;
}
