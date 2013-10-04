/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#if !defined(TOKUDB_COMMON_FUNCS_H)
#define TOKUDB_COMMON_FUNCS_H

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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include "tokudb_common.h"

//DB_ENV->err disabled since it does not use db_strerror
#define PRINT_ERROR(retval, ...)                                  \
do {                                                              \
if (0) g.dbenv->err(g.dbenv, retval, __VA_ARGS__);                \
else {                                                            \
   fprintf(stderr, "\tIn %s:%d %s()\n", __FILE__, __LINE__, __FUNCTION__); \
   fprintf(stderr, "%s: %s:", g.progname, db_strerror(retval));   \
   fprintf(stderr, __VA_ARGS__);                                  \
   fprintf(stderr, "\n");                                         \
   fflush(stderr);                                                \
}                                                                 \
} while (0)

//DB_ENV->err disabled since it does not use db_strerror, errx does not exist.
#define PRINT_ERRORX(...)                                               \
do {                                                              \
if (0) g.dbenv->err(g.dbenv, 0, __VA_ARGS__);                     \
else {                                                            \
   fprintf(stderr, "\tIn %s:%d %s()\n", __FILE__, __LINE__, __FUNCTION__); \
   fprintf(stderr, "%s: ", g.progname);                           \
   fprintf(stderr, __VA_ARGS__);                                  \
   fprintf(stderr, "\n");                                         \
   fflush(stderr);                                                \
}                                                                 \
} while (0)

int   strtoint32  (char* str,  int32_t* num,  int32_t min,  int32_t max, int base);
int   strtouint32 (char* str, uint32_t* num, uint32_t min, uint32_t max, int base);
int   strtoint64  (char* str,  int64_t* num,  int64_t min,  int64_t max, int base);
int   strtouint64 (char* str, uint64_t* num, uint64_t min, uint64_t max, int base);

/*
 * Convert a string to an integer of type "type".
 *
 *
 * Sets errno and returns:
 *    EINVAL: str == NULL, num == NULL, or string not of the form [ \t]*[+-]?[0-9]+
 *    ERANGE: value out of range specified. (Range of [min, max])
 *
 * *num is unchanged on error.
 * Returns:
 *
 */
#define DEF_STR_TO(name, type, bigtype, strtofunc, frmt)       \
int name(char* str, type* num, type min, type max, int base)   \
{                                                              \
   char* test;                                                 \
   bigtype value;                                              \
                                                               \
   assert(str);                                                \
   assert(num);                                                \
   assert(min <= max);                                         \
   assert(g.dbenv || g.progname);                              \
   assert(base == 0 || (base >= 2 && base <= 36));             \
                                                               \
   errno = 0;                                                  \
   while (isspace(*str)) str++;                                \
   value = strtofunc(str, &test, base);                        \
   if ((*test != '\0' && *test != '\n') || test == str) {      \
      PRINT_ERRORX("%s: Invalid numeric argument\n", str);           \
      errno = EINVAL;                                          \
      goto error;                                              \
   }                                                           \
   if (errno != 0) {                                           \
      PRINT_ERROR(errno, "%s\n", str);                               \
   }                                                           \
   if (value < min) {                                          \
      PRINT_ERRORX("%s: Less than minimum value (%" frmt ")\n", str, min); \
      goto error;                                              \
   }                                                           \
   if (value > max) {                                          \
      PRINT_ERRORX("%s: Greater than maximum value (%" frmt ")\n", str, max); \
      goto error;                                              \
   }                                                           \
   *num = value;                                               \
   return EXIT_SUCCESS;                                        \
error:                                                         \
   return errno;                                               \
}

DEF_STR_TO(strtoint32,  int32_t,  int64_t,  strtoll,  PRId32)
DEF_STR_TO(strtouint32, uint32_t, uint64_t, strtoull, PRIu32)
DEF_STR_TO(strtoint64,  int64_t,  int64_t,  strtoll,  PRId64)
DEF_STR_TO(strtouint64, uint64_t, uint64_t, strtoull, PRIu64)

static inline void
outputbyte(uint8_t ch)
{
   if (g.plaintext) {
      if (ch == '\\')         printf("\\\\");
      else if (isprint(ch))   printf("%c", ch);
      else                    printf("\\%02x", ch);
   }
   else printf("%02x", ch);
}

static inline void
outputstring(char* str)
{
   char* p;

   for (p = str; *p != '\0'; p++) {
      outputbyte((uint8_t)*p);
   }
}

