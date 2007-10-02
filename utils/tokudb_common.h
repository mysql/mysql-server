#if !defined(TOKUDB_COMMON_H)

#define TOKUDB_COMMON_H
#include <limits.h>

int   strtoint32(char* str, int32_t* num, int32_t min, int32_t max, int base);
int   strtouint32(char* str, uint32_t* num, uint32_t min, uint32_t max, int base);
int   strtoint64(char* str, int64_t* num, int64_t min, int64_t max, int base);
int   strtouint64(char* str, uint64_t* num, uint64_t min, uint64_t max, int base);

/*
 * Convert a string to an "type".  Uses base 10.
 * Allow range of [min, max].
 * 
 *
 * Sets errno and returns:
 *    EINVAL: str == NULL, num == NULL, or string not of the form [ ]+[+-]?[0-9]+
 *    ERANGE: value out of range specified.
 *
 * *num is unchanged on error.
 */
#define DEF_STR_TO(name, type, bigtype, strtofunc)             \
int name(char* str, type* num, type min, type max, int base)   \
{                                                              \
   char* test;                                                 \
   bigtype value;                                              \
                                                               \
   assert(str);                                                \
   assert(num);                                                \
   assert(min <= max);                                         \
   if (!str || *str == '\0' || !num) {                         \
      errno = EINVAL;                                          \
      goto err;                                                \
   }                                                           \
   errno = 0;                                                  \
                                                               \
   value = strtofunc(str, &test, base);                        \
   if (errno) goto err;                                        \
   if (*test != '\0') {                                        \
      errno = EINVAL;                                          \
      goto err;                                                \
   }                                                           \
   if (value < min || value > max) {                           \
      errno = ERANGE;                                          \
      goto err;                                                \
   }                                                           \
   *num = value;                                               \
   return 0;                                                   \
                                                               \
err:                                                           \
   return errno;                                               \
}

DEF_STR_TO(strtoint32,  int32_t, int64_t, strtoll);
DEF_STR_TO(strtouint32, uint32_t, uint64_t, strtoull);
DEF_STR_TO(strtoint64,  int64_t, int64_t, strtoll);
DEF_STR_TO(strtouint64,  uint64_t, uint64_t, strtoull);

#endif /* #if !defined(TOKUDB_COMMON_H) */
