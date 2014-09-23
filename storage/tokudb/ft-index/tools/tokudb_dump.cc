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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#include <db.h>

#include <toku_stdlib.h>
#include <toku_stdint.h>
#include <toku_portability.h>
#include <toku_assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <memory.h>

typedef struct {
   bool     leadingspace;
   bool     plaintext;
   bool     header;
   bool     footer;
   bool     is_private;
   bool     recovery_and_txn;
   char*    progname;
   char*    homedir;
   char*    database;
   char*    subdatabase;
   int      exitcode;
   int      recover_flags;
   DBTYPE   dbtype;
   DBTYPE   opened_dbtype;
   DB*      db;
   DB_ENV*  dbenv;
} dump_globals;

dump_globals g;

#define SET_BITS(bitvector, bits)      ((bitvector) |= (bits))
#define REMOVE_BITS(bitvector, bits)   ((bitvector) &= ~(bits))
#define IS_SET_ANY(bitvector, bits)    ((bitvector) & (bits))
#define IS_SET_ALL(bitvector, bits)    (((bitvector) & (bits)) == (bits))

#define IS_POWER_OF_2(num)             ((num) > 0 && ((num) & ((num) - 1)) == 0)

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

static int   usage          (void);
static int   create_init_env(void);
static int   dump_database  (void);
static int   open_database  (void);
static int   dump_pairs     (void);
static int   dump_footer    (void);
static int   dump_header    (void);
static int   close_database (void);

