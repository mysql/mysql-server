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
void  generate_keys  args( (
   char dbt_delimiter,
   char* sort_delimiter,
   const char* progname,
   bool plaintext,
   long minsize,
   long maxsize,
   long long maxnumkeys,
   long maxkibibytes,
   unsigned long seed,
   bool printableonly
) );

int main (int argc, char *argv[]) {
   char ch;
   char dbt_delimiter = '\n';
   char* sort_delimiter = "";
   const char* progname = argv[0];
   bool plaintext = false;
   long minsize = -1;
   long maxsize = -1;
   long long maxnumkeys = -1;
   long maxkibibytes = -1;
   bool header = true;
   bool footer = true;
   bool justheader = false;
   bool justfooter = false;
   bool outputkeys = true;
   unsigned long seed = 1;
   bool printableonly = false;

   while ((ch = getopt(argc, argv, "PfFhHTr:s:d:p:m:M:n:N:?o:")) != EOF) {
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
            break;
         }
         case ('o'): {
            if (freopen(optarg, "w", stdout) == NULL)
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
            return (usage(progname));
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
      usage(progname);
      return (EXIT_FAILURE);
   }
   if (justfooter && !footer) {
      fprintf(
         stderr,
         "%s: The -f and -F options may not both be specified.\n",
         progname
      );
      usage(progname);
      return (EXIT_FAILURE);
   }
   if (justfooter && justheader) {
      fprintf(
         stderr,
         "%s: The -H and -F options may not both be specified.\n",
         progname
      );
      usage(progname);
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
   if (plaintext)
   {
      if (footer)
      {
         fprintf(
            stderr,
            "%s: -T implies -f\n",
            progname
         );
         footer = false;
      }
      if (header)
      {
         fprintf(
            stderr,
            "%s: -T implies -h\n",
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
      usage(progname);
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
      usage(progname);
      return (EXIT_FAILURE);
   }

   if (argc != 0) {   
      return (usage(progname));
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
      generate_keys(
         dbt_delimiter,
         sort_delimiter,
         progname,
         plaintext,
         minsize,
         maxsize,
         maxnumkeys,
         maxkibibytes,
         seed,
         printableonly
      );
   }
   if (footer)
   {
      printf("DATA=END\n");
   }
   return 0;
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

/* Uniformly random int from [min,max] */
int random_range(int min, int max)
{
   int power;
   int number;
   int choices;

   if (min == 0 && max == 0) {
      return 0;
   }

   choices = max - min + 1;
   if (choices < 2)
   {
      return min;
   }

   for (power = 2; power < choices; power <<= 1)
   {
   }

   do
   {
      number = random() & (power - 1);
   }
      while (number >= choices);
   
   return min + number;
}

void outputbyte(unsigned char ch, bool plaintext)
{
   if (plaintext) {
      if (ch != '\n' && isprint(ch)) {
         switch (ch) {
            case ('\\'): {
               printf("\\\\");
               break;
            }
            default:
            {
               printf("%c", ch);
               break;
            }
         }
      }
      else {
         printf(
            "\\%c%c",
            "0123456789abcdef"[(ch & 0xf0) >> 4],
            "0123456789abcdef"[ch & 0x0f]
         );
      }
   }
   else {
      printf(
         "%c%c",
         "0123456789abcdef"[(ch & 0xf0) >> 4],
         "0123456789abcdef"[ch & 0x0f]
      );
   }
}

void outputstring(char* str, bool plaintext)
{
   char* p;

   for (p = str; *p != '\0'; p++)
   {
      outputbyte((unsigned char)*p, plaintext);
   }
}

void generate_keys(
   char dbt_delimiter,
   char* sort_delimiter,
   const char* progname,
   bool plaintext,
   long minsize,
   long maxsize,
   long long maxnumkeys,
   long maxkibibytes,
   unsigned long seed,
   bool printableonly
)
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
      if (!plaintext) {
         printf(" ");   /* Each key is preceded by a space. */
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
            
            outputbyte(ch, plaintext);
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
            outputstring(identifier, plaintext);
            totalsize += strlen(identifier);
         }
      }
      printf("%c", dbt_delimiter);

      /* Generate a value. */
      if (!plaintext) {
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
            
            outputbyte(ch, plaintext);
         }
         totalsize += length;
      }
      printf("%c", dbt_delimiter);

      printf("%s", sort_delimiter);
   }
}
