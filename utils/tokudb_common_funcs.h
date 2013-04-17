#if !defined(TOKUDB_COMMON_FUNCS_H)
#define TOKUDB_COMMON_FUNCS_H

/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "tokudb_common.h"

//DB_ENV->err disabled since it does not use db_strerror
#define PRINT_ERROR(retval, ...)                                        \
if (0) g.dbenv->err(g.dbenv, retval, __VA_ARGS__);                \
else {                                                            \
   fprintf(stderr, "%s: %s:", g.progname, db_strerror(retval));   \
   fprintf(stderr, __VA_ARGS__);                                  \
   fprintf(stderr, "\n");                                         \
}

//DB_ENV->err disabled since it does not use db_strerror, errx does not exist.
#define PRINT_ERRORX(...)                                               \
if (0) g.dbenv->err(g.dbenv, 0, __VA_ARGS__);                     \
else {                                                            \
   fprintf(stderr, "%s: ", g.progname);                           \
   fprintf(stderr, __VA_ARGS__);                                  \
   fprintf(stderr, "\n");                                         \
}

int   strtoint32  (char* str,  int32_t* num,  int32_t min,  int32_t max, int base);
int   strtouint32 (char* str, u_int32_t* num, u_int32_t min, u_int32_t max, int base);
int   strtoint64  (char* str,  int64_t* num,  int64_t min,  int64_t max, int base);
int   strtouint64 (char* str, u_int64_t* num, u_int64_t min, u_int64_t max, int base);

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
DEF_STR_TO(strtouint32, u_int32_t, u_int64_t, strtoull, PRIu32)
DEF_STR_TO(strtoint64,  int64_t,  int64_t,  strtoll,  PRId64)
DEF_STR_TO(strtouint64, u_int64_t, u_int64_t, strtoull, PRIu64)

static inline void
outputbyte(u_int8_t ch)
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
      outputbyte((u_int8_t)*p);
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
verify_library_version()
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
#if IS_TDB && !defined(_WIN32) && !defined(_WIN64) && TDB_IS_STATIC
#include <ydb.h>
#endif
static int test_main (int argc, char *argv[]);
int
main(int argc, char *argv[]) {
    int r;
#if IS_TDB && !defined(_WIN32) && !defined(_WIN64) && TDB_IS_STATIC
    toku_ydb_init();
#endif
#if !IS_TDB && DB_VERSION_MINOR==4 && DB_VERSION_MINOR == 7
    r = db_env_set_func_malloc(toku_malloc);   assert(r==0);
    r = db_env_set_func_free(toku_free);      assert(r==0);
    r = db_env_set_func_realloc(toku_realloc);   assert(r==0);
#endif
    r = test_main(argc, argv);
#if IS_TDB && !defined(_WIN32) && !defined(_WIN64) && TDB_IS_STATIC
    toku_ydb_destroy();
#endif
    return r;
}

#endif /* #if !defined(TOKUDB_COMMON_H) */
