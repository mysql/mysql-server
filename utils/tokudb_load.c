/* -*- mode: C; c-basic-offset: 3 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <assert.h>
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
   u_int64_t linenumber;
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

int   usage          ();
int   longusage      ();
int   load_database  ();
int   create_init_env();
int   read_header    ();
int   open_database  ();
int   read_keys      ();
int   apply_commandline_options();
int   close_database ();
int   doublechararray(char** pmem, u_int64_t* size);

int main(int argc, char *argv[]) {
   int ch;
   int retval;
   char** next_config_option;

   /* Set up the globals. */
   memset(&g, 0, sizeof(g));
   g.leadingspace   = true;
   g.overwritekeys  = true;
   //TODO: g.dbtype         = DB_UNKNOWN; when defined.
   g.dbtype         = DB_BTREE;
   g.progname       = argv[0];
   g.header         = true;
   
   if (verify_library_version() != 0) goto error;

   next_config_option = g.config_options = (char**) calloc(argc, sizeof(char*));
   if (next_config_option == NULL) {
      ERROR(errno, "main: calloc\n");
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
            ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('P'): {
            /* Clear password. */
            memset(optarg, 0, strlen(optarg));
            ERRORX("-%c option not supported.\n", ch);
            goto error;
         }
         case ('r'): {
            ERRORX("-%c option not supported.\n", ch);
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
   if (g.config_options)   free(g.config_options);
   if (g.subdatabase)      free(g.subdatabase);
   if (g.read_header.data) free(g.read_header.data);
   if (g.get_dbt.data[0])  free(g.get_dbt.data[0]);
   if (g.get_dbt.data[1])  free(g.get_dbt.data[1]);
   resend_signals();

   return g.exitcode;
}

int load_database()
{
   DB_ENV* dbenv = g.dbenv;
   int retval;

   /* Create a database handle. */
   retval = db_create(&g.db, g.dbenv, 0);
   if (retval != 0) {
      ERROR(retval, "db_create");
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
      ERROR(ret, "DB->set_flags: DB_ENCRYPT");
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
   int cache = 1 << 20; /* 1 megabyte */

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
      ERROR(retval, "set_passwd");
      goto error;
   }
   */

   /* Open the dbenvironment. */
   g.is_private = false;
   flags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL; ///TODO: UNCOMMENT/IMPLEMENT | DB_USE_ENVIRON;
   //TODO: Transactions.. SET_BITS(flags, DB_INIT_TXN);
   
   /*
   ///TODO: UNCOMMENT/IMPLEMENT  Notes:  We require DB_PRIVATE
   if (!dbenv->open(dbenv, g.homedir, flags, 0)) goto success;
   */

   /*
   ///TODO: UNCOMMENT/IMPLEMENT 
   retval = dbenv->set_cachesize(dbenv, 0, cache, 1);
   if (retval) {
      ERROR(retval, "DB_ENV->set_cachesize");
      goto error;
   }
   */
   g.is_private = true;
   //TODO: Do we want to support transactions/logging even in single-process mode?
   //Maybe if the db already exists.
   //If db does not exist.. makes sense not to log or have transactions
   REMOVE_BITS(flags, DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN);
   SET_BITS(flags, DB_CREATE | DB_PRIVATE);

   retval = dbenv->open(dbenv, g.homedir ? g.homedir : ".", flags, 0);
   if (retval) {
      ERROR(retval, "DB_ENV->open");
      goto error;
   }
success:
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
   ERRORX("%s option not supported.\n", field);                            \
   goto error;                                                             \
}
#define PARSE_IGNOREDNUMBER(match, dbfunction)                             \
if (!strcmp(field, match)) {                                               \
   if (strtoint32(value, &num, 1, INT32_MAX, 10)) goto error;              \
   ERRORX("%s option not supported yet (ignored).\n", field);              \
   continue;                                                               \
}

#define PARSE_FLAG(match, flag)                          \
if (!strcmp(field, match)) {                             \
   if (strtoint32(value, &num, 0, 1, 10)) {              \
      ERRORX("%s: boolean name=value pairs require a value of 0 or 1",  \
             field);                                     \
      goto error;                                        \
   }                                                     \
   if ((retval = db->set_flags(db, flag)) != 0) {        \
      ERROR(retval, "set_flags: %s", field);             \
      goto error;                                        \
   }                                                     \
   continue;                                             \
}

#define PARSE_UNSUPPORTEDFLAG(match, flag)               \
if (!strcmp(field, match)) {                             \
   if (strtoint32(value, &num, 0, 1, 10)) {              \
      ERRORX("%s: boolean name=value pairs require a value of 0 or 1",  \
             field);                                     \
      goto error;                                        \
   }                                                     \
   ERRORX("%s option not supported.\n", field);          \
   goto error;                                           \
}

#define PARSE_IGNOREDFLAG(match, flag)                   \
if (!strcmp(field, match)) {                             \
   if (strtoint32(value, &num, 0, 1, 10)) {              \
      ERRORX("%s: boolean name=value pairs require a value of 0 or 1",  \
             field);                                     \
      goto error;                                        \
   }                                                     \
   ERRORX("%s option not supported yet (ignored).\n", field);  \
   continue;                                             \
}

#define PARSE_CHAR(match, dbfunction)                    \
if (!strcmp(field, match)) {                             \
   if (strlen(value) != 1) {                             \
      ERRORX("%s=%s: Expected 1-byte value",             \
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
      ERRORX("%s=%s: Expected 1-byte value",             \
             field, value);                              \
      goto error;                                        \
   }                                                     \
   ERRORX("%s option not supported.\n", field);          \
   goto error;                                           \
}

#define PARSE_COMMON_CONFIGURATIONS()  \
      PARSE_IGNOREDNUMBER(    "bt_minkey",   db->set_bt_minkey);     \
      PARSE_IGNOREDFLAG(      "chksum",      DB_CHKSUM);             \
      PARSE_IGNOREDNUMBER(    "db_lorder",   db->set_lorder);        \
      PARSE_IGNOREDNUMBER(    "db_pagesize", db->set_pagesize);      \
      PARSE_FLAG(             "duplicates",  DB_DUP);                \
      PARSE_FLAG(             "dupsort",     DB_DUPSORT);            \
      PARSE_UNSUPPORTEDNUMBER("extentsize",  db->set_q_extentsize);  \
      PARSE_UNSUPPORTEDNUMBER("h_ffactor",   db->set_h_ffactor);     \
      PARSE_UNSUPPORTEDNUMBER("h_nelem",     db->set_h_nelem);       \
      PARSE_UNSUPPORTEDNUMBER("re_len",      db->set_re_len);        \
      PARSE_UNSUPPORTEDCHAR(  "re_pad",      db->set_re_pad);        \
      PARSE_UNSUPPORTEDFLAG(  "recnum",      DB_RECNUM);             \
      PARSE_UNSUPPORTEDFLAG(  "renumber",    DB_RENUMBER);



int read_header()
{
   static u_int64_t datasize = 1 << 10;
   u_int64_t index = 0;
   char* field;
   char* value;
   int ch;
   int32_t num;
   int retval;
   DB* db = g.db;
   DB_ENV* dbenv = g.dbenv;

   assert(g.header);

   if (g.read_header.data == NULL && (g.read_header.data = (char*)malloc(datasize * sizeof(char))) == NULL) {
      ERROR(errno, "read_header: malloc");
      goto error;
   }
   while (!g.eof) {
      if (caught_any_signals()) goto success;    
      g.linenumber++;
      index = 0;
      /* Read a line. */
      while (true) {
         if ((ch = getchar()) == EOF) {
            g.eof = true;
            if (ferror(stdin)) goto formaterror;
            break;
         }
         if (ch == '\n') break;

         g.read_header.data[index] = ch;
         index++;

         /* Ensure room exists for next character/null terminator. */
         if (index == datasize && doublechararray(&g.read_header.data, &datasize)) goto error;
      }
      if (index == 0 && g.eof) goto success;
      g.read_header.data[index] = '\0';

      field = g.read_header.data;
      if ((value = strchr(g.read_header.data, '=')) == NULL) goto formaterror;
      value[0] = '\0';
      value++;

      if (field[0] == '\0' || value[0] == '\0') goto formaterror;

      if (!strcmp(field, "HEADER")) break;
      if (!strcmp(field, "VERSION")) {
         if (strtoint32(value, &g.version, 1, INT32_MAX, 10)) goto error;
         if (g.version != 3) {
            ERRORX("line %"PRIu64": VERSION %d is unsupported", g.linenumber, g.version);
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
            ERRORX("db type %s not supported.\n", value);
            goto error;
         }
         ERRORX("line %"PRIu64": unknown type %s", g.linenumber, value);
         goto error;
      }
      if (!strcmp(field, "database") || !strcmp(field, "subdatabase")) {
         if (g.subdatabase != NULL) {
            free(g.subdatabase);
            g.subdatabase = NULL;
         }
         if ((retval = printabletocstring(value, &g.subdatabase))) {
            ERROR(retval, "error reading db name");
            goto error;
         }
         continue;
      }
      if (!strcmp(field, "keys")) {
         int32_t temp;
         if (strtoint32(value, &temp, 0, 1, 10)) {
            ERROR(0,
                     "%s: boolean name=value pairs require a value of 0 or 1",
                     field);
            goto error;
         }
         g.keys = temp;
         if (!g.keys) {
            ERRORX("keys=0 not supported");
            goto error;
         }
         continue;
      }
      PARSE_COMMON_CONFIGURATIONS();

      ERRORX("unknown input-file header configuration keyword \"%s\"", field);
      goto error;
   }
success:
   return EXIT_SUCCESS;

   if (false) {
printerror:
      ERROR(retval, "%s=%s", field, value);
   }
   if (false) {
formaterror:
      ERRORX("line %"PRIu64": unexpected format", g.linenumber);
   }
error:
   return EXIT_FAILURE;
}

int apply_commandline_options()
{
   char** next_config_option = g.config_options;
   unsigned index;
   char* field;
   char* value = NULL;
   bool first;
   int ch;
   int32_t num;
   int retval;
   DB* db = g.db;
   DB_ENV* dbenv = g.dbenv;

   for (index = 0; g.config_options[index]; index++) {
      if (value) {
         /* Restore the field=value format. */
         value[-1] = '=';
         value = NULL;
      }
      field = g.config_options[index];

      if ((value = strchr(field, '=')) == NULL) {
         ERRORX("command-line configuration uses name=value format");
         goto error;
      }
      value[0] = '\0';
      value++;

      if (field[0] == '\0' || value[0] == '\0') {
         ERRORX("command-line configuration uses name=value format");
         goto error;
      }

      if (!strcmp(field, "database") || !strcmp(field, "subdatabase")) {
         if (g.subdatabase != NULL) {
            free(g.subdatabase);
            g.subdatabase = NULL;
         }
         if ((retval = printabletocstring(value, &g.subdatabase))) {
            ERROR(retval, "error reading db name");
            goto error;
         }
         continue;
      }
      if (!strcmp(field, "keys")) {
         int32_t temp;
         if (strtoint32(value, &temp, 0, 1, 10)) {
            ERROR(0,
                     "%s: boolean name=value pairs require a value of 0 or 1",
                     field);
            goto error;
         }
         g.keys = temp;
         if (!g.keys) {
            ERRORX("keys=0 not supported");
            goto error;
         }
         continue;
      }
      PARSE_COMMON_CONFIGURATIONS();

      ERRORX("unknown input-file header configuration keyword \"%s\"", field);
      goto error;
   }
   if (value) {
      /* Restore the field=value format. */
      value[-1] = '=';
      value = NULL;
   }
   return EXIT_SUCCESS;

   if (false) {
printerror:
      ERROR(retval, "%s=%s", field, value);
   }
error:
   return EXIT_FAILURE;
}

int open_database()
{
   DB* db = g.db;
   DB_ENV* dbenv = g.dbenv;
   int retval;
   DBTYPE opened_type;

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
         ERRORX("no database type specified");
         goto error;
      }*/
      SET_BITS(open_flags, DB_CREATE);
      //Try creating it.
      retval = db->open(db, NULL, g.database, g.subdatabase, g.dbtype, open_flags, 0666);
   }
   if (retval != 0) {
      ERROR(retval, "DB->open: %s", g.database);
      goto error;
   }
   //TODO: Uncomment when DB_UNKNOWN + db->get_type are implemented.
   /*
   if ((retval = db->get_type(db, &opened_type)) != 0) {
      ERROR(retval, "DB->get_type");
      goto error;
   }
   if (opened_type != DB_BTREE) {
      ERRORX("Unsupported db type %d\n", opened_type);
      goto error;
   }
   if (g.dbtype != DB_UNKNOWN && opened_type != g.dbtype) {
      ERRORX("DBTYPE %d does not match opened DBTYPE %d.\n", g.dbtype, opened_type);
      goto error;
   }*/
   return EXIT_SUCCESS;
error:
   fprintf(stderr, "Quitting out due to errors.\n");
   return EXIT_FAILURE;
}