int main(int argc, char *const argv[]) {
   int ch;
   int retval;

   /* Set up the globals. */
   memset(&g, 0, sizeof(g));
   g.leadingspace   = true;
   //TODO: Uncomment when DB_UNKNOWN + db->get_type are implemented.
   g.dbtype         = DB_UNKNOWN;
   //g.dbtype         = DB_BTREE;
   g.progname       = argv[0];
   g.header         = true;
   g.footer         = true;
   g.recovery_and_txn = true;

   if (verify_library_version() != 0) goto error;

   while ((ch = getopt(argc, argv, "d:f:h:klNP:ps:RrVTx")) != EOF) {
      switch (ch) {
         case ('d'): {
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('f'): {
            if (freopen(optarg, "w", stdout) == NULL) {
               fprintf(stderr,
                       "%s: %s: reopen: %s\n",
                       g.progname, optarg, strerror(errno));
               goto error;
            }
            break;
         }
         case ('h'): {
            g.homedir = optarg;
            break;
         }
         case ('k'): {
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('l'): {
            //TODO: Implement (Requires master database support)
            PRINT_ERRORX("-%c option not supported.\n", ch); //YET!
            goto error;
         }
         case ('N'): {
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('P'): {
            /* Clear password. */
            memset(optarg, 0, strlen(optarg));
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('p'): {
            g.plaintext = true;
            break;
         }
         case ('R'): {
            //TODO: Uncomment when DB_SALVAGE,DB_AGGRESSIVE are implemented.
            /*g.recover_flags |= DB_SALVAGE | DB_AGGRESSIVE;*/

            //TODO: Implement aggressive recovery (requires db->verify())
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('r'): {
            //TODO: Uncomment when DB_SALVAGE,DB_AGGRESSIVE are implemented.
            /*g.recover_flags |= DB_SALVAGE;*/

            //TODO: Implement recovery (requires db->verify())
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('s'): {
            g.subdatabase = optarg;
            break;
         }
         case ('V'): {
            printf("%s\n", db_version(NULL, NULL, NULL));
            goto cleanup;
         }
         case ('T'): {
            g.plaintext    = true;
            g.leadingspace = false;
            g.header       = false;
            g.footer       = false;
            break;
         }
         case ('x'): {
	    g.recovery_and_txn = false;
	    break;
	 }
         case ('?'):
         default: {
            g.exitcode = usage();
            goto cleanup;
         }
      }
   }
   argc -= optind;
   argv += optind;

   //TODO: Uncomment when DB_SALVAGE,DB_AGGRESSIVE,DB_PRINTABLE,db->verify are implemented.
   /*
   if (g.plaintext) g.recover_flags |= DB_PRINTABLE;

   if (g.subdatabase != NULL && IS_SET_ALL(g.recover_flags, DB_SALVAGE)) {
      if (IS_SET_ALL(g.recover_flags, DB_AGGRESSIVE)) {
         PRINT_ERRORX("The -s and -R options may not both be specified.\n");
         goto error;
      }
      PRINT_ERRORX("The -s and -r options may not both be specified.\n");
      goto error;
      
   }
   */

   if (argc != 1) {
      g.exitcode = usage();
      goto cleanup;
   }

   init_catch_signals();
   
   g.database = argv[0];
   if (caught_any_signals()) goto cleanup;
   if (create_init_env() != 0) goto error;
   if (caught_any_signals()) goto cleanup;
   if (dump_database() != 0) goto error;
   if (false) {
error:
      g.exitcode = EXIT_FAILURE;
      fprintf(stderr, "%s: Quitting out due to errors.\n", g.progname);
   }
cleanup:
   if (g.dbenv && (retval = g.dbenv->close(g.dbenv, 0)) != 0) {
      g.exitcode = EXIT_FAILURE;
      fprintf(stderr, "%s: %s: dbenv->close\n", g.progname, db_strerror(retval));
   }
   //   if (g.subdatabase)      free(g.subdatabase);
   resend_signals();

   return g.exitcode;
}

int dump_database()
{
   int retval;

   /* Create a database handle. */
   retval = db_create(&g.db, g.dbenv, 0);
   if (retval != 0) {
      PRINT_ERROR(retval, "db_create");
      return EXIT_FAILURE;
   }

   /*
   TODO: If/when supporting encryption
   if (g.password && (retval = db->set_flags(db, DB_ENCRYPT))) {
      PRINT_ERROR(ret, "DB->set_flags: DB_ENCRYPT");
      goto error;
   }
   */
   if (open_database() != 0) goto error;
   if (caught_any_signals()) goto cleanup;
   if (g.header && dump_header() != 0) goto error;
   if (caught_any_signals()) goto cleanup;
   if (dump_pairs() != 0) goto error;
   if (caught_any_signals()) goto cleanup;
   if (g.footer && dump_footer() != 0) goto error;

   if (false) {
error:
      g.exitcode = EXIT_FAILURE;
   }
cleanup:

   if (close_database() != 0) g.exitcode = EXIT_FAILURE;

   return g.exitcode;
}

int usage()
{
   fprintf(stderr,
           "usage: %s [-pVT] [-x] [-f output] [-h home] [-s database] db_file\n",
           g.progname);
   return EXIT_FAILURE;
}

int create_init_env()
{
   int retval;
   DB_ENV* dbenv;
   int flags;
   //TODO: Experiments to determine right cache size for tokudb, or maybe command line argument.

   retval = db_env_create(&dbenv, 0);
   if (retval) {
      fprintf(stderr, "%s: db_dbenv_create: %s\n", g.progname, db_strerror(retval));
      goto error;
   }
   ///TODO: UNCOMMENT/IMPLEMENT dbenv->set_errfile(dbenv, stderr);
   dbenv->set_errpfx(dbenv, g.progname);
   /*
   TODO: Anything for encryption?
   */

   /* Open the dbenvironment. */
   g.is_private = false;
   //flags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_USE_ENVIRON;
   flags = DB_INIT_LOCK | DB_INIT_MPOOL; ///TODO: UNCOMMENT/IMPLEMENT | DB_USE_ENVIRON;
   if (g.recovery_and_txn) {
      SET_BITS(flags, DB_INIT_LOG | DB_INIT_TXN | DB_RECOVER);
   }
   
   /*
   ///TODO: UNCOMMENT/IMPLEMENT  Notes:  We require DB_PRIVATE
   if (!dbenv->open(dbenv, g.homedir, flags, 0)) goto success;
   */

   /*
   ///TODO: UNCOMMENT/IMPLEMENT 
   retval = dbenv->set_cachesize(dbenv, 0, cache, 1);
   if (retval) {
      PRINT_ERROR(retval, "DB_ENV->set_cachesize");
      goto error;
   }
   */
   g.is_private = true;
   //TODO: Do we want to support transactions even in single-process mode?
   //Logging is not necessary.. this is read-only.
   //However, do we need to use DB_INIT_LOG to join a logging environment?
   //REMOVE_BITS(flags, DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN);
   SET_BITS(flags, DB_CREATE | DB_PRIVATE);

   retval = dbenv->open(dbenv, g.homedir, flags, 0);
   if (retval) {
      PRINT_ERROR(retval, "DB_ENV->open");
      goto error;
   }
   g.dbenv = dbenv;
   return EXIT_SUCCESS;

error:
   return EXIT_FAILURE;
}

#define DUMP_FLAG(bit, dump)    if (IS_SET_ALL(flags, bit)) printf(dump);

#define DUMP_IGNORED_FLAG(bit, dump)


int dump_header()
{
   uint32_t flags;
   int retval;
   DB* db = g.db;
   
   assert(g.header);
   printf("VERSION=3\n");
   printf("format=%s\n", g.plaintext ? "print" : "bytevalue");
   //TODO: Uncomment when DB_UNKNOWN + db->get_type are implemented.
   /*assert(g.dbtype == DB_BTREE || (g.dbtype == DB_UNKNOWN && g.opened_dbtype == DB_BTREE));*/
   printf("type=btree\n");
   //TODO: Get page size from db.  Currently tokudb does not support db->get_pagesize.
   //Don't print this out //printf("db_pagesize=4096\n");
   if (g.subdatabase) {
      printf("subdatabase=");
      outputplaintextstring(g.subdatabase);
      printf("\n");      
   }
   //TODO: Uncomment when db->get_flags is implemented
   if ((retval = db->get_flags(db, &flags)) != 0) {
      PRINT_ERROR(retval, "DB->get_flags");
      goto error;
   }
   DUMP_IGNORED_FLAG(DB_CHKSUM,    "chksum=1\n");
   DUMP_IGNORED_FLAG(DB_RECNUM,    "recnum=1\n");
   printf("HEADER=END\n");
   
   if (ferror(stdout)) goto error;
   return EXIT_SUCCESS;

error:
   return EXIT_FAILURE;
}

int dump_footer()
{
   printf("DATA=END\n");
   if (ferror(stdout)) goto error;

   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
}

int open_database()
{
   DB* db = g.db;
   int retval;

   int open_flags = 0;//|DB_RDONLY;
   //TODO: Transaction auto commit stuff
   SET_BITS(open_flags, DB_AUTO_COMMIT);

   retval = db->open(db, NULL, g.database, g.subdatabase, g.dbtype, open_flags, 0666);
   if (retval != 0) {
      PRINT_ERROR(retval, "DB->open: %s", g.database);
      goto error;
   }
   //TODO: Uncomment when DB_UNKNOWN + db->get_type are implemented.
   /*
   retval = db->get_type(db, &g.opened_dbtype);
   if (retval != 0) {
      PRINT_ERROR(retval, "DB->get_type");
      goto error;
   }
   if (g.opened_dbtype != DB_BTREE) {
      PRINT_ERRORX("Unsupported db type %d\n", g.opened_dbtype);
      goto error;
   }
   if (g.dbtype != DB_UNKNOWN && g.opened_dbtype != g.dbtype) {
      PRINT_ERRORX("DBTYPE %d does not match opened DBTYPE %d.\n", g.dbtype, g.opened_dbtype);
      goto error;
   }*/
   return EXIT_SUCCESS;
error:
   fprintf(stderr, "Quitting out due to errors.\n");
   return EXIT_FAILURE;
}

static int dump_dbt(DBT* dbt)
{
   char* str;
   uint32_t idx;
   
   assert(dbt);
   str = (char*)dbt->data;
   if (g.leadingspace) printf(" ");
   if (dbt->size > 0) {
      assert(dbt->data);
      for (idx = 0; idx < dbt->size; idx++) {
         outputbyte(str[idx]);
         if (ferror(stdout)) {
            perror("stdout");
            goto error;
         }
      }
   }
   printf("\n");
   if (false) {
error:
      g.exitcode = EXIT_FAILURE;
   }
   return g.exitcode;
}

int dump_pairs()
{
   int retval;
   DBT key;
   DBT data;
   DB* db = g.db;
   DBC* dbc = NULL;

   memset(&key, 0, sizeof(key));
   memset(&data, 0, sizeof(data));

   DB_TXN* txn = NULL;
   if (g.recovery_and_txn) {
      retval = g.dbenv->txn_begin(g.dbenv, NULL, &txn, 0);
      if (retval) {
	 PRINT_ERROR(retval, "DB_ENV->txn_begin");
	 goto error;
      }
   }

   if ((retval = db->cursor(db, txn, &dbc, 0)) != 0) {
      PRINT_ERROR(retval, "DB->cursor");
      goto error;
   }
   while ((retval = dbc->c_get(dbc, &key, &data, DB_NEXT)) == 0) {
      if (caught_any_signals()) goto cleanup;
      if (dump_dbt(&key) != 0) goto error;
      if (dump_dbt(&data) != 0) goto error;
   }
   if (retval != DB_NOTFOUND) {
      PRINT_ERROR(retval, "DBC->c_get");
      goto error;
   }
   
   
   if (false) {
error:
      g.exitcode = EXIT_FAILURE;
   }
cleanup:
   if (dbc && (retval = dbc->c_close(dbc)) != 0) {
      PRINT_ERROR(retval, "DBC->c_close");
      g.exitcode = EXIT_FAILURE;
   }
   if (txn) {
       if (retval) {
           int r2 = txn->abort(txn);
           if (r2) PRINT_ERROR(r2, "DB_TXN->abort");
       }
       else {
           retval = txn->commit(txn, 0);
           if (retval) PRINT_ERROR(retval, "DB_TXN->abort");
       }
   }
   return g.exitcode;
}

int close_database()
{
   DB* db = g.db;
   int retval;

   assert(db);
   if ((retval = db->close(db, 0)) != 0) {
      PRINT_ERROR(retval, "DB->close");
      goto error;
   }
   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
}
