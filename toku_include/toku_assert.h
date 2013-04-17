#ifndef TOKU_ASSERT_H
#define TOKU_ASSERT_H
/* The problem with assert.h:  If NDEBUG is set then it doesn't execute the function, if NDEBUG isn't set then we get a branch that isn't taken. */
/* This version will complain if NDEBUG is set. */
/* It evaluates the argument and then calls a function  toku_do_assert() which takes all the hits for the branches not taken. */

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif


#ifdef NDEBUG
#error NDEBUG should not be set
#endif

void toku_do_assert_fail(const char*/*expr_as_string*/,const char */*fun*/,const char*/*file*/,int/*line*/) __attribute__((__visibility__("default"))) __attribute__((__noreturn__));
void toku_do_assert(int,const char*/*expr_as_string*/,const char */*fun*/,const char*/*file*/,int/*line*/) __attribute__((__visibility__("default")));

// Define GCOV if you want to get test-coverage information that ignores the assert statements.
// #define GCOV

#if defined(GCOV) || TOKU_WINDOWS
#define assert(expr) toku_do_assert((expr) != 0, #expr, __FUNCTION__, __FILE__, __LINE__)
#else
#define assert(expr) ((expr) ? (void)0 : toku_do_assert_fail(#expr, __FUNCTION__, __FILE__, __LINE__))
#endif

#ifdef GCOV
#define WHEN_GCOV(x) x
#define WHEN_NOT_GCOV(x)
#else
#define WHEN_GCOV(x)
#define WHEN_NOT_GCOV(x) x
#endif

#define lazy_assert(a)     assert(a) // indicates code is incomplete 
#define invariant(a)       assert(a) // indicates a code invariant that must be true
#define resource_assert(a) assert(a) // indicates resource must be available, otherwise unrecoverable

#if defined(__cplusplus) || defined(__cilkplusplus)
}
#endif

#endif
