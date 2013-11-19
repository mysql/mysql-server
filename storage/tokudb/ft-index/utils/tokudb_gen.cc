/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <toku_portability.h>
#include <toku_assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <db.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#if IS_TDB
#include <src/ydb.h>
#endif

#include "tokudb_common.h"

typedef struct {
   DB_ENV*       dbenv;
   bool        plaintext;
   char*       progname;
} gen_globals;

gen_globals g;
#include "tokudb_common_funcs.h"

static int   usage(void);
static void  generate_keys(void);
static int   get_delimiter(char* str);



char           dbt_delimiter  = '\n';
char           sort_delimiter[3];
uint32_t       lengthmin      = 0;
bool           set_lengthmin  = false;
uint32_t       lengthlimit    = 0;
bool           set_lengthlimit= false;
uint64_t       numkeys        = 0;
bool           set_numkeys    = false;
bool           header         = true;
bool           footer         = true;
bool           justheader     = false;
bool           justfooter     = false;
bool           outputkeys     = true;
uint32_t       seed           = 1;
bool           set_seed       = false;
bool           printableonly  = false;
bool           leadingspace   = true;
bool           force_unique   = true;
bool           dupsort        = false;

static int test_main (int argc, char *const argv[]) {
   int ch;

   /* Set up the globals. */
   memset(&g, 0, sizeof(g));

   g.progname = argv[0];

   if (verify_library_version() != 0) goto error;
   
   strcpy(sort_delimiter, "");

   while ((ch = getopt(argc, argv, "PpTo:r:m:M:n:uVhHfFd:s:DS")) != EOF) {
      switch (ch) {
         case ('P'): {
            printableonly  = true;
            break;
         }
         case ('p'): {
            g.plaintext    = true;
            leadingspace   = true;
            break;
         }
         case ('T'): {
            g.plaintext    = true;
            leadingspace   = false;
            header         = false;
            footer         = false;
            break;
         }
         case ('o'): {
            if (freopen(optarg, "w", stdout) == NULL) {
               PRINT_ERROR(errno, "%s: reopen\n", optarg);
               goto error;
            }
            break;
         }
         case ('r'): {
            if (strtouint32(optarg, &seed, 0, UINT32_MAX, 10)) {
               PRINT_ERRORX("%s: (-r) Random seed invalid.", optarg);
                goto error;
            }
            set_seed = true;
            break;
         }
         case ('m'): {
            if (strtouint32(optarg, &lengthmin, 0, UINT32_MAX, 10)) {
               PRINT_ERRORX("%s: (-m) Min length of keys/values invalid.", optarg);
                goto error;
            }
            set_lengthmin = true;
            break;
         }
         case ('M'): {
            if (strtouint32(optarg, &lengthlimit, 1, UINT32_MAX, 10)) {
               PRINT_ERRORX("%s: (-M) Limit of key/value length invalid.", optarg);
                goto error;
            }
            set_lengthlimit = true;
            break;
         }
         case ('n'): {
            if (strtouint64(optarg, &numkeys, 0, UINT64_MAX, 10)) {
               PRINT_ERRORX("%s: (-n) Number of keys to generate invalid.", optarg);
                goto error;
            }
            set_numkeys = true;
            break;
         }
         case ('u'): {
            force_unique = false;
            break;
         }
         case ('h'): {
            header = false;
            break;
         }
         case ('H'): {
            justheader = true;
            break;
         }
         case ('f'): {
            footer = false;
            break;
         }
         case ('F'): {
            justfooter = true;
            break;
         }
         case ('d'): {
            int temp = get_delimiter(optarg);
            if (temp == EOF) {
               PRINT_ERRORX("%s: (-d) Key (or value) delimiter must be one character.",
                      optarg);
               goto error;
            }
            if (isxdigit(temp)) {
               PRINT_ERRORX("%c: (-d) Key (or value) delimiter cannot be a hex digit.",
                      temp);
               goto error;
            }
            dbt_delimiter = (char)temp;
            break;
         }
         case ('s'): {
            int temp = get_delimiter(optarg);
            if (temp == EOF) {
               PRINT_ERRORX("%s: (-s) Sorting (Between key/value pairs) delimiter must be one character.",
                      optarg);
               goto error;
            }
            if (isxdigit(temp)) {
               PRINT_ERRORX("%c: (-s) Sorting (Between key/value pairs) delimiter cannot be a hex digit.",
                      temp);
               goto error;
            }
            sort_delimiter[0] = (char)temp;
            sort_delimiter[1] = '\0';
#if TOKU_WINDOWS
            if (!strcmp(sort_delimiter, "\n")) {
                strcpy(sort_delimiter, "\r\n");
            }
#endif
            break;
         }
         case ('V'): {
            printf("%s\n", db_version(NULL, NULL, NULL));
            return EXIT_SUCCESS;
         }
         case 'D': {
	    fprintf(stderr, "Duplicates no longer supported by tokudb\n");
	    return EXIT_FAILURE;
	 }
         case 'S': {
	    fprintf(stderr, "Dupsort no longer supported by tokudb\n");
	    return EXIT_FAILURE;
	 }
         case ('?'):
         default: {
            return (usage());
         }
      }
   }
   argc -= optind;
   argv += optind;

   if (justheader && !header) {
      PRINT_ERRORX("The -h and -H options may not both be specified.\n");
      goto error;
   }
   if (justfooter && !footer) {
      PRINT_ERRORX("The -f and -F options may not both be specified.\n");
      goto error;
   }
   if (justfooter && justheader) {
      PRINT_ERRORX("The -H and -F options may not both be specified.\n");
      goto error;
   }
   if (justfooter && header) {
      PRINT_ERRORX("-F implies -h\n");
      header = false;
   }
   if (justheader && footer) {
      PRINT_ERRORX("-H implies -f\n");
      footer = false;
   }
   if (!leadingspace) {
      if (footer) {
         PRINT_ERRORX("-p implies -f\n");
         footer = false;
      }
      if (header) {
         PRINT_ERRORX("-p implies -h\n");
         header = false;
      }
   }
   if (justfooter || justheader) outputkeys = false;
   else if (!set_numkeys)
   {
      PRINT_ERRORX("Using default number of keys.  (-n 1024).\n");
      numkeys = 1024;
   }
   if (outputkeys && !set_seed) {
      PRINT_ERRORX("Using default seed.  (-r 1).\n");
      seed = 1;
   }
   if (outputkeys && !set_lengthmin) {
      PRINT_ERRORX("Using default lengthmin.  (-m 0).\n");
      lengthmin = 0;
   }
   if (outputkeys && !set_lengthlimit) {
      PRINT_ERRORX("Using default lengthlimit.  (-M 1024).\n");
      lengthlimit = 1024;
   }
   if (outputkeys && lengthmin >= lengthlimit) {
      PRINT_ERRORX("Max key size must be greater than min key size.\n");
      goto error;
   }

   if (argc != 0) {
      return usage();
   }
   if (header) {
      printf("VERSION=3\n");
      printf("format=%s\n", g.plaintext ? "print" : "bytevalue");
      printf("type=btree\n");
      // printf("db_pagesize=%d\n", 4096);  //Don't write pagesize which would be useless.
      if (dupsort)
         printf("dupsort=%d\n", dupsort);
      printf("HEADER=END\n");
   }
   if (outputkeys) generate_keys();
   if (footer)     printf("DATA=END\n");
   return EXIT_SUCCESS;

error:
   fprintf(stderr, "Quitting out due to errors.\n");
   return EXIT_FAILURE;
}