int doublechararray(char** pmem, u_int64_t* size)
{
   DB_ENV* dbenv = g.dbenv;

   assert(pmem);
   assert(size);
   assert(IS_POWER_OF_2(*size));

   *size <<= 1;
   if (*size == 0) {
      /* Overflowed u_int64_t. */
      ERRORX("Line %"PRIu64": Line too long.\n", g.linenumber);
      goto error;
   }
   if ((*pmem = (char*)realloc(*pmem, *size)) == NULL) {
      ERROR(errno, "doublechararray: realloc");
      goto error;
   }
   return EXIT_SUCCESS;

error:
   return EXIT_FAILURE;
}

int get_dbt(DBT* pdbt)
{
   DB_ENV* dbenv = g.dbenv;
   /* Need to store a key and value. */
   static u_int64_t datasize[2] = {1 << 10, 1 << 10};
   static int which = 0;
   char* datum;
   u_int64_t index = 0;
   int highch;
   int lowch;
   DB* db = g.db;

   /* *pdbt should have been memset to 0 before being called. */
   which = 1 - which;
   if (g.get_dbt.data[which] == NULL &&
      (g.get_dbt.data[which] = (char*)malloc(datasize[which] * sizeof(char))) == NULL) {
      ERROR(errno, "get_dbt: malloc");
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
                  ERRORX("Line %"PRIu64": Unexpected end of file (2 hex digits per byte).\n", g.linenumber);
                  goto error;
               }
               else if (!isxdigit(highch)) {
                  ERRORX("Line %"PRIu64": Unexpected '%c' (non-hex) input.\n", g.linenumber, highch);
                  goto error;
               }

               lowch = getchar();
               if (lowch == EOF) {
                  g.eof = true;
                  ERRORX("Line %"PRIu64": Unexpected end of file (2 hex digits per byte).\n", g.linenumber);
                  goto error;
               }
               else if (!isxdigit(lowch)) {
                  ERRORX("Line %"PRIu64": Unexpected '%c' (non-hex) input.\n", g.linenumber, lowch);
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
               ERRORX("Line %"PRIu64": Nonprintable character found.", g.linenumber);
               goto error;
            }
         }
         if (nextch == EOF) {
            break;
         }
         if (index == datasize[which]) {
            /* Overflow, double the memory. */
            if (doublechararray(&g.get_dbt.data[which], &datasize[which])) goto error;
            datum = g.get_dbt.data[which];
         }
         datum[index] = nextch;
         index++;
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
            ERRORX("Line %"PRIu64": Unexpected end of file (2 hex digits per byte).\n", g.linenumber);
            goto error;
         }
         if (!isxdigit(highch)) {
            ERRORX("Line %"PRIu64": Unexpected '%c' (non-hex) input.\n", g.linenumber, highch);
            goto error;
         }
         if (!isxdigit(lowch)) {
            ERRORX("Line %"PRIu64": Unexpected '%c' (non-hex) input.\n", g.linenumber, lowch);
            goto error;
         }
         if (index == datasize[which]) {
            /* Overflow, double the memory. */
            if (doublechararray(&g.get_dbt.data[which], &datasize[which])) goto error;
            datum = g.get_dbt.data[which];
         }
         datum[index] = (hextoint(highch) << 4) | hextoint(lowch);
         index++;
      }
      if (highch == EOF) g.eof = true;
   }

   /* Done reading. */
   pdbt->size = index;
   pdbt->data = (void*)datum;
   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
}

