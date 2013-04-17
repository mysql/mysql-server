#ifndef TOKU_ASSERT_H
#define TOKU_ASSERT_H

/* The purpose of this file is to define assert() for use by the handlerton.
 * The intention is for a failed handlerton assert to invoke a failed assert
 * in the fractal tree layer, which dumps engine status to the error log.
 */

void toku_hton_assert_fail(const char*/*expr_as_string*/,const char */*fun*/,const char*/*file*/,int/*line*/, int/*errno*/) __attribute__((__visibility__("default"))) __attribute__((__noreturn__));

#define assert(expr)      ((expr)      ? (void)0 : toku_hton_assert_fail(#expr, __FUNCTION__, __FILE__, __LINE__, errno))

#endif
