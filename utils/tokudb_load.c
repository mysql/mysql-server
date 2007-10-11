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
   char*    data[2];
} gdbt;

typedef struct {
   char*    data;
} rhdr;

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

int   usage          ();
int   longusage      ();
int   load_database  ();
int   create_init_env();
int   read_header    ();
int   open_database  ();
int   read_keys      ();
int   apply_commandline_options();
int   close_database ();
int   hextoint       (int ch);
int   doublechararray(char** pmem, uint64_t* size);

int main(int argc, char *argv[]) {
   int ch;
   int retval;
   struct option options[] = {
      { "no_overwrite", no_argument,         NULL, 'n' },
      { "help",         no_argument,         NULL, 'H' },
      { "plain_text",   no_argument,         NULL, 'T' },
      { "Version",      no_argument,         NULL, 'V' },
      { "config",       required_argument,   NULL, 'c' },
      { "input_file",   required_argument,   NULL, 'f' },
      { "home",         required_argument,   NULL, 'h' },
      { "password",     required_argument,   NULL, 'p' },
      { "type",         required_argument,   NULL, 't' },
      { NULL,           0,                   NULL, 0   }
   };
   char** next_config_option;

   /* Set up the globals. */
   memset(&g, 0, sizeof(g));
   g.leadingspace   = true;
   g.overwritekeys  = true;
   g.dbtype         = DB_UNKNOWN;
   g.progname       = argv[0];
   g.header         = true;

   next_config_option = g.config_options = (char**) calloc(argc, sizeof(char*));
   if (next_config_option == NULL) {
      fprintf(stderr, "%s: %s\n", g.progname, strerror(errno));
      goto error;
   }

   while ((ch = getopt_long_only(argc, argv, "nHTVc:f:h:p:t:", options, NULL)) != EOF) {
      switch (ch) {
         case ('n'): {
            /* g.overwritekeys = false; */
            fprintf(stderr, "%s: -%c option not supported.\n", g.progname, ch);
            goto error;
         }
         case ('H'): {
            g.exitcode = longusage();
            goto cleanup;
         }
         case ('T'): {
            g.plaintext    = true;
            g.leadingspace = false;
            g.header       = false;
            break;
         }
         case ('V'): {
            fprintf(stderr, "%s: -%c option not supported.\n", g.progname, ch);
            goto error;
         }
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
         case ('p'): {
            /* Clear password. */
            memset(optarg, 0, strlen(optarg));
            fprintf(stderr, "%s: -%c option not supported.\n", g.progname, ch);
            goto error;
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
   //TODO:  /* Handle possible interruptions/signals. */

   g.database = argv[0];
   if (create_init_env() != 0) goto error;
   while (!g.eof) {
      if (load_database() != 0) goto error;
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
   //TODO:  /* Resend any caught signal. */
   if (g.config_options)   free(g.config_options);
   if (g.subdatabase)      free(g.subdatabase);
   if (g.read_header.data) free(g.read_header.data);
   if (g.get_dbt.data[0])  free(g.get_dbt.data[0]);
   if (g.get_dbt.data[1])  free(g.get_dbt.data[1]);

   return g.exitcode;
}

int load_database()
{
   DB_ENV* dbenv = g.dbenv;
   int retval;

   /* Create a database handle. */
   retval = db_create(&g.db, g.dbenv, 0);
   if (retval != 0) {
      dbenv->err(dbenv, retval, "db_create");
      return EXIT_FAILURE;
   }

   if (g.header && read_header() != 0) goto error;
   if (g.eof) goto cleanup;
   if (apply_commandline_options() != 0) goto error;
   if (g.eof) goto cleanup;

   //TODO: Only quit out if DB does NOT EXIST.
   if (g.dbtype == DB_UNKNOWN) {
      dbenv->err(dbenv, 0, "no database type specified");
      goto error;
   }
   /*
   TODO: If/when supporting encryption
   if (g.password && (retval = db->set_flags(db, DB_ENCRYPT))) {
      dbenv->err(dbenv, ret, "DB->set_flags: DB_ENCRYPT");
      goto error;
   }
   */
   if (open_database() != 0) goto error;
   if (g.eof) goto cleanup;
   if (read_keys() != 0) goto error;
   if (g.eof) goto cleanup;

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
   printf("TODO: Implement %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
   printf
   (
      "usage: %s [-ThHfF] [-d delimiter] [-s delimiter]\n"
      "       -m minsize -M maxsize [-r random seed]\n"
      "       (-n maxnumkeys | -N maxkibibytes) [-o filename]\n",
      g.progname
   );
   return EXIT_FAILURE;
}

int longusage()
{
   printf("TODO: Implement %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
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
      dbenv->err(dbenv, retval, "set_passwd");
      goto error;
   }
   */

   /* Open the dbenvironment. */
   g.is_private = false;
//   flags = DB_JOINENV | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_USE_ENVIRON;
   flags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL; ///TODO: UNCOMMENT/IMPLEMENT | DB_USE_ENVIRON;
   //TODO: Transactions.. SET_BITS(flags, DB_INIT_TXN);
   
   /*
   ///TODO: UNCOMMENT/IMPLEMENT  Notes:  We require DB_PRIVATE
   if (!dbenv->open(dbenv, g.homedir, flags, 0)) goto success;
   */

   /*
   ///TODO: UNCOMMENT/IMPLEMENT 
   retval = dbenv->set_cachesize(dbenv, 0, cache, 1);
   */
   if (retval) {
      dbenv->err(dbenv, retval, "DB_ENV->set_cachesize");
      goto error;
   }
   g.is_private = true;
   //TODO: Do we want to support transactions/logging even in single-process mode?
   //Maybe if the db already exists.
   //If db does not exist.. makes sense not to log or have transactions
   REMOVE_BITS(flags, DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN);
   SET_BITS(flags, DB_CREATE | DB_PRIVATE);

   retval = dbenv->open(dbenv, g.homedir ? g.homedir : ".", flags, 0);
   if (retval) {
      dbenv->err(dbenv, retval, "DB_ENV->open");
      goto error;
   }
success:
   g.dbenv = dbenv;
   return EXIT_SUCCESS;

error:
   return EXIT_FAILURE;
}

int printabletocstring(char* inputstr, char** poutputstr)
{
   char highch;
   char lowch;
   char nextch;
   char* cstring;
   DB_ENV* dbenv = g.dbenv;


   assert(inputstr);
   assert(poutputstr);
   assert(*poutputstr == NULL);

   cstring = (char*)malloc((strlen(inputstr) + 1) * sizeof(char));
   if (cstring == NULL) {
      dbenv->err(dbenv, ENOMEM, "");
      goto error;
   }

   for (*poutputstr = cstring; *inputstr != '\0'; inputstr++) {
      if (*inputstr == '\\') {
         if ((highch = *++inputstr) == '\\') {
            *cstring++ = '\\';
            continue;
         }
         if (highch == '\0' || (lowch = *++inputstr) == '\0') {
            dbenv->err(dbenv, 0, "unexpected end of input data or key/data pair");
            goto error;
         }
         if (!isxdigit(highch)) {
            dbenv->err(dbenv, 0, "Unexpected '%c' (non-hex) input.\n", highch);
            goto error;
         }
         if (!isxdigit(lowch)) {
            dbenv->err(dbenv, 0, "Unexpected '%c' (non-hex) input.\n", lowch);
            goto error;
         }
         nextch = (hextoint(highch) << 4) | hextoint(lowch);
         if (nextch == '\0') {
            /* Database names are c strings, and cannot have extra NULL terminators. */
            dbenv->err(dbenv, 0, "Unexpected '\\00' in input.\n");
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
   dbenv->err(dbenv, 0, "Quitting out due to errors.\n");
   return EXIT_FAILURE;
}

///TODO: IMPLEMENT/Replace original line.
#define PARSE_NUMBER(match, dbfunction)                                    \
if (!strcmp(field, match)) {                                               \
   if (strtoint32(dbenv, NULL, value, &num, 1, INT32_MAX, 10)) goto error; \
   /*if ((retval = dbfunction(db, num)) != 0) goto printerror;*/           \
   continue;                                                               \
}

///TODO: IMPLEMENT/Replace original line.
#define PARSE_UNSUPPORTEDNUMBER(match, dbfunction)                         \
if (!strcmp(field, match)) {                                               \
   if (strtoint32(dbenv, NULL, value, &num, 1, INT32_MAX, 10)) goto error; \
   dbenv->err(dbenv, 0, "%s option not supported.\n", field);              \
   goto error;                                                             \
}

#define PARSE_FLAG(match, flag)                          \
if (!strcmp(field, match)) {                             \
   if (strtoint32(dbenv, NULL, value, &num, 0, 1, 10)) { \
      dbenv->err(dbenv, 0,                               \
               "%s: boolean name=value pairs require a value of 0 or 1",  \
               field);                                   \
      goto error;                                        \
   }                                                     \
   if ((retval = db->set_flags(db, flag)) != 0) {        \
      dbenv->err(dbenv, retval,                          \
              "set_flags: %s",                           \
              field);                                    \
      goto error;                                        \
   }                                                     \
   continue;                                             \
}

#define PARSE_UNSUPPORTEDFLAG(match, flag)               \
if (!strcmp(field, match)) {                             \
   if (strtoint32(dbenv, NULL, value, &num, 0, 1, 10)) { \
      dbenv->err(dbenv, 0,                               \
               "%s: boolean name=value pairs require a value of 0 or 1",  \
               field);                                   \
      goto error;                                        \
   }                                                     \
   dbenv->err(dbenv, 0, "%s option not supported.\n", field);    \
   goto error;                                           \
}

#define PARSE_CHAR(match, dbfunction)                    \
if (!strcmp(field, match)) {                             \
   if (strlen(value) != 1) {                             \
      dbenv->err(dbenv, 0,                               \
               "%s=%s: Expected 1-byte value",           \
               field, value);                            \
      goto error;                                        \
   }                                                     \
   if ((retval = dbfunction(db, value[0])) != 0) {       \
      goto printerror;                                   \
   }                                                     \
   continue;                                             \
}

int read_header()
{
   static uint64_t datasize = 1 << 10;
   uint64_t index = 0;
   char* field;
   char* value;
   int ch;
   int32_t num;
   int retval;
   DB* db = g.db;
   DB_ENV* dbenv = g.dbenv;

   assert(g.header);

   if (g.read_header.data == NULL && (g.read_header.data = (char*)malloc(datasize * sizeof(char))) == NULL) {
      dbenv->err(dbenv, errno, "");
      goto error;
   }
   while (!g.eof) {
      g.linenumber++;
      index = 0;
      /* Read a line. */
      while (true) {
         if ((ch = getchar()) == EOF) {
            g.eof = true;
//TODO: Check ferror for every EOF test.
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
         if (strtoint32(dbenv, NULL, value, &g.version, 1, INT32_MAX, 10)) goto error;
         if (g.version != 3) {
            dbenv->err(dbenv, 0, "line %lu: VERSION %d is unsupported", g.linenumber, g.version);
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
            dbenv->err(dbenv, 0, "db type %s not supported.\n", value);
            goto error;
         }
         dbenv->err(dbenv, 0, "line %lu: unknown type %s", g.linenumber, value);
         goto error;
      }
      if (!strcmp(field, "database") || !strcmp(field, "subdatabase")) {
         if (g.subdatabase != NULL) {
            //TODO: Free before quitting main., clear at start too.. could be a new db without a name?
            free(g.subdatabase);
            g.subdatabase = NULL;
         }
         if ((retval = printabletocstring(value, &g.subdatabase))) {
            dbenv->err(dbenv, retval, "error reading db name");
            goto error;
         }
         continue;
      }
      if (!strcmp(field, "keys")) {
         int32_t temp;
         if (strtoint32(dbenv, NULL, value, &temp, 0, 1, 10)) {
            dbenv->err(dbenv, 0,
                     "%s: boolean name=value pairs require a value of 0 or 1",
                     field);
            goto error;
         }
         g.keys = temp;
         if (!g.keys) {
            dbenv->err(dbenv, 0, "keys=0 not supported", field);
            goto error;
         }
         continue;
      }
      PARSE_NUMBER(           "bt_minkey",   db->set_bt_minkey);
      PARSE_NUMBER(           "db_lorder",   db->set_lorder);
      PARSE_NUMBER(           "db_pagesize", db->set_pagesize);
      PARSE_NUMBER(           "re_len",      db->set_re_len);
      PARSE_UNSUPPORTEDNUMBER("extentsize",  db->set_q_extentsize);
      PARSE_UNSUPPORTEDNUMBER("h_ffactor",   db->set_h_ffactor);
      PARSE_UNSUPPORTEDNUMBER("h_nelem",     db->set_h_nelem);
      PARSE_CHAR(             "re_pad",      db->set_re_pad);
      PARSE_FLAG(             "chksum",      DB_CHKSUM_SHA1);
      PARSE_FLAG(             "duplicates",  DB_DUP);
      PARSE_FLAG(             "dupsort",     DB_DUPSORT);
      PARSE_FLAG(             "recnum",      DB_RECNUM);
      PARSE_UNSUPPORTEDFLAG(  "renumber",    DB_RENUMBER);

      dbenv->err(dbenv, 0, "unknown input-file header configuration keyword \"%s\"", field);
      goto error;
   }
success:
   return EXIT_SUCCESS;

   if (false) {
printerror:
      dbenv->err(dbenv, retval, "%s=%s", field, value);
   }
   if (false) {
formaterror:
      dbenv->err(dbenv, 0, "line %lu: unexpected format", g.linenumber);
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

   assert(g.header);

   for (index = 0; g.config_options[index]; index++) {
      if (value) {
         /* Restore the field=value format. */
         value[-1] = '=';
         value = NULL;
      }
      field = g.config_options[index];

      if ((value = strchr(field, '=')) == NULL) {
         dbenv->err(dbenv, 0, "command-line configuration uses name=value format");
         goto error;
      }
      value[0] = '\0';
      value++;

      if (field[0] == '\0' || value[0] == '\0') {
         dbenv->err(dbenv, 0, "command-line configuration uses name=value format");
         goto error;
      }

      if (!strcmp(field, "database") || !strcmp(field, "subdatabase")) {
         if (g.subdatabase != NULL) {
            free(g.subdatabase);
            g.subdatabase = NULL;
         }
         if ((retval = printabletocstring(value, &g.subdatabase))) {
            dbenv->err(dbenv, retval, "error reading db name");
            goto error;
         }
         continue;
      }
      if (!strcmp(field, "keys")) {
         int32_t temp;
         if (strtoint32(dbenv, NULL, value, &temp, 0, 1, 10)) {
            dbenv->err(dbenv, 0,
                     "%s: boolean name=value pairs require a value of 0 or 1",
                     field);
            goto error;
         }
         g.keys = temp;
         if (!g.keys) {
            dbenv->err(dbenv, 0, "keys=0 not supported", field);
            goto error;
         }
         continue;
      }
      /*
      ///TODO: UNCOMMENT/IMPLEMENT
      PARSE_NUMBER(           "bt_minkey",   db->set_bt_minkey);
      PARSE_NUMBER(           "db_lorder",   db->set_lorder);
      PARSE_NUMBER(           "db_pagesize", db->set_pagesize);
      PARSE_NUMBER(           "re_len",      db->set_re_len);
      PARSE_UNSUPPORTEDNUMBER("extentsize",  db->set_q_extentsize);
      PARSE_UNSUPPORTEDNUMBER("h_ffactor",   db->set_h_ffactor);
      PARSE_UNSUPPORTEDNUMBER("h_nelem",     db->set_h_nelem);
      PARSE_CHAR(             "re_pad",      db->set_re_pad);
      PARSE_FLAG(             "chksum",      DB_CHKSUM_SHA1);
      PARSE_FLAG(             "duplicates",  DB_DUP);
      PARSE_FLAG(             "dupsort",     DB_DUPSORT);
      PARSE_FLAG(             "recnum",      DB_RECNUM);
      PARSE_UNSUPPORTEDFLAG(  "renumber",    DB_RENUMBER);
      */

      dbenv->err(dbenv, 0, "unknown input-file header configuration keyword \"%s\"", field);
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
      dbenv->err(dbenv, retval, "%s=%s", field, value);
   }
error:
   return EXIT_FAILURE;
}

int open_database()
{
   DB* db = g.db;
   DB_ENV* dbenv = g.dbenv;
   int retval;

   int open_flags = DB_CREATE;
   //TODO: Transaction auto commit stuff
   //if (TXN_ON(dbenv)) SET_BITS(open_flags, DB_AUTO_COMMIT);
//TODO: First see if it exists.. THEN create it?
   retval = db->open(db, NULL, g.database, g.subdatabase, g.dbtype, open_flags, 0666);
   if (retval != 0) {
      dbenv->err(dbenv, retval, "DB->open: %s", g.database);
      goto error;
   }
   //TODO: Ensure we have enough cache to store some min number of btree pages.
   //NOTE: This may require closing db, environment, and creating new ones.


/*
///TODO: UNCOMMENT/IMPLEMENT
   DBTYPE existingtype;
   retval = db->get_type(db, &existingtype);
   if (retval != 0) {
      dbenv->err(dbenv, retval, "DB->get_type: %s", g.database);
      goto error;
   }
   assert(g.dbtype == DB_BTREE);
   if (existingtype != g.dbtype) {
      fprintf(stderr, "Existing database is not a dictionary (DB_BTREE).\n");
      goto error;
   }
   */
   return EXIT_SUCCESS;
error:
   fprintf(stderr, "Quitting out due to errors.\n");
   return EXIT_FAILURE;
}

int hextoint(int ch)
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

int doublechararray(char** pmem, uint64_t* size)
{
   DB_ENV* dbenv = g.dbenv;

   assert(pmem);
   assert(size);
   assert(IS_POWER_OF_2(*size));

   *size <<= 1;
   if (*size == 0) {
      /* Overflowed uint64_t. */
      dbenv->err(dbenv, 0, "Line %llu: Line too long.\n", g.linenumber);
      goto error;
   }
   if ((*pmem = (char*)realloc(*pmem, *size)) == NULL) {
      dbenv->err(dbenv, errno, "");
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
   static uint64_t datasize[2] = {1 << 10, 1 << 10};
   static int which = 0;
   char* datum;
   uint64_t index = 0;
   int highch;
   int lowch;
   DB* db = g.db;

   /* *pdbt should have been memset to 0 before being called. */
   which = 1 - which;
   if (g.get_dbt.data[which] == NULL &&
      (g.get_dbt.data[which] = (char*)malloc(datasize[which] * sizeof(char))) == NULL) {
      dbenv->err(dbenv, errno, "");
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
                  dbenv->err(dbenv, 0, "Line %llu: Unexpected end of file (2 hex digits per byte).\n", g.linenumber);
                  goto error;
               }
               else if (!isxdigit(highch)) {
                  dbenv->err(dbenv, 0, "Line %llu: Unexpected '%c' (non-hex) input.\n", g.linenumber, highch);
                  goto error;
               }

               lowch = getchar();
               if (lowch == EOF) {
                  g.eof = true;
                  dbenv->err(dbenv, 0, "Line %llu: Unexpected end of file (2 hex digits per byte).\n", g.linenumber);
                  goto error;
               }
               else if (!isxdigit(lowch)) {
                  dbenv->err(dbenv, 0, "Line %llu: Unexpected '%c' (non-hex) input.\n", g.linenumber, lowch);
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
               dbenv->err(dbenv, 0, "Line %llu: Nonprintable character found.", g.linenumber);
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
            dbenv->err(dbenv, 0, "Line %llu: Unexpected end of file (2 hex digits per byte).\n", g.linenumber);
            goto error;
         }
         if (!isxdigit(highch)) {
            dbenv->err(dbenv, 0, "Line %llu: Unexpected '%c' (non-hex) input.\n", g.linenumber, highch);
            goto error;
         }
         if (!isxdigit(lowch)) {
            dbenv->err(dbenv, 0, "Line %llu: Unexpected '%c' (non-hex) input.\n", g.linenumber, lowch);
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

int insert_pair(DBT* key, DBT* data)
{
   DB_ENV* dbenv = g.dbenv;
   DB* db = g.db;

   int retval = db->put(db, NULL, key, data, g.overwritekeys ? 0 : DB_NOOVERWRITE);
   if (retval != 0) {
      //TODO: Check for transaction failures/etc.. retry if necessary.
      dbenv->err(dbenv, retval, "DB->put");
      goto error;
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
         g.linenumber++;
         if (get_dbt(&key) != 0) goto error;
         if (g.eof) {
            dbenv->err(dbenv, 0, "Line %llu: Key exists but value missing.", g.linenumber);
            goto error;
         }
         g.linenumber++;
         if (get_dbt(&data) != 0) goto error;
         if (insert_pair(&key, &data) != 0) goto error;
      }
   }
   else while (!g.eof) {
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
            dbenv->err(dbenv, 0, "Line %llu: Unexpected input while reading key.\n", g.linenumber);
            goto error;
         }
      }

      if (g.eof) {
         dbenv->err(dbenv, 0, "Line %llu: Key exists but value missing.", g.linenumber);
         goto error;
      }
      g.linenumber++;
      spacech = getchar();
      switch (spacech) {
         case (EOF): {
            g.eof = true;
            dbenv->err(dbenv, 0, "Line %llu: Unexpected end of file while reading value.\n", g.linenumber);
            goto error;
         }
         case (' '): {
            /* Time to read a key. */
            if (get_dbt(&data) != 0) goto error;
            break;
         }
         default: {
            dbenv->err(dbenv, 0, "Line %llu: Unexpected input while reading value.\n", g.linenumber);
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
      dbenv->err(dbenv, retval, "DB->close");
      goto error;
   }
   return EXIT_SUCCESS;
error:
   return EXIT_FAILURE;
}
