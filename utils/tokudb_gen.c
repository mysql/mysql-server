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
int   get_delimiter(char* str);

char           dbt_delimiter  = '\n';
char           sort_delimiter[2];
char*          progname;
bool           plaintext      = false;
long           lengthmin      = -1;
long           lengthlimit    = -1;
int64_t        numkeys        = -1;
bool           header         = true;
bool           footer         = true;
bool           justheader     = false;
bool           justfooter     = false;
bool           outputkeys     = true;
unsigned long  seed           = 1;
bool           printableonly  = false;
bool           leadingspace   = true;

int main (int argc, char *argv[]) {
   char ch;

   progname = argv[0];
   strcpy(sort_delimiter, "");
   
   while ((ch = getopt(argc, argv, "PfFhHTpr:s:d:p:m:M:n:o:")) != EOF) {
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
            plaintext      = true;
            leadingspace   = false;
            header         = false;
            footer         = false;
            break;
         }
         case ('p'): {
            plaintext      = true;
            leadingspace   = true;
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
            int temp = get_delimiter(optarg);
            if (temp == EOF) {
               fprintf(stderr,
                       "%s: %s: (-d) Key (or value) delimiter must be one character.",
                       progname, optarg);
               return (EXIT_FAILURE);
            }
            if (isxdigit(temp)) {
               fprintf(stderr,
                       "%s: %c: (-d) Key (or value) delimiter cannot be a hex digit.",
                       progname, temp);
               return (EXIT_FAILURE);
            }
            dbt_delimiter = (char)temp;
            break;
         }
         case ('s'): {
            int temp = get_delimiter(optarg);
            if (temp == EOF) {
               fprintf(stderr,
                       "%s: %s: (-s) Sorting (Between key/value pairs) delimiter must be one character.",
                       progname, optarg);
               return (EXIT_FAILURE);
            }
            if (isxdigit(temp)) {
               fprintf(stderr,
                       "%s: %c: (-s) Sorting (Between key/value pairs) delimiter cannot be a hex digit.",
                       progname, temp);
               return (EXIT_FAILURE);
            }
            sort_delimiter[0] = (char)temp;
            sort_delimiter[1] = '\0';
            break;
         }
         case ('r'): {
            char* test;
            
            seed = strtol(optarg, &test, 10);
            if (optarg[0] == '\0' || *test != '\0') {              
               fprintf(stderr,
                       "%s: %s: (-r) Random seed invalid.",
                       progname, optarg);
            }
            break;
         }
         case ('m'): {
            char* test;
            
            if (optarg[0] == '\0' ||
               (lengthmin = strtol(optarg, &test, 10)) < 0 ||
               *test != '\0')
            {              
               fprintf(stderr,
                       "%s: %s: (-m) Min length of keys/values invalid.",
                       progname, optarg);
            }
            break;
         }
         case ('M'): {
            char* test;
            
            if (optarg[0] == '\0' ||
               (lengthlimit = strtol(optarg, &test, 10)) < 0 ||
               *test != '\0')
            {              
               fprintf(stderr,
                       "%s: %s: (-M) Limit of key/value length invalid.",
                       progname, optarg);
            }
            break;
         }
         case ('n'): {
            char* test;
            
            if (optarg[0] == '\0' ||
               (numkeys = strtoll(optarg, &test, 10)) <= 0 ||
               *test != '\0')
            {              
               fprintf(stderr,
                       "%s: %s: (-n) Number of keys to generate invalid.",
                       progname, optarg);
            }
            break;
         }
         default: {
            return (usage());
         }
      }
   }
   argc -= optind;
   argv += optind;
   
   if (justheader && !header) {
      fprintf(stderr,
              "%s: The -h and -H options may not both be specified.\n",
              progname);
      return usage();
   }
   if (justfooter && !footer) {
      fprintf(stderr,
              "%s: The -f and -F options may not both be specified.\n",
              progname);
      return usage();
   }
   if (justfooter && justheader) {
      fprintf(stderr,
              "%s: The -H and -F options may not both be specified.\n",
              progname);
      return usage();
   }
   if (justfooter && header) {
      fprintf(stderr,
              "%s: -F implies -h\n",
              progname);
      header = false;
   }
   if (justheader && footer) {
      fprintf(stderr,
              "%s: -H implies -f\n",
              progname);
      footer = false;
   }
   if (!leadingspace) {
      if (footer) {
         fprintf(stderr,
                 "%s: -p implies -f\n",
                 progname);
         footer = false;
      }
      if (header) {
         fprintf(stderr,
                 "%s: -p implies -h\n",
                 progname);
         header = false;
      }
   }
   if (justfooter || justheader) outputkeys = false;
   else if (numkeys == -1)
   {
      fprintf(stderr,
              "%s: The -n option is required.\n",
              progname);
      return usage();
   }
   if (outputkeys && seed == 1) {
      fprintf(stderr,
              "%s: Using default seed.  (-r 1).\n",
              progname);
      seed = 1;
   }
   if (outputkeys && lengthmin == -1) {
      fprintf(stderr,
              "%s: Using default lengthmin.  (-m 0).\n",
              progname);
      lengthmin = 0;
   }
   if (outputkeys && lengthlimit == -1) {
      fprintf(stderr,
              "%s: Using default lengthlimit.  (-M 1024).\n",
              progname);
      lengthlimit = 1024;
   }
   if (outputkeys && lengthmin >= lengthlimit) {
      fprintf(stderr,
              "%s: Max key size must be greater than min key size.\n",
              progname);
      return usage();
   }

   if (argc != 0) {   
      return usage();
   }
   if (header) {
      printf("VERSION=3\n"
             "format=%s\n"
             "type=btree\n"
             "db_pagesize=4096\n"
             "HEADER=END\n",
             plaintext ? "print" : "bytevalue");
   }
   if (justheader) return 0;
   if (outputkeys) generate_keys();
   if (footer)     printf("DATA=END\n");
   return 0;
}

int usage()
{
   fprintf(stderr,
           "usage: %s [-ThHfF] [-d delimiter] [-s delimiter]\n"
           "       [-m lengthmin] [-M lengthlimit] [-r random seed]\n"
           "       [-o filename] -n numkeys\n",
           progname);
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

/* Almost-uniformly random int from [0,max) */
int32_t random_below(int32_t max)
{
   assert(max > 0);
   return random() % max;
}

void outputbyte(uint8_t ch)
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

   for (p = str; *p != '\0'; p++) {
      outputbyte((uint8_t)*p);
   }
}

void generate_keys()
{
   bool     usedemptykey   = false;
   int64_t  numgenerated   = 0;
   int64_t  totalsize      = 0;
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
         if (length == 0 && !usedemptykey) usedemptykey = true;
         else {
            /* Append identifier to ensure uniqueness. */
            sprintf(identifier, "x%llx", numgenerated);
            outputstring(identifier);
            totalsize += strlen(identifier);
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
