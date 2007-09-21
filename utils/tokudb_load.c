#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>

#include <db.h>

extern char* optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

#if !defined(bool)
typedef unsigned char bool;
#endif

#if !defined(true)
#define true ((bool)1)
#endif

#if !defined(false)
#define false ((bool)0)
#endif
#define args(arguments) arguments

int   usage          args( (const char* progname) );
void  open_database  args( (DB** pdb, const char* database, DBTYPE dbtype) );
void  read_keys      args( (DB* db, bool plaintext, bool overwritekeys) );
void  close_database args( (DB* db) );
void  read_header    args( (bool plaintext, DBTYPE* dbtype, bool* dbtype_set, bool* overwritekeys) );
void  read_footer    args( (bool plaintext) );

bool leadingspace = true;
bool plaintext = false;
bool overwritekeys = true;
DBTYPE dbtype = DB_UNKNOWN;
char* progname;
bool header = true;
bool footer = true;

int main (int argc, char *argv[]) {
   progname = argv[0];
	
   char ch;
   while ((ch = getopt(argc, argv, "nTVc:f:h:t:")) != EOF) {
      switch (ch) {
         case ('n'): {
            overwritekeys = false;
/*
-n
    Do not overwrite existing keys in the database when loading into an already existing database. If a key/data pair cannot be loaded into the database for this reason, a warning message is displayed on the standard error output, and the key/data pair are skipped.
*/
            break;
         }
         case ('T'): {
            plaintext = true;
            leadingspace = false;
            footer = false;
            header = false;
            break;
         }
         case ('V'): {
            /* TODO: Write the library version number to the standard output, and exit. */
            printf("TODO: Write the library version number to the standard output, and exit.\n");
            return 0;
         }
         case ('c'): {
            printf("TODO: configuration options.\n");
/*
Specify configuration options ignoring any value they may have based on the input. The command-line format is name=value. See Supported Keywords for a list of supported words for the -c option.
bt_minkey (number)
    The minimum number of keys per page. 
database (string)
    The database to load. 
db_lorder (number)
    The byte order for integers in the stored database metadata. 
db_pagesize (number)
    The size of database pages, in bytes. 
duplicates (boolean)
    The value of the DB_DUP flag. 
dupsort (boolean)
    The value of the DB_DUPSORT flag. 
extentsize (number)
    The size of database extents, in pages, for Queue databases configured to use extents. 
h_ffactor (number)
    The density within the Hash database. 
h_nelem (number)
    The size of the Hash database. 
keys (boolean)
    Specify whether keys are present for Queue or Recno databases. 
re_len (number)
    Specify fixed-length records of the specified length. 
re_pad (string)
    Specify the fixed-length record pad character. 
recnum (boolean)
    The value of the DB_RECNUM flag. 
renumber (boolean)
    The value of the DB_RENUMBER flag.
*/
            return (EXIT_FAILURE);
         }         
         case ('f'): {
            if (freopen(optarg, "r", stdin) == NULL)
            {
               fprintf(
                  stderr,
                  "%s: %s: reopen: %s\n",
                  progname,
                  optarg,
                  strerror(errno)
               );
               return (EXIT_FAILURE);
            }
            break;
         }
         case ('h'): {
            return (EXIT_FAILURE);
/*
Specify a home directory for the database environment.

If a home directory is specified, the database environment is opened
using the DB_INIT_LOCK, DB_INIT_LOG, DB_INIT_MPOOL, DB_INIT_TXN, and
DB_USE_ENVIRON flags to DB_ENV->open. (This means that db_load can be
used to load data into databases while they are in use by other
processes.) If the DB_ENV->open call fails, or if no home directory is
specified, the database is still updated, but the environment is
ignored; for example, no locking is done.
*/
         }
         case ('t'): {
            if (!strcmp(optarg, "btree")) {
               dbtype = DB_BTREE;
            }
            else if (!strcmp(optarg, "hash")) {
               dbtype = DB_HASH;
            }
            else if (!strcmp(optarg, "recno")) {
               dbtype = DB_RECNO;
            }
            else if (!strcmp(optarg, "queue")) {
               dbtype = DB_QUEUE;
            }
            else {
               return (usage(progname));
            }
            
            if (dbtype != DB_BTREE) {
               fprintf(stderr, "DB type '%s' unsupported.\n", optarg);
               goto error;
            }
            break;
         }
         
         case ('?'):
         default: {
            return (usage(progname));
         }
      }
   }
   argc -= optind;
   argv += optind;
   
   if (plaintext && dbtype == DB_UNKNOWN) {
      fprintf(
         stderr,
         "%s: (-T) Plain text input requires a database type (-t).\n",
         ldg.progname
      );
      usage(ldg.progname);
      return (EXIT_FAILURE);
   }

   if (argc != 1) {
      return (usage(ldg.progname));
   }
   char* database = argv[0];
   DB* db;

   read_header(plaintext, &dbtype, &dbtype_set, &overwritekeys);
   open_database(&db, database, dbtype);
   read_keys(db, plaintext, overwritekeys);
   read_footer(plaintext);
   close_database(db);
   
   return 0;

error:
   fprintf(stderr, "Quitting out due to errors.\n");
   return EXIT_FAILURE;
}

int usage(const char* progname)
{
   printf
   (
      "usage: %s [-ThHfF] [-d delimiter] [-s delimiter]\n"
      "       -m minsize -M maxsize [-r random seed]\n"
      "       (-n maxnumkeys | -N maxkibibytes) [-o filename]\n",
      progname
   );
   return 1;
}