#ifndef DB_YESOVERWRITE
#define DB_YESOVERWRITE 0
#endif

int insert_pair(DBT* key, DBT* data)
{
   DB_ENV* dbenv = g.dbenv;
   DB* db = g.db;

   int retval = db->put(db, NULL, key, data, g.overwritekeys ? DB_YESOVERWRITE : DB_NOOVERWRITE);
   if (retval != 0) {
      //TODO: Check for transaction failures/etc.. retry if necessary.
      ERROR(retval, "DB->put");
      if (!(retval == DB_KEYEXIST && g.overwritekeys)) goto error;
   }
   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
}

int read_keys()
{
   int retval;
   size_t length;
   DBT key;
   DBT data;
   int spacech;
   DB* db = g.db;
   DB_ENV* dbenv = g.dbenv;

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
            ERRORX("Line %"PRIu64": Key exists but value missing.", g.linenumber);
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
            ERRORX("Line %"PRIu64": Unexpected input while reading key.\n", g.linenumber);
            goto error;
         }
      }

      if (g.eof) {
         ERRORX("Line %"PRIu64": Key exists but value missing.", g.linenumber);
         goto error;
      }
      g.linenumber++;
      spacech = getchar();
      switch (spacech) {
         case (EOF): {
            g.eof = true;
            ERRORX("Line %"PRIu64": Unexpected end of file while reading value.\n", g.linenumber);
            goto error;
         }
         case (' '): {
            /* Time to read a key. */
            if (get_dbt(&data) != 0) goto error;
            break;
         }
         default: {
            ERRORX("Line %"PRIu64": Unexpected input while reading value.\n", g.linenumber);
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
   DB_ENV* dbenv = g.dbenv;
   int retval;

   assert(db);
   if ((retval = db->close(db, 0)) != 0) {
      ERROR(retval, "DB->close");
      goto error;
   }
   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
}
