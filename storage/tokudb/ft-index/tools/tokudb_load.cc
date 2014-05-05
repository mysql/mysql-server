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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <db.h>
#include "tokudb_common.h"

typedef struct {
   bool     leadingspace;
   bool     plaintext;
   bool     overwritekeys;
   bool     header;
   bool     eof;
   bool     keys;
   bool     is_private;
   char*    progname;
   char*    homedir;
   char*    database;
   char*    subdatabase;
   char**   config_options;
   int32_t  version;
   int      exitcode;
   uint64_t linenumber;
   DBTYPE   dbtype;
   DB*      db;
   DB_ENV*  dbenv;
   struct {
      char* data[2];
   }        get_dbt;
   struct {
      char* data;
   }        read_header;
} load_globals;

load_globals g;
#include "tokudb_common_funcs.h"

static int   usage          (void);
static int   load_database  (void);
static int   create_init_env(void);
static int   read_header    (void);
static int   open_database  (void);
static int   read_keys      (void);
static int   apply_commandline_options(void);
static int   close_database (void);
static int   doublechararray(char** pmem, uint64_t* size);

int test_main(int argc, char *const argv[]) {
   int ch;
   int retval;
   char** next_config_option;

   /* Set up the globals. */
   memset(&g, 0, sizeof(g));
   g.leadingspace   = true;
   g.overwritekeys  = true;
   g.dbtype         = DB_UNKNOWN;
   //g.dbtype         = DB_BTREE;
   g.progname       = argv[0];
   g.header         = true;
   
   if (verify_library_version() != 0) goto error;

   next_config_option = g.config_options = (char**) calloc(argc, sizeof(char*));
   if (next_config_option == NULL) {
      PRINT_ERROR(errno, "main: calloc\n");
      goto error;
   }
   while ((ch = getopt(argc, argv, "c:f:h:nP:r:Tt:V")) != EOF) {
      switch (ch) {
         case ('c'): {
            *next_config_option++ = optarg;
            break;
         }
         case ('f'): {
            if (freopen(optarg, "r", stdin) == NULL) {
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
         case ('n'): {
            /* g.overwritekeys = false; */
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('P'): {
            /* Clear password. */
            memset(optarg, 0, strlen(optarg));
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('r'): {
            PRINT_ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('T'): {
            g.plaintext    = true;
            g.leadingspace = false;
            g.header       = false;
            break;
         }
         case ('t'): {
            if (!strcmp(optarg, "btree")) {
               g.dbtype = DB_BTREE;
               break;
            }
            if (!strcmp(optarg, "hash") || !strcmp(optarg, "recno") || !strcmp(optarg, "queue")) {
               fprintf(stderr, "%s: db type %s not supported.\n", g.progname, optarg);
               goto error;
            }
            fprintf(stderr, "%s: Unrecognized db type %s.\n", g.progname, optarg);
            goto error;
         }
         case ('V'): {
            printf("%s\n", db_version(NULL, NULL, NULL));
            goto cleanup;
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

   if (argc != 1) {
      g.exitcode = usage();
      goto cleanup;
   }
   init_catch_signals();

   g.database = argv[0];
   if (create_init_env() != 0) goto error;
   if (caught_any_signals()) goto cleanup;
   while (!g.eof) {
      if (load_database() != 0) goto error;
      if (caught_any_signals()) goto cleanup;
   }
   if (false) {
error:
      g.exitcode = EXIT_FAILURE;
      fprintf(stderr, "%s: Quitting out due to errors.\n", g.progname);
   }
cleanup:
   if (g.dbenv && (retval = g.dbenv->close(g.dbenv, 0)) != 0) {
      g.exitcode = EXIT_FAILURE;
      fprintf(stderr, "%s: dbenv->close: %s\n", g.progname, db_strerror(retval));
   }
   if (g.config_options)   toku_free(g.config_options);
   if (g.subdatabase)      toku_free(g.subdatabase);
   if (g.read_header.data) toku_free(g.read_header.data);
   if (g.get_dbt.data[0])  toku_free(g.get_dbt.data[0]);
   if (g.get_dbt.data[1])  toku_free(g.get_dbt.data[1]);
   resend_signals();

   return g.exitcode;
}

int load_database()
{
   int retval;

   /* Create a database handle. */
   retval = db_create(&g.db, g.dbenv, 0);
   if (retval != 0) {
      PRINT_ERROR(retval, "db_create");
      return EXIT_FAILURE;
   }

   if (g.header && read_header() != 0) goto error;
   if (g.eof) goto cleanup;
   if (caught_any_signals()) goto cleanup;
   if (apply_commandline_options() != 0) goto error;
   if (g.eof) goto cleanup;
   if (caught_any_signals()) goto cleanup;

   /*
   TODO: If/when supporting encryption
   if (g.password && (retval = db->set_flags(db, DB_ENCRYPT))) {
      PRINT_ERROR(ret, "DB->set_flags: DB_ENCRYPT");
      goto error;
   }
   */
   if (open_database() != 0) goto error;
   if (g.eof) goto cleanup;
   if (caught_any_signals()) goto cleanup;
   if (read_keys() != 0) goto error;
   if (g.eof) goto cleanup;
   if (caught_any_signals()) goto cleanup;

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
           "usage: %s [-TV] [-c name=value] [-f file] [-h home] [-t btree] db_file\n",
           g.progname);
   return EXIT_FAILURE;
}

int create_init_env()
{
   int retval;
   DB_ENV* dbenv;
   int flags;
   //TODO: Experiments to determine right cache size for tokudb, or maybe command line argument.
   //int cache = 1 << 20; /* 1 megabyte */

   retval = db_env_create(&dbenv, 0);
   if (retval) {
      fprintf(stderr, "%s: db_dbenv_create: %s\n", g.progname, db_strerror(retval));
      goto error;
   }
   ///TODO: UNCOMMENT/IMPLEMENT dbenv->set_errfile(dbenv, stderr);
   dbenv->set_errpfx(dbenv, g.progname);
   /*
   TODO: If/when supporting encryption
   if (g.password && (retval = dbenv->set_encrypt(dbenv, g.password, DB_ENCRYPT_AES))) {
      PRINT_ERROR(retval, "set_passwd");
      goto error;
   }
   */

   /* Open the dbenvironment. */
   g.is_private = false;
   flags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOG; ///TODO: UNCOMMENT/IMPLEMENT | DB_USE_ENVIRON;
   //TODO: Transactions.. SET_BITS(flags, DB_INIT_TXN);
   
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
   //TODO: Do we want to support transactions/logging even in single-process mode?
   //Maybe if the db already exists.
   //If db does not exist.. makes sense not to log or have transactions
   //REMOVE_BITS(flags, DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN);
   SET_BITS(flags, DB_CREATE | DB_PRIVATE);

   retval = dbenv->open(dbenv, g.homedir ? g.homedir : ".", flags, 0);
   if (retval) {
      PRINT_ERROR(retval, "DB_ENV->open");
      goto error;
   }
   g.dbenv = dbenv;
   return EXIT_SUCCESS;

error:
   return EXIT_FAILURE;
}

#define PARSE_NUMBER(match, dbfunction)                                    \
if (!strcmp(field, match)) {                                               \
   if (strtoint32(value, &num, 1, INT32_MAX, 10)) goto error;              \
   if ((retval = dbfunction(db, num)) != 0) goto printerror;               \
   continue;                                                               \
}
#define PARSE_UNSUPPORTEDNUMBER(match, dbfunction)                         \
if (!strcmp(field, match)) {                                               \
   if (strtoint32(value, &num, 1, INT32_MAX, 10)) goto error;              \
   PRINT_ERRORX("%s option not supported.\n", field);                            \
   goto error;                                                             \
}
#define PARSE_IGNOREDNUMBER(match, dbfunction)                             \
if (!strcmp(field, match)) {                                               \
   if (strtoint32(value, &num, 1, INT32_MAX, 10)) goto error;              \
   PRINT_ERRORX("%s option not supported yet (ignored).\n", field);              \
   continue;                                                               \
}

#define PARSE_FLAG(match, flag)                          \
if (!strcmp(field, match)) {                             \
   if (strtoint32(value, &num, 0, 1, 10)) {              \
      PRINT_ERRORX("%s: boolean name=value pairs require a value of 0 or 1",  \
             field);                                     \
      goto error;                                        \
   }                                                     \
   if ((retval = db->set_flags(db, flag)) != 0) {        \
      PRINT_ERROR(retval, "set_flags: %s", field);             \
      goto error;                                        \
   }                                                     \
   continue;                                             \
}

#define PARSE_UNSUPPORTEDFLAG(match, flag)               \
if (!strcmp(field, match)) {                             \
   if (strtoint32(value, &num, 0, 1, 10)) {              \
      PRINT_ERRORX("%s: boolean name=value pairs require a value of 0 or 1",  \
             field);                                     \
      goto error;                                        \
   }                                                     \
   PRINT_ERRORX("%s option not supported.\n", field);          \
   goto error;                                           \
}

#define PARSE_IGNOREDFLAG(match, flag)                   \
if (!strcmp(field, match)) {                             \
   if (strtoint32(value, &num, 0, 1, 10)) {              \
      PRINT_ERRORX("%s: boolean name=value pairs require a value of 0 or 1",  \
             field);                                     \
      goto error;                                        \
   }                                                     \
   PRINT_ERRORX("%s option not supported yet (ignored).\n", field);  \
   continue;                                             \
}

#define PARSE_CHAR(match, dbfunction)                    \
if (!strcmp(field, match)) {                             \
   if (strlen(value) != 1) {                             \
      PRINT_ERRORX("%s=%s: Expected 1-byte value",             \
             field, value);                              \
      goto error;                                        \
   }                                                     \
   if ((retval = dbfunction(db, value[0])) != 0) {       \
      goto printerror;                                   \
   }                                                     \
   continue;                                             \
}

#define PARSE_UNSUPPORTEDCHAR(match, dbfunction)         \
if (!strcmp(field, match)) {                             \
   if (strlen(value) != 1) {                             \
      PRINT_ERRORX("%s=%s: Expected 1-byte value",             \
             field, value);                              \
      goto error;                                        \
   }                                                     \
   PRINT_ERRORX("%s option not supported.\n", field);          \
   goto error;                                           \
}

#define PARSE_COMMON_CONFIGURATIONS()  \
      PARSE_IGNOREDNUMBER(    "bt_minkey",   db->set_bt_minkey);     \
      PARSE_IGNOREDFLAG(      "chksum",      DB_CHKSUM);             \
      PARSE_IGNOREDNUMBER(    "db_lorder",   db->set_lorder);        \
      PARSE_IGNOREDNUMBER(    "db_pagesize", db->set_pagesize);      \
      PARSE_UNSUPPORTEDNUMBER("extentsize",  db->set_q_extentsize);  \
      PARSE_UNSUPPORTEDNUMBER("h_ffactor",   db->set_h_ffactor);     \
      PARSE_UNSUPPORTEDNUMBER("h_nelem",     db->set_h_nelem);       \
      PARSE_UNSUPPORTEDNUMBER("re_len",      db->set_re_len);        \
      PARSE_UNSUPPORTEDCHAR(  "re_pad",      db->set_re_pad);        \
      PARSE_UNSUPPORTEDFLAG(  "recnum",      DB_RECNUM);             \
      PARSE_UNSUPPORTEDFLAG(  "renumber",    DB_RENUMBER);



int read_header()
{
   static uint64_t datasize = 1 << 10;
   uint64_t idx = 0;
   char* field;
   char* value;
   int ch;
   int32_t num;
   int retval;
   int r;

   assert(g.header);

   if (g.read_header.data == NULL && (g.read_header.data = (char*)toku_malloc(datasize * sizeof(char))) == NULL) {
      PRINT_ERROR(errno, "read_header: malloc");
      goto error;
   }
   while (!g.eof) {
      if (caught_any_signals()) goto success;    
      g.linenumber++;
      idx = 0;
      /* Read a line. */
      while (true) {
         if ((ch = getchar()) == EOF) {
            g.eof = true;
            if (ferror(stdin)) goto formaterror;
            break;
         }
         if (ch == '\n') break;

         g.read_header.data[idx] = (char)ch;
         idx++;

         /* Ensure room exists for next character/null terminator. */
         if (idx == datasize && doublechararray(&g.read_header.data, &datasize)) goto error;
      }
      if (idx == 0 && g.eof) goto success;
      g.read_header.data[idx] = '\0';

      field = g.read_header.data;
      if ((value = strchr(g.read_header.data, '=')) == NULL) goto formaterror;
      value[0] = '\0';
      value++;

      if (field[0] == '\0' || value[0] == '\0') goto formaterror;

      if (!strcmp(field, "HEADER")) break;
      if (!strcmp(field, "VERSION")) {
         if (strtoint32(value, &g.version, 1, INT32_MAX, 10)) goto error;
         if (g.version != 3) {
            PRINT_ERRORX("line %" PRIu64 ": VERSION %d is unsupported", g.linenumber, g.version);
            goto error;
         }
         continue;
      }
      if (!strcmp(field, "format")) {
         if (!strcmp(value, "bytevalue")) {
            g.plaintext = false;
            continue;
         }
         if (!strcmp(value, "print")) {
            g.plaintext = true;
            continue;
         }
         goto formaterror;
      }
      if (!strcmp(field, "type")) {
         if (!strcmp(value, "btree")) {
            g.dbtype = DB_BTREE;
            continue;
         }
         if (!strcmp(value, "hash") || strcmp(value, "recno") || strcmp(value, "queue")) {
            PRINT_ERRORX("db type %s not supported.\n", value);
            goto error;
         }
         PRINT_ERRORX("line %" PRIu64 ": unknown type %s", g.linenumber, value);
         goto error;
      }
      if (!strcmp(field, "database") || !strcmp(field, "subdatabase")) {
         if (g.subdatabase != NULL) {
            toku_free(g.subdatabase);
            g.subdatabase = NULL;
         }
         if ((retval = printabletocstring(value, &g.subdatabase))) {
            PRINT_ERROR(retval, "error reading db name");
            goto error;
         }
         continue;
      }
      if (!strcmp(field, "keys")) {
         int32_t temp;
         if (strtoint32(value, &temp, 0, 1, 10)) {
            PRINT_ERROR(0,
                     "%s: boolean name=value pairs require a value of 0 or 1",
                     field);
            goto error;
         }
         g.keys = (bool)temp;
         if (!g.keys) {
            PRINT_ERRORX("keys=0 not supported");
            goto error;
         }
         continue;
      }
      PARSE_COMMON_CONFIGURATIONS();

      PRINT_ERRORX("unknown input-file header configuration keyword \"%s\"", field);
      goto error;
   }
success:
   r = 0;

   if (false) {
formaterror:
      r = EXIT_FAILURE;
      PRINT_ERRORX("line %" PRIu64 ": unexpected format", g.linenumber);
   }
   if (false) {
error:
      r = EXIT_FAILURE;
   }
   return r;
}

int apply_commandline_options()
{
   int r = -1;
   unsigned idx;
   char* field;
   char* value = NULL;
   int32_t num;
   int retval;

   for (idx = 0; g.config_options[idx]; idx++) {
      if (value) {
         /* Restore the field=value format. */
         value[-1] = '=';
         value = NULL;
      }
      field = g.config_options[idx];

      if ((value = strchr(field, '=')) == NULL) {
         PRINT_ERRORX("command-line configuration uses name=value format");
         goto error;
      }
      value[0] = '\0';
      value++;

      if (field[0] == '\0' || value[0] == '\0') {
         PRINT_ERRORX("command-line configuration uses name=value format");
         goto error;
      }

      if (!strcmp(field, "database") || !strcmp(field, "subdatabase")) {
         if (g.subdatabase != NULL) {
            toku_free(g.subdatabase);
            g.subdatabase = NULL;
         }
         if ((retval = printabletocstring(value, &g.subdatabase))) {
            PRINT_ERROR(retval, "error reading db name");
            goto error;
         }
         continue;
      }
      if (!strcmp(field, "keys")) {
         int32_t temp;
         if (strtoint32(value, &temp, 0, 1, 10)) {
            PRINT_ERROR(0,
                     "%s: boolean name=value pairs require a value of 0 or 1",
                     field);
            goto error;
         }
         g.keys = (bool)temp;
         if (!g.keys) {
            PRINT_ERRORX("keys=0 not supported");
            goto error;
         }
         continue;
      }
      PARSE_COMMON_CONFIGURATIONS();

      PRINT_ERRORX("unknown input-file header configuration keyword \"%s\"", field);
      goto error;
   }
   if (value) {
      /* Restore the field=value format. */
      value[-1] = '=';
      value = NULL;
   }
   r = 0;

error:
   return r;
}

int open_database()
{
   DB* db = g.db;
   int retval;

   int open_flags = 0;
   //TODO: Transaction auto commit stuff
   //if (TXN_ON(dbenv)) SET_BITS(open_flags, DB_AUTO_COMMIT);

   //Try to see if it exists first.
   retval = db->open(db, NULL, g.database, g.subdatabase, g.dbtype, open_flags, 0666);
   if (retval == ENOENT) {
      //Does not exist and we did not specify a type.
      //TODO: Uncomment when DB_UNKNOWN + db->get_type are implemented.
      /*
      if (g.dbtype == DB_UNKNOWN) {
         PRINT_ERRORX("no database type specified");
         goto error;
      }*/
      SET_BITS(open_flags, DB_CREATE);
      //Try creating it.
      retval = db->open(db, NULL, g.database, g.subdatabase, g.dbtype, open_flags, 0666);
   }
   if (retval != 0) {
      PRINT_ERROR(retval, "DB->open: %s", g.database);
      goto error;
   }
   //TODO: Uncomment when DB_UNKNOWN + db->get_type are implemented.
   /*
   if ((retval = db->get_type(db, &opened_type)) != 0) {
      PRINT_ERROR(retval, "DB->get_type");
      goto error;
   }
   if (opened_type != DB_BTREE) {
      PRINT_ERRORX("Unsupported db type %d\n", opened_type);
      goto error;
   }
   if (g.dbtype != DB_UNKNOWN && opened_type != g.dbtype) {
      PRINT_ERRORX("DBTYPE %d does not match opened DBTYPE %d.\n", g.dbtype, opened_type);
      goto error;
   }*/
   return EXIT_SUCCESS;
error:
   fprintf(stderr, "Quitting out due to errors.\n");
   return EXIT_FAILURE;
}

int doublechararray(char** pmem, uint64_t* size)
{
   assert(pmem);
   assert(size);
   assert(IS_POWER_OF_2(*size));

   *size <<= 1;
   if (*size == 0) {
      /* Overflowed uint64_t. */
      PRINT_ERRORX("Line %" PRIu64 ": Line too long.\n", g.linenumber);
      goto error;
   }
   if ((*pmem = (char*)toku_realloc(*pmem, *size)) == NULL) {
      PRINT_ERROR(errno, "doublechararray: realloc");
      goto error;
   }
   return EXIT_SUCCESS;

error:
   return EXIT_FAILURE;
}

static int get_dbt(DBT* pdbt)
{
   /* Need to store a key and value. */
   static uint64_t datasize[2] = {1 << 10, 1 << 10};
   static int which = 0;
   char* datum;
   uint64_t idx = 0;
   int highch;
   int lowch;

   /* *pdbt should have been memset to 0 before being called. */
   which = 1 - which;
   if (g.get_dbt.data[which] == NULL &&
      (g.get_dbt.data[which] = (char*)toku_malloc(datasize[which] * sizeof(char))) == NULL) {
      PRINT_ERROR(errno, "get_dbt: malloc");
      goto error;
   }
   
   datum = g.get_dbt.data[which];

   if (g.plaintext) {
      int firstch;
      int nextch = EOF;

      for (firstch = getchar(); firstch != EOF; firstch = getchar()) {
         switch (firstch) {
            case ('\n'): {
               /* Done reading this key/value. */
               nextch = EOF;
               break;
            }
            case ('\\'): {
               /* Escaped \ or two hex digits. */
               highch = getchar();
               if (highch == '\\') {
                  nextch = '\\';
                  break;
               }
               else if (highch == EOF) {
                  g.eof = true;
                  PRINT_ERRORX("Line %" PRIu64 ": Unexpected end of file (2 hex digits per byte).\n", g.linenumber);
                  goto error;
               }
               else if (!isxdigit(highch)) {
                  PRINT_ERRORX("Line %" PRIu64 ": Unexpected '%c' (non-hex) input.\n", g.linenumber, highch);
                  goto error;
               }

               lowch = getchar();
               if (lowch == EOF) {
                  g.eof = true;
                  PRINT_ERRORX("Line %" PRIu64 ": Unexpected end of file (2 hex digits per byte).\n", g.linenumber);
                  goto error;
               }
               else if (!isxdigit(lowch)) {
                  PRINT_ERRORX("Line %" PRIu64 ": Unexpected '%c' (non-hex) input.\n", g.linenumber, lowch);
                  goto error;
               }

               nextch = (hextoint(highch) << 4) | hextoint(lowch);
               break;
            }
            default: {
               if (isprint(firstch)) {
                  nextch = firstch;
                  break;
               }
               PRINT_ERRORX("Line %" PRIu64 ": Nonprintable character found.", g.linenumber);
               goto error;
            }
         }
         if (nextch == EOF) {
            break;
         }
         if (idx == datasize[which]) {
            /* Overflow, double the memory. */
            if (doublechararray(&g.get_dbt.data[which], &datasize[which])) goto error;
            datum = g.get_dbt.data[which];
         }
         datum[idx] = (char)nextch;
         idx++;
      }
      if (firstch == EOF) g.eof = true;
   }
   else {
      for (highch = getchar(); highch != EOF; highch = getchar()) {
         if (highch == '\n') {
            /* Done reading this key/value. */
            break;
         }

         lowch = getchar();
         if (lowch == EOF) {
            g.eof = true;
            PRINT_ERRORX("Line %" PRIu64 ": Unexpected end of file (2 hex digits per byte).\n", g.linenumber);
            goto error;
         }
         if (!isxdigit(highch)) {
            PRINT_ERRORX("Line %" PRIu64 ": Unexpected '%c' (non-hex) input.\n", g.linenumber, highch);
            goto error;
         }
         if (!isxdigit(lowch)) {
            PRINT_ERRORX("Line %" PRIu64 ": Unexpected '%c' (non-hex) input.\n", g.linenumber, lowch);
            goto error;
         }
         if (idx == datasize[which]) {
            /* Overflow, double the memory. */
            if (doublechararray(&g.get_dbt.data[which], &datasize[which])) goto error;
            datum = g.get_dbt.data[which];
         }
         datum[idx] = (char)((hextoint(highch) << 4) | hextoint(lowch));
         idx++;
      }
      if (highch == EOF) g.eof = true;
   }

   /* Done reading. */
   pdbt->size = idx;
   pdbt->data = (void*)datum;
   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
}

static int insert_pair(DBT* key, DBT* data)
{
   DB* db = g.db;

   int retval = db->put(db, NULL, key, data, g.overwritekeys ? 0 : DB_NOOVERWRITE);
   if (retval != 0) {
      //TODO: Check for transaction failures/etc.. retry if necessary.
      PRINT_ERROR(retval, "DB->put");
      if (!(retval == DB_KEYEXIST && g.overwritekeys)) goto error;
   }
   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
}

int read_keys()
{
   DBT key;
   DBT data;
   int spacech;

   char footer[sizeof("ATA=END\n")];

   memset(&key, 0, sizeof(key));
   memset(&data, 0, sizeof(data));


   //TODO: Start transaction/end transaction/abort/retry/etc

   if (!g.leadingspace) {
      assert(g.plaintext);
      while (!g.eof) {
         if (caught_any_signals()) goto success;
         g.linenumber++;
         if (get_dbt(&key) != 0) goto error;
         if (g.eof) {
            if (key.size == 0) {
                //Last entry had no newline.  Done.
                break;
            }
            PRINT_ERRORX("Line %" PRIu64 ": Key exists but value missing.", g.linenumber);
            goto error;
         }
         g.linenumber++;
         if (get_dbt(&data) != 0) goto error;
         if (insert_pair(&key, &data) != 0) goto error;
      }
   }
   else while (!g.eof) {
      if (caught_any_signals()) goto success;
      g.linenumber++;
      spacech = getchar();
      switch (spacech) {
         case (EOF): {
            /* Done. */
            g.eof = true;
            goto success;
         }
         case (' '): {
            /* Time to read a key. */
            if (get_dbt(&key) != 0) goto error;
            break;
         }
         case ('D'): {
            if (fgets(footer, sizeof("ATA=END\n"), stdin) != NULL &&
               (!strcmp(footer, "ATA=END") || !strcmp(footer, "ATA=END\n")))
            {
               goto success;
            }
            goto unexpectedinput;
         }
         default: {
unexpectedinput:
            PRINT_ERRORX("Line %" PRIu64 ": Unexpected input while reading key.\n", g.linenumber);
            goto error;
         }
      }

      if (g.eof) {
         PRINT_ERRORX("Line %" PRIu64 ": Key exists but value missing.", g.linenumber);
         goto error;
      }
      g.linenumber++;
      spacech = getchar();
      switch (spacech) {
         case (EOF): {
            g.eof = true;
            PRINT_ERRORX("Line %" PRIu64 ": Unexpected end of file while reading value.\n", g.linenumber);
            goto error;
         }
         case (' '): {
            /* Time to read a key. */
            if (get_dbt(&data) != 0) goto error;
            break;
         }
         default: {
            PRINT_ERRORX("Line %" PRIu64 ": Unexpected input while reading value.\n", g.linenumber);
            goto error;
         }
      }
      if (insert_pair(&key, &data) != 0) goto error;
   }
success:
   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
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