static int usage()
{
   fprintf(stderr,
           "usage: %s [-PpTuVhHfFDS] [-o output] [-r seed] [-m minsize] [-M limitsize]\n"
           "       %*s[-n numpairs] [-d delimiter] [-s delimiter]\n",
           g.progname, (int)strlen(g.progname) + 1, "");
   return EXIT_FAILURE;
}

static uint8_t randbyte(void)
{
   static uint32_t   numsavedbits   = 0;
   static uint64_t   savedbits      = 0;
   uint8_t           retval;

   if (numsavedbits < 8) {
      savedbits |= ((uint64_t)random()) << numsavedbits;
      numsavedbits += 31;  /* Random generates 31 random bits. */
   }
   retval         = savedbits & 0xff;
   numsavedbits  -= 8;
   savedbits    >>= 8;
   return retval;
}

/* Almost-uniformly random int from [0,limit) */
static int32_t random_below(int32_t limit)
{
   assert(limit > 0);
   return random() % limit;
}

static void generate_keys()
{
   bool     usedemptykey   = false;
   uint64_t  numgenerated   = 0;
   uint64_t  totalsize      = 0;
   char     identifier[24]; /* 8 bytes * 2 = 16; 16+1=17; 17+null terminator = 18. Extra padding. */
   int      length;
   int      i;
   uint8_t  ch;

   srandom(seed);
   while (numgenerated < numkeys) {
      numgenerated++;

      /* Each key is preceded by a space (unless using -T). */
      if (leadingspace) printf(" ");

      /* Generate a key. */
      {
         /* Pick a key length. */
         length = random_below(lengthlimit - lengthmin) + lengthmin;

         /* Output 'length' random bytes. */
         for (i = 0; i < length; i++) {
            do {ch = randbyte();}
               while (printableonly && !isprint(ch));
            outputbyte(ch);
         }
         totalsize += length;
        if (force_unique) {
            if (length == 0 && !usedemptykey) usedemptykey = true;
            else {
                /* Append identifier to ensure uniqueness. */
                sprintf(identifier, "x%" PRIx64, numgenerated);
                outputstring(identifier);
                totalsize += strlen(identifier);
            }
        }
      }
      printf("%c", dbt_delimiter);

      /* Each value is preceded by a space (unless using -T). */
      if (leadingspace) printf(" ");

      /* Generate a value. */
      {
         /* Pick a key length. */
         length = random_below(lengthlimit - lengthmin) + lengthmin;

         /* Output 'length' random bytes. */
         for (i = 0; i < length; i++) {
            do {ch = randbyte();}
               while (printableonly && !isprint(ch));
            outputbyte(ch);
         }
         totalsize += length;
      }
      printf("%c", dbt_delimiter);

      printf("%s", sort_delimiter);
   }
}

int get_delimiter(char* str)
{
   if (strlen(str) == 2 && str[0] == '\\') {
      switch (str[1]) {
         case ('a'): return '\a';
         case ('b'): return '\b';
#ifndef __ICL
         case ('e'): return '\e';
#endif
         case ('f'): return '\f';
         case ('n'): return '\n';
         case ('r'): return '\r';
         case ('t'): return '\t';
         case ('v'): return '\v';
         case ('0'): return '\0';
         case ('\\'): return '\\';
         default: return EOF;
      }
   }
   if (strlen(str) == 1) return str[0];
   return EOF;
}
