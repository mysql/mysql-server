/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* can't use -lmysys because this prog is used to create -lstrings */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHARSETS_SUBDIR "sql/share/charsets"
#define CTYPE_TABLE_SIZE      257
#define TO_LOWER_TABLE_SIZE   256
#define TO_UPPER_TABLE_SIZE   256
#define SORT_ORDER_TABLE_SIZE 256
#define ROW_LEN 16

void print_arrays_for(char *set);

char *prog;
char buf[1024], *p, *endptr;

int
main(int argc, char **argv)
{
  prog = *argv;

  if (argc < 2) {
    fprintf(stderr, "usage: %s source-dir [charset [, charset]]\n", prog);
    exit(EXIT_FAILURE);
  }

  --argc; ++argv;       /* skip program name */

  if (chdir(*argv) != 0) {
    fprintf(stderr, "%s: can't cd to %s\n", prog, *argv);
    exit(EXIT_FAILURE);
  }
  --argc; ++argv;

  if (chdir(CHARSETS_SUBDIR) != 0) {
    fprintf(stderr, "%s: can't cd to %s\n", prog, CHARSETS_SUBDIR);
    exit(EXIT_FAILURE);
  }

  while (argc--)
    print_arrays_for(*argv++);

  exit(EXIT_SUCCESS);
}

void
print_array(FILE *f, const char *set, const char *name, int n)
{
  int i;
  char val[100];

  printf("uchar %s_%s[] = {\n", name, set);

  p = buf;
  *buf = '\0';
  for (i = 0; i < n; ++i)
  {
    /* get a word from f */
    endptr = p;
    for (;;)
    {
      while (isspace((* (unsigned char*) endptr)))
        ++endptr;
      if (*endptr && *endptr != '#')    /* not comment */
        break;
      if ((fgets(buf, sizeof(buf), f)) == NULL)
        return;         /* XXX: break silently */
      endptr = buf;
    }

    p = val;
    while (!isspace((* (unsigned char*) endptr)))
      *p++ = *endptr++;
    *p = '\0';
    p = endptr;

    /* write the value out */

    if (i == 0 || i % ROW_LEN == n % ROW_LEN)
      printf("  ");

    printf("%3d", (unsigned char) strtol(val, (char **) NULL, 16));

    if (i < n - 1)
      printf(",");

    if ((i+1) % ROW_LEN == n % ROW_LEN)
      printf("\n");
  }

  printf("};\n\n");
}

void
print_arrays_for(char *set)
{
  FILE *f;

  sprintf(buf, "%s.conf", set);

  if ((f = fopen(buf, "r")) == NULL) {
    fprintf(stderr, "%s: can't read conf file for charset %s\n", prog, set);
    exit(EXIT_FAILURE);
  }

  printf("\
/* The %s character set.  Generated automatically by configure and\n\
 * the %s program\n\
 */\n\n",
	 set, prog);

  /* it would be nice if this used the code in mysys/charset.c, but... */
  print_array(f, set, "ctype",      CTYPE_TABLE_SIZE);
  print_array(f, set, "to_lower",   TO_LOWER_TABLE_SIZE);
  print_array(f, set, "to_upper",   TO_UPPER_TABLE_SIZE);
  print_array(f, set, "sort_order", SORT_ORDER_TABLE_SIZE);
  printf("\n");

  fclose(f);

  return;
}
