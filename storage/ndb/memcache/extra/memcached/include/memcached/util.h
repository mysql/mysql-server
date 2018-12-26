/* Modifications copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
*/
#ifndef UTIL_H
#define UTIL_H
/*
 * Wrappers around strtoull/strtoll that are safer and easier to
 * use.  For tests and assumptions, see internal_tests.c.
 *
 * str   a NULL-terminated base decimal 10 unsigned integer
 * out   out parameter, if conversion succeeded
 *
 * returns true if conversion succeeded.
 */
#include <memcached/visibility.h>
#ifdef __cplusplus
extern "C" {
#endif

MEMCACHED_PUBLIC_API bool safe_strtoull(const char *str, uint64_t *out);
MEMCACHED_PUBLIC_API bool safe_strtoll(const char *str, int64_t *out);
MEMCACHED_PUBLIC_API bool safe_strtoul(const char *str, uint32_t *out);
MEMCACHED_PUBLIC_API bool safe_strtol(const char *str, int32_t *out);
MEMCACHED_PUBLIC_API bool safe_strtof(const char *str, float *out);

MEMCACHED_PUBLIC_API extern uint64_t mc_swap64(uint64_t);

#if ! ( defined htonll || defined HAVE_HTONLL )
#define htonll mc_swap64
#define ntohll mc_swap64
#endif

#ifdef __GCC
# define __gcc_attribute__ __attribute__
#else
# define __gcc_attribute__(x)
#endif

/**
 * Vararg variant of perror that makes for more useful error messages
 * when reporting with parameters.
 *
 * @param fmt a printf format
 */
MEMCACHED_PUBLIC_API void vperror(const char *fmt, ...)
    __gcc_attribute__ ((format (printf, 1, 2)));

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
