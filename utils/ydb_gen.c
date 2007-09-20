#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

extern char* optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

typedef uint8_t bool;
#define true   ((bool)1)
#define false  ((bool)0)

int   usage(void);
void  generate_keys(void);

char           dbt_delimiter = '\n';
char*          sort_delimiter = "";
char*          progname;
bool           plaintext = false;
long           minsize = -1;
long           maxsize = -1;
int64_t        maxnumkeys = -1;
long           maxkibibytes = -1;
bool           header = true;
bool           footer = true;
bool           justheader = false;
bool           justfooter = false;
bool           outputkeys = true;
unsigned long  seed = 1;
bool           printableonly = false;
bool           leadingspace = true;

int main (int argc, char *argv[]) {
   char ch;

   progname = argv[0];
   
   while ((ch = getopt(argc, argv, "PfFhHTpr:s:d:p:m:M:n:N:?o:")) != EOF) {
      switch (ch) {
         case ('P'): {
            printableonly = true;
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
         case ('T'): {
            plaintext = true;
            leadingspace = false;
            header = false;
            footer = false;
            break;
         }
         case ('p'): {
            plaintext = true;
            leadingspace = true;
            break;
         }
         case ('o'): {
            if (freopen(optarg, "w", stdout) == NULL) {
               fprintf(stderr,
                       "%s: %s: reopen: %s\n",
                       progname, optarg, strerror(errno));
               return (EXIT_FAILURE);
            }
            break;
         }
         case ('d'): {
            if (strlen(optarg) != 1) {
               fprintf(
                  stderr,
                  "%s: %s: (-n) Key (or value) delimiter must be one character.",
                  progname,
                  optarg
               );
               return (EXIT_FAILURE);
            }
            dbt_delimiter = optarg[0];
            if (isxdigit(dbt_delimiter)) {
               fprintf(
                  stderr,
                  "%s: %c: (-n) Key (or value) delimiter cannot be a hex digit.",
                  progname,
                  dbt_delimiter
               );
               return (EXIT_FAILURE);
            }
            break;
         }
         case ('s'): {
            sort_delimiter = optarg;
            if (strlen(sort_delimiter) != 1) {
               fprintf(
                  stderr,
                  "%s: %s: (-s) Sorting (Between key/value pairs) delimiter must be one character.",
                  progname,
                  optarg
               );
               return (EXIT_FAILURE);
            }
            if (isxdigit(sort_delimiter[0])) {
               fprintf(
                  stderr,
                  "%s: %s: (-s) Sorting (Between key/value pairs) delimiter cannot be a hex digit.",
                  progname,
                  sort_delimiter
               );
               return (EXIT_FAILURE);
            }
            break;
         }
         case ('r'):
         {
            char* test;
            
            seed = strtol(optarg, &test, 10);
            if (
               optarg[0] == '\0' ||
               *test != '\0'
            )
            {              
               fprintf(
                  stderr,
                  "%s: %s: (-r) Random seed invalid.",
                  progname,
                  optarg
               );
            }
            break;
         }
         case ('m'):
         {
            char* test;
            
            if (
               optarg[0] == '\0' ||
               (minsize = strtol(optarg, &test, 10)) < 0 ||
               *test != '\0'
            )
            {              
               fprintf(
                  stderr,
                  "%s: %s: (-m) Min size of keys/values invalid.",
                  progname,
                  optarg
               );
            }
            break;
         }
         case ('M'):
         {
            char* test;
            
            if (
               optarg[0] == '\0' ||
               (maxsize = strtol(optarg, &test, 10)) < 0 ||
               *test != '\0'
            )
            {              
               fprintf(
                  stderr,
                  "%s: %s: (-M) Max size of keys/values invalid.",
                  progname,
                  optarg
               );
            }
            break;
         }
         case ('n'):
         {
            char* test;
            
            if (
               optarg[0] == '\0' ||
               (maxnumkeys = strtoll(optarg, &test, 10)) <= 0 ||
               *test != '\0'
            )
            {              
               fprintf(
                  stderr,
                  "%s: %s: (-n) Max number of keys to generate invalid.",
                  progname,
                  optarg
               );
            }
            break;
         }
         case ('N'):
         {
            char* test;
            
            if (
               optarg[0] == '\0' ||
               (maxkibibytes = strtol(optarg, &test, 10)) <= 0 ||
               *test != '\0'
            )
            {              
               fprintf(
                  stderr,
                  "%s: %s: (-N) Max kibibytes to generate invalid.",
                  progname,
                  optarg
               );
            }
            break;
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
      fprintf(
         stderr,
         "%s: The -h and -H options may not both be specified.\n",
         progname
      );
      usage();
      return (EXIT_FAILURE);
   }
   if (justfooter && !footer) {
      fprintf(
         stderr,
         "%s: The -f and -F options may not both be specified.\n",
         progname
      );
      usage();
      return (EXIT_FAILURE);
   }
   if (justfooter && justheader) {
      fprintf(
         stderr,
         "%s: The -H and -F options may not both be specified.\n",
         progname
      );
      usage();
      return (EXIT_FAILURE);
   }
   if (justfooter && header) {
      fprintf(
         stderr,
         "%s: -F implies -h\n",
         progname
      );
      header = false;
   }
   if (justheader && footer) {
      fprintf(
         stderr,
         "%s: -H implies -f\n",
         progname
      );
      footer = false;
   }
   if (!leadingspace)
   {
      if (footer)
      {
         fprintf(
            stderr,
            "%s: -p implies -f\n",
            progname
         );
         footer = false;
      }
      if (header)
      {
         fprintf(
            stderr,
            "%s: -p implies -h\n",
            progname
         );
         header = false;
      }
   }
   if (justfooter || justheader)
   {
      outputkeys = false;
   }
   else if (
      (maxnumkeys > 0 && maxkibibytes > 0) ||
      (maxnumkeys <= 0 && maxkibibytes <= 0)
   )
   {
      fprintf(
         stderr,
         "%s: exactly one of the -n and -N options must be specified.\n",
         progname
      );
      usage();
      return (EXIT_FAILURE);
   }
   if (outputkeys && seed == 1)
   {
      fprintf(
         stderr,
         "%s: Using default seed.  (-r 1).\n",
         progname
      );
      seed = 1;
   }
   if (outputkeys && minsize == -1) {
      fprintf(
         stderr,
         "%s: Using default minsize.  (-m 0).\n",
         progname
      );
      minsize = 0;
   }
   if (outputkeys && maxsize == -1) {
      fprintf(
         stderr,
         "%s: Using default maxsize.  (-M 1024).\n",
         progname
      );
      maxsize = 1024;
   }
   if (outputkeys && minsize > maxsize) {
      fprintf(
         stderr,
         "%s: Max key size must be greater than min key size.\n",
         progname
      );
      usage();
      return (EXIT_FAILURE);
   }

   if (argc != 0) {   
      return (usage());
   }
   if (header)
   {
      printf(
         "VERSION=3\n"
         "format=%s\n"
         "type=btree\n"
         "db_pagesize=4096\n"
         "HEADER=END\n",
         (
            plaintext ?
            "print" :
            "bytevalue"
         )
      );
   }
   if (justheader)
   {
      return 0;
   }
   if (outputkeys)
   {
      /* Generate Keys! */
      generate_keys();
   }
   if (footer)
   {
      printf("DATA=END\n");
   }
   return 0;
}

int usage()
{
   fprintf
   (
      stderr,
      "usage: %s [-ThHfF] [-d delimiter] [-s delimiter]\n"
      "       -m minsize -M maxsize [-r random seed]\n"
      "       (-n maxnumkeys | -N maxkibibytes) [-o filename]\n",
      progname
   );
   return 1;
}

unsigned char randbyte()
{
   static int numsavedbits = 0;
   static unsigned long long savedbits = 0;
   unsigned char retval;
   
   if (numsavedbits < 8)
   {
      savedbits |= ((unsigned long long)random()) << numsavedbits;
      numsavedbits += 31;  /* Random generates 31 random bits. */
   }
   retval = savedbits & 0xff;
   numsavedbits -= 8;
   savedbits >>= 8;
   return retval;
}

/* Almost-uniformly random int from [0,max) */
int random_below(int max)
{
   assert(max > 0);
   return (random() % max);
}

void outputbyte(unsigned char ch)
{
   if (plaintext) {
      if (ch == '\\')         printf("\\\\");
      else if (isprint(ch))   printf("%c", ch);
      else                    printf("\\%02x", ch);
   }
   else printf("%02x", ch);
}

void outputstring(char* str)
{
   char* p;

   for (p = str; *p != '\0'; p++)
   {
      outputbyte((unsigned char)*p);
   }
}

void generate_keys()
{
   bool usedemptykey = false;
   long long numgenerated = 0;
   long long totalsize = 0;
   char identifier[24]; /* 8 bytes * 2 = 16; 16+1=17; 17+null terminator = 18. Extra padding. */
   int length;
   int i;

   srandom(seed);
   while (
      (
         maxnumkeys == -1 ||
         numgenerated < maxnumkeys  
      ) &&
      (
         maxkibibytes == -1 ||
         totalsize >> 10 < maxkibibytes
      )
   )
   {
      numgenerated++;
      
      /* Generate a key. */
      if (leadingspace) {
         printf(" ");   /* Each key is preceded by a space. */
      }
      {
         /* Pick a key length. */
         length = random_below(maxsize - minsize + 1) + minsize;
         
         /* Output 'length' random bytes. */
         for (i = 0; i < length; i++)
         {
            unsigned char ch;
            
            do {
               ch = randbyte();
            }
               while (printableonly && !isprint(ch));
            
            outputbyte(ch);
         }
         totalsize += length;
         if (length == 0 && !usedemptykey)
         {
            usedemptykey = true;
         }
         else
         {
            /* Append identifier to ensure uniqueness. */
            sprintf(identifier, "x%llx", numgenerated);
            outputstring(identifier);
            totalsize += strlen(identifier);
         }
      }
      printf("%c", dbt_delimiter);

      /* Generate a value. */
      if (leadingspace) {
         printf(" ");   /* Each value is preceded by a space. */
      }
      {
         /* Pick a key length. */
         length = random_range(minsize, maxsize);
         
         /* Output 'length' random bytes. */
         for (i = 0; i < length; i++)
         {
            unsigned char ch;
            
            do {
               ch = randbyte();
            }
               while (printableonly && !isprint(ch));
            
            outputbyte(ch);
         }
         totalsize += length;
      }
      printf("%c", dbt_delimiter);

      printf("%s", sort_delimiter);
   }
}
