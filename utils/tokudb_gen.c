/* -*- mode: C; c-basic-offset: 3 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

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
   DB_ENV*       dbenv;
   bool        plaintext;
   char*       progname;
} gen_globals;

gen_globals g;
#include "tokudb_common_funcs.h"

int   usage(void);
void  generate_keys(void);
int   get_delimiter(char* str);



char           dbt_delimiter  = '\n';
char           sort_delimiter[2];
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


int main (int argc, char *argv[]) {
   int ch;

   /* Set up the globals. */
   memset(&g, 0, sizeof(g));

   g.progname = argv[0];

   if (verify_library_version() != 0) goto error;
   
   strcpy(sort_delimiter, "");

   while ((ch = getopt(argc, argv, "PpTo:r:m:M:n:uVhHfFd:s:")) != EOF) {
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
               ERROR(errno, "%s: reopen\n", optarg);
               goto error;
            }
            break;
         }
         case ('r'): {
            if (strtouint32(optarg, &seed, 0, UINT32_MAX, 10)) {
               ERRORX("%s: (-r) Random seed invalid.", optarg);
                goto error;
            }
            set_seed = true;
            break;
         }
         case ('m'): {
            if (strtouint32(optarg, &lengthmin, 0, UINT32_MAX, 10)) {
               ERRORX("%s: (-m) Min length of keys/values invalid.", optarg);
                goto error;
            }
            set_lengthmin = true;
            break;
         }
         case ('M'): {
            if (strtouint32(optarg, &lengthlimit, 1, UINT32_MAX, 10)) {
               ERRORX("%s: (-M) Limit of key/value length invalid.", optarg);
                goto error;
            }
            set_lengthlimit = true;
            break;
         }
         case ('n'): {
            if (strtouint64(optarg, &numkeys, 0, UINT64_MAX, 10)) {
               ERRORX("%s: (-n) Number of keys to generate invalid.", optarg);
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
               ERRORX("%s: (-d) Key (or value) delimiter must be one character.",
                      optarg);
               goto error;
            }
            if (isxdigit(temp)) {
               ERRORX("%c: (-d) Key (or value) delimiter cannot be a hex digit.",
                      temp);
               goto error;
            }
            dbt_delimiter = (char)temp;
            break;
         }
         case ('s'): {
            int temp = get_delimiter(optarg);
            if (temp == EOF) {
               ERRORX("%s: (-s) Sorting (Between key/value pairs) delimiter must be one character.",
                      optarg);
               goto error;
            }
            if (isxdigit(temp)) {
               ERRORX("%c: (-s) Sorting (Between key/value pairs) delimiter cannot be a hex digit.",
                      temp);
               goto error;
            }
            sort_delimiter[0] = (char)temp;
            sort_delimiter[1] = '\0';
            break;
         }
         case ('V'): {
            printf("%s\n", db_version(NULL, NULL, NULL));
            return EXIT_SUCCESS;
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
      ERRORX("The -h and -H options may not both be specified.\n");
      goto error;
   }
   if (justfooter && !footer) {
      ERRORX("The -f and -F options may not both be specified.\n");
      goto error;
   }
   if (justfooter && justheader) {
      ERRORX("The -H and -F options may not both be specified.\n");
      goto error;
   }
   if (justfooter && header) {
      ERRORX("-F implies -h\n");
      header = false;
   }
   if (justheader && footer) {
      ERRORX("-H implies -f\n");
      footer = false;
   }
   if (!leadingspace) {
      if (footer) {
         ERRORX("-p implies -f\n");
         footer = false;
      }
      if (header) {
         ERRORX("-p implies -h\n");
         header = false;
      }
   }
   if (justfooter || justheader) outputkeys = false;
   else if (!set_numkeys)
   {
      ERRORX("Using default number of keys.  (-n 1024).\n");
      numkeys = 1024;
   }
   if (outputkeys && !set_seed) {
      ERRORX("Using default seed.  (-r 1).\n");
      seed = 1;
   }
   if (outputkeys && !set_lengthmin) {
      ERRORX("Using default lengthmin.  (-m 0).\n");
      lengthmin = 0;
   }
   if (outputkeys && !set_lengthlimit) {
      ERRORX("Using default lengthlimit.  (-M 1024).\n");
      lengthlimit = 1024;
   }
   if (outputkeys && lengthmin >= lengthlimit) {
      ERRORX("Max key size must be greater than min key size.\n");
      goto error;
   }

   if (argc != 0) {
      return usage();
   }
   if (header) {
      printf("VERSION=3\n"
             "format=%s\n"
             "type=btree\n"
             //"db_pagesize=4096\n"  //Don't write pagesize which would be useless.
             "HEADER=END\n",
             g.plaintext ? "print" : "bytevalue");
   }
   if (outputkeys) generate_keys();
   if (footer)     printf("DATA=END\n");
   return EXIT_SUCCESS;

error:
   fprintf(stderr, "Quitting out due to errors.\n");
   return EXIT_FAILURE;
}

int usage()
{
   fprintf(stderr,
           "usage: %s [-PpTuVhHfF] [-o output] [-r seed] [-m minsize] [-M limitsize]\n"
           "       %*s[-n numpairs] [-d delimiter] [-s delimiter]\n",
           g.progname, strlen(g.progname) + 1, "");
   return EXIT_FAILURE;
}

uint8_t randbyte()
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
int32_t random_below(int32_t limit)
{
   assert(limit > 0);
   return random() % limit;
}

void generate_keys()
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
                sprintf(identifier, "x%llx", numgenerated);
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
         case ('e'): return '\e';
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