static inline void
outputplaintextstring(char* str)
{
   bool old_plaintext = g.plaintext;
   g.plaintext = true;
   outputstring(str);
   g.plaintext = old_plaintext;
}

static inline int
hextoint(int ch)
{
   if (ch >= '0' && ch <= '9') {
      return ch - '0';
   }
   if (ch >= 'a' && ch <= 'z') {
      return ch - 'a' + 10;
   }
   if (ch >= 'A' && ch <= 'Z') {
      return ch - 'A' + 10;
   }
   return EOF;
}

static inline int
printabletocstring(char* inputstr, char** poutputstr)
{
   char highch;
   char lowch;
   char nextch;
   char* cstring;

   assert(inputstr);
   assert(poutputstr);
   assert(*poutputstr == NULL);

   cstring = (char*)toku_malloc((strlen(inputstr) + 1) * sizeof(char));
   if (cstring == NULL) {
      PRINT_ERROR(errno, "printabletocstring");
      goto error;
   }

   for (*poutputstr = cstring; *inputstr != '\0'; inputstr++) {
      if (*inputstr == '\\') {
         if ((highch = *++inputstr) == '\\') {
            *cstring++ = '\\';
            continue;
         }
         if (highch == '\0' || (lowch = *++inputstr) == '\0') {
            PRINT_ERROR(0, "unexpected end of input data or key/data pair");
            goto error;
         }
         if (!isxdigit(highch)) {
            PRINT_ERROR(0, "Unexpected '%c' (non-hex) input.\n", highch);
            goto error;
         }
         if (!isxdigit(lowch)) {
            PRINT_ERROR(0, "Unexpected '%c' (non-hex) input.\n", lowch);
            goto error;
         }
         nextch = (char)((hextoint(highch) << 4) | hextoint(lowch));
         if (nextch == '\0') {
            /* Database names are c strings, and cannot have extra NULL terminators. */
            PRINT_ERROR(0, "Unexpected '\\00' in input.\n");
            goto error;
         }
         *cstring++ = nextch;
      }
      else *cstring++ = *inputstr;
   }
   /* Terminate the string. */
   *cstring = '\0';
   return EXIT_SUCCESS;

error:
   PRINT_ERROR(0, "Quitting out due to errors.\n");
   return EXIT_FAILURE;
}

static inline int
verify_library_version(void)
{
   int major;
   int minor;
   
   db_version(&major, &minor, NULL);
   if (major != DB_VERSION_MAJOR || minor != DB_VERSION_MINOR) {
      PRINT_ERRORX("version %d.%d doesn't match library version %d.%d\n",
             DB_VERSION_MAJOR, DB_VERSION_MINOR, major, minor);
      return EXIT_FAILURE;
   }
   return EXIT_SUCCESS;
}

static int last_caught = 0;

static void catch_signal(int which_signal) {
    last_caught = which_signal;
    if (last_caught == 0) last_caught = SIGINT;
}

static inline void
init_catch_signals(void) {
    signal(SIGINT, catch_signal);
    signal(SIGTERM, catch_signal);
#ifdef SIGHUP
    signal(SIGHUP, catch_signal);
#endif
#ifdef SIGPIPE
    signal(SIGPIPE, catch_signal);
#endif
}

static inline int
caught_any_signals(void) {
    return last_caught != 0;
}

static inline void
resend_signals(void) {
    if (last_caught) {
        signal(last_caught, SIG_DFL);
        raise(last_caught);
    }
}

#include <memory.h>
#if IS_TDB && TOKU_WINDOWS
#include <src/ydb.h>
#endif
static int test_main (int argc, char *const argv[]);
int
main(int argc, char *const argv[]) {
    int r;
#if IS_TDB && TOKU_WINDOWS
    toku_ydb_init();
#endif
#if !IS_TDB && DB_VERSION_MINOR==4 && DB_VERSION_MINOR == 7
    r = db_env_set_func_malloc(toku_malloc);   assert(r==0);
    r = db_env_set_func_free(toku_free);      assert(r==0);
    r = db_env_set_func_realloc(toku_realloc);   assert(r==0);
#endif
    r = test_main(argc, argv);
#if IS_TDB && TOKU_WINDOWS
    toku_ydb_destroy();
#endif
    return r;
}

#endif /* #if !defined(TOKUDB_COMMON_H) */
