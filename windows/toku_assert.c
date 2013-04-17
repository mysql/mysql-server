#include <toku_portability.h>
#include "toku_assert.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

int toku_continue_on_assert_failure=0;

static void toku_assert_failed_but_continue_anyway (void) __attribute__((noinline));

static void
toku_assert_failed_but_continue_anyway (void) {
    printf("Assertion failed, but continuing anyway\n");
}

void toku_do_assert(int expr,const char* expr_as_string,const char *function,const char*file,int line) {
    if (expr==0) {
	fprintf(stderr, "%s:%d %s: Assertion `%s' failed\n", file,line,function,expr_as_string);
	if (!toku_continue_on_assert_failure) {
            fflush(stderr);
#if TOKU_WINDOWS
            //Following commented methods will not always end the process (could hang).
            //They could be unacceptable for other reasons as well (popups,
            //flush buffers before quitting, etc)
            //  abort()
            //  assert(FALSE) (assert.h assert)
            //  raise(SIGABRT)
            //  divide by 0
            //  null dereference
            //  _exit
            //  exit
            //  ExitProcess
            TerminateProcess(GetCurrentProcess(), 134); //Only way found so far to unconditionally
                                                        //Terminate the process
#endif
	    abort();
        }
	else
	    toku_assert_failed_but_continue_anyway();
    }
}
