#include "toku_assert.h"
#include <stdio.h>
#include <stdlib.h>

void toku_do_assert(int expr,const char* expr_as_string,const char *function,const char*file,int line) {
    if (expr==0) {
	fprintf(stderr, "%s:%d %s: Assertion `%s' failed\n", file,line,function,expr_as_string);
	abort();
    }
}