void open_database(DB** pdb, const char* database, DBTYPE dbtype)
{
   DB* db;
   
   int retval = db_create(pdb, NULL, 0);
   if (retval != 0) {
      fprintf(stderr, "db_create: %s\n", db_strerror(retval));
      goto error;
   }
   db = *pdb;
   retval = db->open(db, NULL, database, NULL, dbtype, DB_CREATE, 0664);
   if (retval != 0) {
      db->err(db, retval, "%s", database);
      goto error;
   }
   return;
error:
   fprintf(stderr, "Quitting out due to errors.\n");
   exit(EXIT_FAILURE);
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

void doublechararray(char** pmem, uint64_t* size)
{
   *size <<= 1;
   if (*size == 0) {
      /* Overflowed uint64_t. */
      fprintf(stderr, "DBT (key or data) overflow.  Size > 2^63.\n");
      goto error;
   }
   *pmem = (char*)realloc(*pmem, *size);
   if (*pmem == NULL) {
      perror("Doubling size of char array");
      goto error;
   }
   return;
error:
   fprintf(stderr, "Quitting out due to errors.\n");
   exit(EXIT_FAILURE);
}

void get_dbt(bool plaintext, DBT* pdbt)
{
   /* Need to store a key and value. */
   static char* data[2] = {NULL, NULL};
   static uint64_t datasize[2] = {1 << 10, 1 << 10};
   static int which = 0;
   char* datum;
   uint64_t index = 0;
   int highch;
   int lowch;
   
   which = 1 - which;
   if (data[which] == NULL)
   {
      data[which] = (char*)malloc(datasize[which] * sizeof(char));
   }
   datum = data[which];
   
   if (plaintext) {
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
                  fprintf(stderr, "Unexpected end of file (2 hex digits per byte).\n");
                  goto error;
               }
               else if (!isxdigit(highch)) {
                  fprintf(stderr, "Unexpected '%c' (non-hex) input.\n", highch);
                  goto error;
               }

               lowch = getchar();
               if (lowch == EOF) {
                  fprintf(stderr, "Unexpected end of file (2 hex digits per byte).\n");
                  goto error;
               }
               else if (!isxdigit(lowch)) {
                  fprintf(stderr, "Unexpected '%c' (non-hex) input.\n", lowch);
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
               fprintf(stderr, "Nonprintable character found.");
               goto error;
            }
         }
         if (nextch == EOF) {
            break;
         }
         if (index == datasize[which]) {
            /* Overflow, double the memory. */
            doublechararray(&data[which], &datasize[which]);
            datum = data[which]; 
         }
         datum[index] = nextch;
         index++;         
      }
   }
   else {
      for (highch = getchar(); highch != EOF; highch = getchar()) {
         if (highch == '\n') {
            /* Done reading this key/value. */
            break;
         }
         
         lowch = getchar();
         if (lowch == EOF) {
            fprintf(stderr, "Unexpected end of file (2 hex digits per byte).\n");
            goto error;
         }
         if (!isxdigit(highch)) {
            fprintf(stderr, "Unexpected '%c' (non-hex) input.\n", highch);
            goto error;
         }
         if (!isxdigit(lowch)) {
            fprintf(stderr, "Unexpected '%c' (non-hex) input.\n", lowch);
            goto error;
         }
         if (index == datasize[which]) {
            /* Overflow, double the memory. */
            doublechararray(&data[which], &datasize[which]);
            datum = data[which]; 
         }
         datum[index] = (hextoint(highch) << 4) | hextoint(lowch);
         index++;         
      }
   }
   
   /* Done reading. */
   memset(pdbt, 0, sizeof(*pdbt));
   pdbt->size = index;
   pdbt->data = (void*)datum;
   return;
error:
   fprintf(stderr, "Quitting out due to errors.\n");
   exit(EXIT_FAILURE);
}

void read_keys(DB* db, bool plaintext, bool overwritekeys)
{
   int retval;
   size_t length;
   DBT key;
   DBT data;
   int spacech;
   char footer[sizeof("DATA=END\n")];
   

   while (!feof(stdin)) {
      spacech = getchar();
      switch (spacech) {
         case (EOF): {
            /* Done. */
            return;
         }
         case (' '): {
            /* Time to read a key. */
            //TODO: Only some versions require the space here!
            get_dbt(plaintext, &key);
            break;
         }
         case ('D'): {
            ungetc('D', stdin);
            if (
               fgets(footer, sizeof("DATA=END\n"), stdin) != NULL &&
               (
                  !strcmp(footer, "DATA=END") ||
                  !strcmp(footer, "DATA=END\n")
               )
            )
            {
               return;
            }
            goto unexpectedinput;
         }
         default: {
unexpectedinput:
            fprintf(stderr, "Unexpected input while reading key.\n");
            goto error;
         }
      }

      spacech = getchar();
      switch (spacech) {
         case (EOF): {
            fprintf(stderr, "Unexpected end of file while reading value.\n");
            goto error;
         }
         case (' '): {
            /* Time to read a key. */
            //TODO: Only some versions require the space here!
            get_dbt(plaintext, &data);
            break;
         }
         default: {
            fprintf(stderr, "Unexpected input while reading value.\n");
            goto error;
         }
      }
      
      retval = db->put(
         db,
         NULL,
         &key,
         &data,
         overwritekeys ? 0 : DB_NOOVERWRITE
      );
      if (retval != 0) {
         db->err(db, retval, "DB->put");
         printf("TODO: Error condition %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
         goto error;
      }
   }
   return;
error:
   fprintf(stderr, "Quitting out due to errors.\n");
   exit(EXIT_FAILURE);
}

void close_database(DB* db)
{
   db->close(db, 0);
}

void read_header(bool plaintext, DBTYPE* dbtype, bool* dbtype_set, bool* overwritekeys)
{
   printf("TODO: Implement %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
}

void read_footer(bool plaintext)
{
   printf("TODO: Implement %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
}
