#include "toku_assert.h"
#include <stdio.h>
#include <stdlib.h>

int toku_continue_on_assert_failure=0;

static void toku_assert_failed_but_continue_anyway (void) __attribute__((noinline));

static void
toku_assert_failed_but_continue_anyway (void) {
    printf("Assertion failed, but continuing anyway\n");
}

void toku_do_assert(int expr,const char* expr_as_string,const char *function,const char*file,int line) {
    if (expr==0) {
	fprintf(stderr, "%s:%d %s: Assertion `%s' failed\n", file,line,function,expr_as_string);
	if (!toku_continue_on_assert_failure)
	    abort();
	else
	    toku_assert_failed_but_continue_anyway();
    }
}
