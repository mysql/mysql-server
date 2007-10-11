#if !defined(TOKUDB_COMMON_H)

#define TOKUDB_COMMON_H
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <db.h>

typedef uint8_t bool;

#define true ((bool)1)
#define false ((bool)0)

#define SET_BITS(bitvector, bits)      ((bitvector) |= (bits))
#define REMOVE_BITS(bitvector, bits)   ((bitvector) &= ~(bits))
#define IS_SET_ANY(bitvector, bits)    ((bitvector) & (bits))
#define IS_SET_ALL(bitvector, bits)    (((bitvector) & (bits)) == (bits))

#define IS_POWER_OF_2(num)             ((num) > 0 && ((num) & ((num) - 1)) == 0)

int   strtoint32  (DB_ENV* dbenv, char* progname, char* str,  int32_t* num,  int32_t min,  int32_t max, int base);
int   strtouint32 (DB_ENV* dbenv, char* progname, char* str, uint32_t* num, uint32_t min, uint32_t max, int base);
int   strtoint64  (DB_ENV* dbenv, char* progname, char* str,  int64_t* num,  int64_t min,  int64_t max, int base);
int   strtouint64 (DB_ENV* dbenv, char* progname, char* str, uint64_t* num, uint64_t min, uint64_t max, int base);

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
int name(DB_ENV* dbenv, char* progname, char* str, type* num, type min, type max, int base)   \
{                                                              \
   char* test;                                                 \
   bigtype value;                                              \
                                                               \
   assert(str);                                                \
   assert(num);                                                \
   assert(min <= max);                                         \
   assert(dbenv || progname);                                  \
   assert(base == 0 || (base >= 2 && base <= 36));             \
                                                               \
   errno = 0;                                                  \
   while (isspace(*str)) str++;                                \
   value = strtofunc(str, &test, base);                        \
   if ((*test != '\0' && *test != '\n') || test == str) {      \
      if (dbenv == NULL) fprintf(stderr, "%s: %s: Invalid numeric argument\n", progname, str);  \
      else dbenv->err(dbenv, 0, "%s: Invalid numeric argument", str);  \
      errno = EINVAL;                                          \
      goto error;                                              \
   }                                                           \
   if (errno != 0) {                                           \
      if (dbenv == NULL) fprintf(stderr, "%s: %s: %s\n", progname, str, strerror(errno)); \
      else dbenv->err(dbenv, errno, "%s", str);                \
      goto error;                                              \
   }                                                           \
   if (value < min) {                                          \
      if (dbenv == NULL) fprintf(stderr, "%s: %s: Less than minimum value (%" frmt ")\n", progname, str, min); \
      else dbenv->err(dbenv, 0, "%s: Less than minimum value (%" frmt ")", str, min); \
      goto error;                                              \
   }                                                           \
   if (value > max) {                                          \
      if (dbenv == NULL) fprintf(stderr, "%s: %s: Greater than maximum value (%" frmt ")\n", progname, str, max); \
      else dbenv->err(dbenv, 0, "%s: Greater than maximum value (%" frmt ")", str, max); \
      goto error;                                              \
   }                                                           \
   *num = value;                                               \
   return EXIT_SUCCESS;                                        \
error:                                                         \
   return errno;                                               \
}

DEF_STR_TO(strtoint32,  int32_t, int64_t, strtoll, "d");
DEF_STR_TO(strtouint32, uint32_t, uint64_t, strtoull, "u");
DEF_STR_TO(strtoint64,  int64_t, int64_t, strtoll, "lld");
DEF_STR_TO(strtouint64,  uint64_t, uint64_t, strtoull, "llu");

#endif /* #if !defined(TOKUDB_COMMON_H) */
